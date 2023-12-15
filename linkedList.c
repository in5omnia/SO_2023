//
// Created by Eduardo Nazário on 14/12/2023.
//

#include "linkedList.h"

list_t *create_linkedList() {
  list_t *list = (list_t *)malloc(sizeof(list_t));
  if (list == NULL) {
    perror("Error creating list");
    exit(errno);
  }
  list->head = NULL;
  list->tail = NULL;
  list->size = 0;
  return list;
}

int append_to_linkedList(list_t *list, char *data) {
  if (list == NULL) {
    fprintf(stderr, "Error: list is NULL\n");
    return 1;
  }

  node_t *new_node = (node_t *)malloc(sizeof(node_t));
  if (new_node == NULL) {
    fprintf(stderr, "Error creating new node\n");
    return 1;
  }

  new_node->data = strdup(data);
  new_node->next = NULL;

  if (list->head == NULL) {
    list->head = new_node;
    list->tail = new_node;
  } else {
    list->tail->next = new_node;
    list->tail = new_node;
  }
  list->size++;
  return 0;
}

void free_linkedList_node(node_t *node) {
  if (node == NULL)
    return;
  free(node->data);
  free(node);
}

void free_linkedList(list_t *list) {
  if (list == NULL)
    return;

  node_t *current = list->head;
  while (current) {
    node_t *temp = current;
    current = current->next;
    free_linkedList_node(temp);
  }
  free(list);
}

char *pop_linkedList(list_t *list) {
  if (list == NULL)
    return NULL;

  node_t *current = list->head;
  if (current == NULL)
    return NULL;

  list->head = current->next;

  char *data = strdup(current->data);
  free_linkedList_node(current);

  list->size--;

  return data;
}