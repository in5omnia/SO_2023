#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/types.h>
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
  int max_threads = (int)strtoul(argv[MAX_THREADS_ARG_INDEX], &endptr, 10);
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

      if (exec_file(open(filepath, O_RDONLY), filepath, max_threads)) {
        exit(1);
      };
      if (ems_terminate()) { // free mallocs
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

        printf("----Filename: %s\t-------------------------------\n", filename);
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

int exec_file(int fd, char *job_filepath, const int max_threads) {
  pthread_t *threads[max_threads];
  for (int i = 0; i < max_threads; i++) {
    threads[i] = malloc(sizeof(pthread_t));
  }

  pthread_mutex_t *write_lock;
  write_lock = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(write_lock, NULL) != 0) {
		fprintf(stderr, "Failed to create mutex\n");
		return 1;
	}
  pthread_mutex_t *read_lock;
  read_lock = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(read_lock, NULL) != 0) {
		fprintf(stderr, "Failed to create mutex\n");
		return 1;
	}
  pthread_mutex_t *reserve_lock;
  reserve_lock = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(reserve_lock, NULL) != 0) {
		fprintf(stderr, "Failed to create mutex\n");
		return 1;
	}
  pthread_mutex_t *create_lock;
  create_lock = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(create_lock, NULL) != 0) {
		fprintf(stderr, "Failed to create mutex\n");
		return 1;
	}
  pthread_mutex_t *barrier_active_lock;
  barrier_active_lock = malloc(sizeof(pthread_mutex_t));
  if (pthread_mutex_init(barrier_active_lock, NULL) != 0) {
    fprintf(stderr, "Failed to create mutex\n");
    return 1;
  }
  pthread_mutex_t *barrier_unlock_lock;
	barrier_unlock_lock = malloc(sizeof(pthread_mutex_t));
  if (pthread_mutex_init(barrier_unlock_lock, NULL) != 0) {
    fprintf(stderr, "Failed to create mutex\n");
    return 1;
  }
  pthread_cond_t *barrier_active_cond;
  barrier_active_cond = malloc(sizeof(pthread_cond_t));
  pthread_cond_init(barrier_active_cond, NULL);
  pthread_cond_t *barrier_unlock_cond;
	barrier_unlock_cond = malloc(sizeof(pthread_cond_t));
  pthread_cond_init(barrier_unlock_cond, NULL);

	unsigned int* barrier_active = malloc(sizeof(unsigned int));
	(*barrier_active) = 0;
	unsigned int* waiting_threads = malloc(sizeof(unsigned int));
	(*waiting_threads) = 0;

  if (fd == -1) {
    fprintf(stderr, "Failed to open file %s\n", job_filepath);
    return 1;
  }

  // launch max_threads
  unsigned int *wait;
  wait = malloc(sizeof(unsigned int) * (unsigned long)max_threads);
  // initialize wait array
  for (int i = 0; i < max_threads; i++) {
    wait[i] = 0;
  }
  pthread_mutex_t *wait_lock;
  wait_lock = malloc(sizeof(pthread_mutex_t));
  (*wait_lock) = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

  for (int i = 0; i < max_threads; i++) {
    struct thread_func_args *args = malloc(sizeof(struct thread_func_args));
    args->fd = fd;
    args->job_filepath = job_filepath;
    args->thread_num = i;
    args->max_threads = max_threads;
    args->wait = wait;
    args->wait_lock = wait_lock;
    args->write_lock = write_lock;
    args->read_lock = read_lock;
    args->reserve_lock = reserve_lock;
    args->create_lock = create_lock;
    args->barrier_active_cond = barrier_active_cond;
    args->barrier_unlock_cond = barrier_unlock_cond;
    args->barrier_active_lock = barrier_active_lock;
    args->barrier_unlock_lock = barrier_unlock_lock;
		args->waiting_threads = waiting_threads;
		args->barrier_active = barrier_active;


    if (pthread_create(threads[i], NULL, thread_func, args)) {
      fprintf(stderr, "Error creating thread\n");
      free(args);
      return 1;
    }
  }

  // wait for all to terminate
  for (int i = 0; i < max_threads; i++) {
    pthread_join(*threads[i], NULL);
    free(threads[i]);
  }

  return 0;
}

