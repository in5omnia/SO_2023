#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "auxiliar_functions.h"
#include "eventlist.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_ms = 0;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int* get_seat_with_delay(struct Event* event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_ms) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_ms = delay_ms;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  free_list(event_list);
  return 0;
}


//Creates a new event.
int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (get_event_with_delay(event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);
    free(event);
    return 1;
  }

  return 0;
}


// Reserves seats for an event.
int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      break;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      fprintf(stderr, "Seat already reserved\n");
      break;
    }

    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;
  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats) {
    event->reservations--;
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0;
    }
    return 1;
  }

  return 0;
}

int ems_show(unsigned int event_id, char * job_filepath) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  char * out_file_path = generate_filepath(job_filepath);
  if (out_file_path == NULL) {
		fprintf(stderr, "Error generating filepath\n");
		return 1;
  }
  int out_file = open(out_file_path, O_CREAT | O_WRONLY | O_APPEND, 0666);	//FIXME: what file permission number to use
  if (out_file == -1) {
	fprintf(stderr, "Error opening file\n");
	return 1;
  }


  unsigned long buffer_size = (event->rows*event->cols*2)+1;
  char buffer[buffer_size];
  unsigned long number_chars_written = 0;
  int written_len;

  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      unsigned int* seat = get_seat_with_delay(event, seat_index(event, i, j));

	  written_len = snprintf(buffer + number_chars_written, buffer_size - number_chars_written, "%d", *seat);
	  if (written_len != 1) {
			fprintf(stderr, "Error writing to buffer\n");
			return 1;
	  }
	  ++number_chars_written;

      if (j < event->cols) {
		written_len = snprintf(buffer + number_chars_written, buffer_size - number_chars_written, "%s", " ");
		if (written_len != 1) {
			  fprintf(stderr, "Error writing to buffer\n");
			  return 1;
		}
		++number_chars_written;
      }
    }
	written_len = snprintf(buffer + number_chars_written, buffer_size - number_chars_written, "%s", "\n");
	if (written_len != 1) {
		fprintf(stderr, "Error writing to buffer\n");
		return 1;
	}
	++number_chars_written;
  }
  ssize_t bytes_written = write(out_file, buffer, buffer_size-1);	//remove null terminator
  int check_bytes = check_bytes_written(out_file, buffer, bytes_written, (ssize_t)buffer_size-1);

  close(out_file);
  if (check_bytes == 1) {
	  return 1;
  }
  return 0;
}

int ems_list_events(char * job_filepath) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (event_list->head == NULL) {
    printf("No events\n");
    return 0;
  }

  char * out_file_path = generate_filepath(job_filepath);
  if (out_file_path == NULL) {
	  fprintf(stderr, "Error generating filepath\n");
	  return 1;
  }
  int out_file = open(out_file_path, O_CREAT | O_WRONLY | O_APPEND, 0666);	//FIXME: what file permission number to use
  if (out_file == -1) {
	  fprintf(stderr, "Error opening file\n");
	  return 1;
  }


  ssize_t bytes_written;
  int written_len;
  char buffer[EVENT_LIST_BUFFER_SIZE];

  struct ListNode* current = event_list->head;
  while (current != NULL) {

	  written_len = snprintf(buffer, EVENT_LIST_BUFFER_SIZE, "Event: ");
	  if (written_len != EVENT_LIST_CHARS_WRITTEN) {
		  fprintf(stderr, "Error writing to buffer1\n");
		  return 1;
	  }
	  written_len = snprintf(buffer + EVENT_LIST_CHARS_WRITTEN, EVENT_LIST_BUFFER_SIZE - EVENT_LIST_CHARS_WRITTEN, "%u\n", (current->event)->id);
	  if (written_len != 2) {
		  fprintf(stderr, "Error writing to buffer2\n");
		  return 1;
	  }
	  bytes_written = write(out_file, buffer, EVENT_LIST_BUFFER_SIZE-1);	//remove null terminator
	  int check_bytes = check_bytes_written(out_file, buffer, bytes_written, (ssize_t)EVENT_LIST_BUFFER_SIZE-1);
	  if (check_bytes == 1) {
		  close(out_file);
		  return 1;
	  }
	  //printf("Event: ");
	  //printf("%u\n", (current->event)->id);
	  current = current->next;
  }
  close(out_file);
  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
