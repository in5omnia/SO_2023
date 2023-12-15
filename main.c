#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dirent.h>
#include <unistd.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

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
      printf("----Filename: %s\t-------------------------------\n", filename);
      int filename_len = (int)strlen(filename);

      if (filename_len > JOB_FILE_EXTENSION_LEN &&
          strcmp(filename + filename_len - JOB_FILE_EXTENSION_LEN,
                 JOB_FILE_EXTENSION) == 0) {

        if (ems_init(state_access_delay_ms)) {
          fprintf(stderr, "Failed to initialize EMS\n");
          return 1;
        }
        printf("EMS initialized\n");

        char *filepath = malloc(strlen(dirpath) + strlen(entry->d_name) + 2);
        if (filepath == NULL) {
          fprintf(stderr, "Error allocating memory for filepath\n");
          return 1;
        }
        sprintf(filepath, "%s/%s", dirpath, entry->d_name);
        fprintf(stdout, "Processing file %s\n", filepath);

        if (ems_submit_file(filepath)) {
          fprintf(stderr, "Failed to submit file %s\n", filepath);
          return 1;
        }
        free(filepath);
        ems_terminate();
        printf("EMS terminated\n");
      }
    }
    entry = readdir(dir);
  }

  return 0;
}

int exec_file(int fd, char *job_filepath) {
  unsigned int event_id, delay;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

  while (1) {

    switch (get_next(fd)) {
    case CMD_CREATE:
      printf("SWITCH cmd CREATE \n");
      if (parse_create(fd, &event_id, &num_rows, &num_columns) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        return 1;
      }

      if (ems_create(event_id, num_rows, num_columns)) {
        fprintf(stderr, "Failed to create event\n");
      }

      break;

    case CMD_RESERVE:
      printf("SWITCH cmd RESERVE \n");
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
      printf("SWITCH cmd SHOW \n");
      if (parse_show(fd, &event_id) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        return 1;
      }
      if (ems_show(event_id, job_filepath)) {
        fprintf(stderr, "Failed to show event\n");
      }

      break;

    case CMD_LIST_EVENTS:
      printf("SWITCH cmd LIST \n");
      if (ems_list_events(job_filepath)) {
        fprintf(stderr, "Failed to list events\n");
      }
      break;

    case CMD_WAIT:
      printf("SWITCH cmd WAIT \n");
      if (parse_wait(fd, &delay, NULL) == -1) { // thread_id is not implemented
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        return 1;
      }

      if (delay > 0) {
        printf("Waiting...\n");
        ems_wait(delay);
      }

      break;

    case CMD_INVALID:
      printf("SWITCH cmd INVALID \n");
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      break;

    case CMD_HELP:
      printf("Available commands:\n"
             "  CREATE <event_id> <num_rows> <num_columns>\n"
             "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
             "  SHOW <event_id>\n"
             "  LIST\n"
             "  WAIT <delay_ms> [thread_id]\n" // thread_id is not implemented
             "  BARRIER\n"                     // Not implemented
             "  HELP\n");

      break;

    case CMD_BARRIER: // Not implemented

    case CMD_EMPTY:
      break;

    case EOC:
      printf("SWITCH cmd EOC \n");
      return 0;

    default:
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      return 1;
    }
  }
}
