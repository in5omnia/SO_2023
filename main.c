#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "constants.h"
#include "linkedList.h"
#include "main.h"
#include "operations.h"
#include "parser.h"

static list_t *file_list = NULL;

// ./ems <dir> <max jobs> <max threads> [delay]
int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc < NUM_MANDATORY_ARGS) {
    fprintf(stderr, "Invalid number of arguments\n");
    return 1;
  }

  if (argc > NUM_MANDATORY_ARGS) {
    char *endptr;
    unsigned long int delay = strtoul(argv[DELAY_ARG_INDEX], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  char *dirpath = argv[DIR_ARG_INDEX];
  file_list = create_linkedList();
  int ok = traverse_dir(dirpath, file_list);
  if (ok) {
    fprintf(stderr, "Failed to traverse directory\n");
    return 1;
  }

  char *endptr;
  int max_procs = (int)strtoul(argv[MAX_PROCS_ARG_INDEX], &endptr, 10);
  // Start child processes to execute the jobs up to MAX PROCS
  while (file_list->size > 0) {
    char *filepath = pop_linkedList(file_list);
    if (filepath == NULL) {
      fprintf(stderr, "Failed to pop filepath from list\n");
      return 1;
    }

    if (max_procs == 0) {
      wait(NULL);
      max_procs++;
    }

    max_procs--;
    pid_t pid = fork();
    if (pid == 0) {
      if (ems_init(state_access_delay_ms)) {
        fprintf(stderr, "Failed to initialize EMS\n");
        exit(1);
      }

      if (exec_file(open(filepath, O_RDONLY))) {
        exit(1);
      };
      if (ems_terminate()) {
        exit(1);
      }
      exit(0);
    }

    free(filepath);
  }

  // wait for all child processes to finish
  while (wait(NULL) > 0)
    ;
  free_linkedList(file_list);

  return 0;
}

int traverse_dir(char *dirpath, list_t *fileList) {
  DIR *dir = opendir(dirpath);

  if (dir == NULL) {
    fprintf(stderr, "Directory %s does not exists\n", dirpath);
    return 1;
  }

  struct dirent *entry;
  entry = readdir(dir);
  while (entry) {
    // read all files in the directory
    if (entry->d_type == DT_REG) {
      // check if file is .job terminated
      char *filename = entry->d_name;
      int filename_len = (int)strlen(filename);

      if (filename_len > JOB_FILE_EXTENSION_LEN &&
          strcmp(filename + filename_len - JOB_FILE_EXTENSION_LEN,
                 JOB_FILE_EXTENSION) == 0) {

        char *filepath = malloc(strlen(dirpath) + strlen(entry->d_name) + 2);
        if (filepath == NULL) {
          fprintf(stderr, "Error allocating memory for filepath\n");
          closedir(dir);
          return 1;
        }
        sprintf(filepath, "%s/%s", dirpath, entry->d_name);

        if (append_to_linkedList(fileList, filepath)) {
          fprintf(stderr, "Failed to append file %s\n", filepath);
          closedir(dir);
          return 1;
        }

        free(filepath);
      }
    }

    entry = readdir(dir);
  }
  closedir(dir);
  return 0;
}

int exec_file(int fd) {
  unsigned int event_id, delay;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

  switch (get_next(fd)) {
  case CMD_CREATE:
    if (parse_create(fd, &event_id, &num_rows, &num_columns) != 0) {
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      return 1;
    }

    if (ems_create(event_id, num_rows, num_columns)) {
      fprintf(stderr, "Failed to create event\n");
    }

    break;

  case CMD_RESERVE:
    num_coords = parse_reserve(fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

    if (num_coords == 0) {
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      return 1;
    }

    if (ems_reserve(event_id, num_coords, xs, ys)) {
      fprintf(stderr, "Failed to reserve seats\n");
    }

    break;

  case CMD_SHOW:
    if (parse_show(STDIN_FILENO, &event_id) != 0) {
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      return 1;
    }

    if (ems_show(event_id)) {
      fprintf(stderr, "Failed to show event\n");
    }

    break;

  case CMD_LIST_EVENTS:
    if (ems_list_events()) {
      fprintf(stderr, "Failed to list events\n");
    }

    break;

  case CMD_WAIT:
    if (parse_wait(STDIN_FILENO, &delay, NULL) ==
        -1) { // thread_id is not implemented
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      return 1;
    }

    if (delay > 0) {
      printf("Waiting...\n");
      ems_wait(delay);
    }

    break;

  case CMD_INVALID:
    fprintf(stderr, "Invalid command. See HELP for usage\n");
    return 1;

  case CMD_HELP:
    printf("Available commands:\n"
           "  CREATE <event_id> <num_rows> <num_columns>\n"
           "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
           "  SHOW <event_id>\n"
           "  LIST\n"
           "  WAIT <delay_ms> [thread_id]\n" // thread_id is not
                                             // implemented
           "  BARRIER\n"                     // Not implemented
           "  HELP\n");

    break;

  case CMD_BARRIER: // Not implemented
  case CMD_EMPTY:
    break;

  case EOC:
    ems_terminate();
    break;
  default:
    fprintf(stderr, "Invalid command. See HELP for usage\n");
    return 1;
  }
  return 0;
}
