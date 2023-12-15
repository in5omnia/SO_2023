//
// Created by Eduardo Nazário on 14/12/2023.
//

#ifndef SO_2023_MAIN_H
#define SO_2023_MAIN_H

struct thread_func_args {
  int fd;
  char *job_filepath;
  int thread_num;
  int max_threads;
  unsigned int *wait;
	unsigned int *waiting_threads;
	unsigned int *barrier_active;
  pthread_mutex_t *wait_lock;
  pthread_mutex_t *write_lock;
  pthread_mutex_t *read_lock;
  pthread_mutex_t *reserve_lock;
  pthread_mutex_t *create_lock;
  pthread_mutex_t *barrier_active_lock;
  pthread_mutex_t *barrier_unlock_lock;
  pthread_cond_t *barrier_active_cond;
  pthread_cond_t *barrier_unlock_cond;
};

int exec_file(int fd, char *job_filepath, int max_threads);

int traverse_dir(char *dirpath, list_t *fileList);
void *thread_func(void *arg);

#endif // SO_2023_MAIN_H