void *thread_func(void *arg) {
	// Read args
  struct thread_func_args *args = (struct thread_func_args *)arg;
  int fd = args->fd;
  char *job_filepath = args->job_filepath;
  int thread_num = args->thread_num;
  int max_threads = args->max_threads;
  unsigned int *wait = args->wait;
	unsigned int *waiting = args->waiting_threads;
	unsigned int *barrier_active = args->barrier_active;
	printf("BARRIER POINTER 3 %p\n", barrier_active);

  pthread_mutex_t *wait_lock = args->wait_lock;
  pthread_mutex_t *write_lock = args->write_lock;
  pthread_mutex_t *read_lock = args->read_lock;
  pthread_mutex_t *reserve_lock = args->reserve_lock;
  pthread_mutex_t *create_lock = args->create_lock;
  pthread_mutex_t *barrier_active_lock = args->barrier_active_lock;
  pthread_mutex_t *barrier_unlock_lock =
      args->barrier_unlock_lock;
  pthread_cond_t *barrier_active_cond = args->barrier_active_cond;
  pthread_cond_t *barrier_unlock_cond =
      args->barrier_unlock_cond;

  unsigned int event_id, delay;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];


  while (1) {
    // first thing to do is to check if there's been a wait command
    // lock to check if there's been a wait command

    pthread_mutex_lock(wait_lock);
    // print the entire wait array
    unsigned int wait_time = wait[thread_num];
    if (wait_time > 0) {
      wait[thread_num] -= wait_time;
      pthread_mutex_unlock(wait_lock);
      printf("Process %d: Waiting %d ms\n", thread_num, wait_time);
      ems_wait((unsigned int)wait_time);
      printf("Process %d: Finished waiting", thread_num);
    } else {
      pthread_mutex_unlock(wait_lock);
    }

		if (pthread_mutex_trylock(read_lock) != 0) {	//read was already locked
			// lock barrier_active_lock
			printf("Thread %d: barrier active Lock\n", thread_num);
			pthread_mutex_lock(barrier_active_lock);
			printf("THREAD %d, BARRIER STATUS: %d\n",thread_num, *barrier_active);
			// print as well the pointer location to barrier status
			printf("LOCATION OF BARRIER STATUS: %p\n", barrier_active);
			if ((*barrier_active) != 0) {
				printf("Thread %d: barrier active\n", thread_num);
				--(*waiting);
				printf("Thread %d: Goind to signal barrier active cond, Waiting = %d\n", thread_num, *waiting);
				pthread_cond_signal(barrier_active_cond);
				printf("Thread %d: Signaled barrier active cond\n", thread_num);
				pthread_mutex_unlock(barrier_active_lock);
				printf("Thread %d: barrier active Unlock\n", thread_num);
				pthread_mutex_lock(barrier_unlock_lock);
				while ((*barrier_active) != 0){
					printf("Thread %d: barrier unlock cond wait\n", thread_num);
					pthread_cond_wait(barrier_unlock_cond, barrier_unlock_lock);
				}
				pthread_mutex_unlock(barrier_unlock_lock);
				printf("Thread %d: barrier unlock cond wait finished\n", thread_num);
				sched_yield();
			} else {  // barrier is not active
				printf("Thread %d: barrier not active\n", thread_num);
				pthread_mutex_unlock(barrier_active_lock);
				sched_yield();
			}
			continue;
		}

    switch (get_next(fd)) {
    case CMD_CREATE:
      printf("CREATE --------------- %s \n", job_filepath);
      if (parse_create(fd, &event_id, &num_rows, &num_columns) != 0) {
        pthread_mutex_unlock(read_lock);
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        return (void *)1;
      }
      pthread_mutex_unlock(read_lock);
      if (ems_create(event_id, num_rows, num_columns, create_lock)) {
        fprintf(stderr, "Failed to create event\n");
      }
      break;

    case CMD_RESERVE:
      printf("RESERVE --------------- %s\n", job_filepath);
      num_coords = parse_reserve(fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);
      pthread_mutex_unlock(read_lock);
      if (num_coords == 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        return (void *)1;
      }
      if (ems_reserve(event_id, num_coords, xs, ys, reserve_lock)) {
        fprintf(stderr, "Failed to reserve seats\n");
      }
      break;

    case CMD_SHOW:
      printf("SSHOW --------------- %s\n", job_filepath);
      if (parse_show(fd, &event_id) != 0) {
        pthread_mutex_unlock(read_lock);
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        return (void *)1;
      }
      pthread_mutex_unlock(read_lock);
      if (ems_show(event_id, job_filepath, write_lock, reserve_lock)) {
        fprintf(stderr, "Failed to show event\n");
      }
      break;

    case CMD_LIST_EVENTS:
      printf("LIST ABOUT TO UNLOCK READ --------------- %s\n", job_filepath);
      pthread_mutex_unlock(read_lock);

      if (ems_list_events(job_filepath, write_lock, create_lock)) {
        fprintf(stderr, "Failed to list events\n");
      }
      break;

    case CMD_WAIT:
      printf("SWITCH cmd WAIT \n");
      unsigned int thread_asked = 0;
      // 0 = ALL THREADS
      int ret = parse_wait(fd, &delay, &thread_asked);
      pthread_mutex_unlock(read_lock);
      if (ret == -1) { // thread_id is not implemented
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        return (void *)1;
      }
      if (ret == 0) {
        // ALL THREADS
        pthread_mutex_lock(wait_lock);
        for (int i = 0; i < max_threads; i++) {
          wait[i] = delay;
        }
        pthread_mutex_unlock(wait_lock);
      } else if (ret == 1) {
        // SPECIFIC THREAD
        pthread_mutex_lock(wait_lock);
        wait[thread_asked] = delay;
        pthread_mutex_unlock(wait_lock);
      }

      if (delay > 0) {
        printf("Waiting...\n");
        ems_wait(delay);
      }
      break;

    case CMD_INVALID:
      pthread_mutex_unlock(read_lock);
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      break;

    case CMD_HELP: // fixme ffs
      pthread_mutex_unlock(read_lock);
      printf("Available commands:\n"
             "  CREATE <event_id> <num_rows> <num_columns>\n"
             "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
             "  SHOW <event_id>\n"
             "  LIST\n"
             "  WAIT <delay_ms> [thread_id]\n" // thread_id is not// implemented
             "  BARRIER\n"                     // Not implemented
             "  HELP\n");

      break;

    case CMD_BARRIER:
      if (ems_barrier(barrier_active_cond, barrier_unlock_cond,
                      barrier_active_lock, max_threads, waiting, barrier_active)) {
				pthread_mutex_unlock(read_lock);
				fprintf(stderr, "Failed to create barrier\n");
        return (void *)1;
      }
      pthread_mutex_unlock(read_lock);

    case CMD_EMPTY:
      pthread_mutex_unlock(read_lock);
      break;

    case EOC:
      printf(
          "EOC------------------------------------ %s --------------------\n",
          job_filepath);
      pthread_mutex_unlock(read_lock);
      printf("Thread %d exiting\n", thread_num);
      return (void *)0;

    default:
      pthread_mutex_unlock(read_lock);
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      printf("Thread %d exiting\n", thread_num);
      return (void *)1;
    }
  }

  return NULL;
}
