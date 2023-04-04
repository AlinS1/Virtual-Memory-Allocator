#pragma once
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIE(assertion, call_description)                       \
	do {                                                       \
		if (assertion) {                                       \
			fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__); \
			perror(call_description);                          \
			exit(errno);                                       \
		}                                                      \
	} while (0)

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

typedef struct {
	uint64_t start_address;
	size_t size;
	void *miniblock_list;
} block_t;

typedef struct {
	uint64_t start_address;
	size_t size;
	uint8_t perm;
	void *rw_buffer;
} miniblock_t;

typedef struct {
	uint64_t arena_size;
	list_t *alloc_list;
} arena_t;

arena_t *alloc_arena(const uint64_t size);
void dealloc_arena(arena_t *arena);

void concat_block(block_t *old_block, block_t *new_block, int idx);
int alloc_block_errors(arena_t *arena, uint64_t address, uint64_t end_addr_new);
void alloc_block(arena_t *arena, const uint64_t address, const uint64_t size);
void free_block(arena_t *arena, const uint64_t address);

void read(arena_t *arena, uint64_t address, uint64_t size);
void write(arena_t *arena, const uint64_t address, const uint64_t size,
		   int8_t *data);
void pmap(const arena_t *arena);
void mprotect(arena_t *arena, uint64_t address, int8_t *permission);

int transform_permission(char *data);
int find_permission(int8_t *permission);
block_t *find_block(arena_t *arena, const uint64_t address);
int check_permission(list_t *minib_list, node_t *minib_node, uint64_t size,
					 int j, int mode);
void print_permissions(uint8_t permissions);

// ===== Linked-list functions =====
list_t *ll_create(unsigned int data_size);
void ll_add_nth_node(list_t *list, unsigned int n, const void *new_data);
node_t *ll_remove_nth_node(list_t *list, unsigned int n);
unsigned int ll_get_size(list_t *list);
void ll_free(list_t **pp_list);
void free_node(list_t *list, int idx);

// ===== Auxiliary functions =====
int command_type(char *command);
int nr_of_parameters(char *line, char *delim);
int check_parameters(int type, int nr_param);
