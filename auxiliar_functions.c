#include "auxiliar_functions.h"

#define JOB_F_E_LEN 5

char *generate_filepath(char *filename) {
  char *filepath = strdup(filename);
  if (filepath == NULL) {
    fprintf(stderr, "Error allocating memory\n");
    return NULL; // FIXME how to handle this?
  }
  memcpy(filepath + strlen(filename) - JOB_F_E_LEN, EXTENSION_STR,
         EXTENSION_LEN + 1);
  return filepath;
}

int check_bytes_written(int out_file, const char *buffer, ssize_t bytes_written,
                        ssize_t bytes_to_write) {
  if (bytes_written == -1) {
    fprintf(stderr, "Error writing to file\n");
    return 1;
  } else {
    while (bytes_written < bytes_to_write) { // bytes_to_write = buffer_size-1
      fprintf(stderr, "Only wrote %zd bytes to file\n", bytes_written);
      ssize_t new_bytes_written = write(
          out_file, buffer + bytes_written,
          (size_t)(bytes_to_write - bytes_written)); // remove null terminator
      bytes_written += new_bytes_written;
      if (new_bytes_written == -1) {
        fprintf(stderr, "Error writing to file\n");
        return 1;
      }
    }
    return 0;
  }
}