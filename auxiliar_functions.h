//
// Created by Beatriz Gavilan on 13/12/2023.
//

#ifndef P1_BASE_AUXILIAR_FUNCTIONS_H
#define P1_BASE_AUXILIAR_FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EXTENSION_STR ".out"
#define EXTENSION_LEN 4
#define EVENT_LIST_BUFFER_SIZE 10 //"Event: 1\n"
#define EVENT_LIST_CHARS_WRITTEN 7
#define NO_EVENTS_BUFFER_SIZE 11 //"No events\n"
#define HELP_MESSAGE                                                           \
  ("Available commands:\n  CREATE <event_id> <num_rows> <num_columns>\n  "     \
   "RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n  SHOW <event_id>\n  "   \
   "LIST\n  WAIT <delay_ms> [thread_id]\n  BARRIER\n  HELP\n")
#define HELP_BUFFER_SIZE (strlen(HELP_MESSAGE)+1)     // includes null terminator

char *generate_filepath(char *filename);
int check_bytes_written(int out_file, const char *buffer, ssize_t bytes_written,
                        ssize_t bytes_to_write);

#endif // P1_BASE_AUXILIAR_FUNCTIONS_H
