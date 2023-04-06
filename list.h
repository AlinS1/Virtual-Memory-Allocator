// Similea Alin-Andrei 314CA
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node_t {
	struct node_t *prev;
	struct node_t *next;
	void *data;
} node_t;

typedef struct {
	node_t *head;
	unsigned int data_size;
	unsigned int total_elements;
} list_t;

// ===== Linked-list functions =====
list_t *ll_create(unsigned int data_size);
void ll_add_nth_node(list_t *list, unsigned int n, const void *new_data);
node_t *ll_remove_nth_node(list_t *list, unsigned int n);
unsigned int ll_get_size(list_t *list);
void ll_free(list_t **pp_list);
void free_node(list_t *list, int idx);
