//
// Created by Eduardo Nazário on 14/12/2023.
//

#ifndef SO_2023_LINKEDLIST_H
#define SO_2023_LINKEDLIST_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node {
  char *data;
  struct node *next;
} node_t;

typedef struct list {
  node_t *head;
  node_t *tail;
  int size;
} list_t;

list_t *create_linkedList();
int append_to_linkedList(list_t *list, char *data);
void free_linkedList_node(node_t *node);
void free_linkedList(list_t *list);
char *pop_linkedList(list_t *list);

#endif // SO_2023_LINKEDLIST_H
