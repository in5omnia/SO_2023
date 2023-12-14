//
// Created by Beatriz Gavilan on 13/12/2023.
//

#ifndef P1_BASE_AUXILIAR_FUNCTIONS_H
#define P1_BASE_AUXILIAR_FUNCTIONS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define EXTENSION_STR ".out"
#define EXTENSION_LEN 4

char* generate_filepath(char* filename);
int check_bytes_written(int out_file, const char* buffer, ssize_t bytes_written, ssize_t bytes_to_write);

#endif //P1_BASE_AUXILIAR_FUNCTIONS_H
