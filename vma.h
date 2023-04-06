// Similea Alin-Andrei 314CA
#pragma once
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

#define DIE(assertion, call_description)                       \
	do {                                                       \
		if (assertion) {                                       \
			fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__); \
			perror(call_description);                          \
			exit(errno);                                       \
		}                                                      \
	} while (0)

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
void free_buffers(list_t *minib_list);
void dealloc_arena(arena_t *arena);

void concat_block(block_t *old_block, block_t *new_block, int idx);
int alloc_block_errors(arena_t *arena, uint64_t address, uint64_t end_addr_new);
void cases_of_alloc_block(arena_t *arena, int k, block_t *prev_b,
						  uint64_t prev_end, block_t *next_b,
						  uint64_t next_start, block_t *new_block,
						  uint64_t end_address_new);
block_t *init_new_block(uint64_t address, uint64_t size);

void alloc_block(arena_t *arena, const uint64_t address, const uint64_t size);
void free_block(arena_t *arena, const uint64_t address);

void read(arena_t *arena, uint64_t address, uint64_t size);
char *create_string(uint64_t size);
void write(arena_t *arena, const uint64_t address, const uint64_t size,
		   int8_t *data);
void pmap(const arena_t *arena);
void mprotect(arena_t *arena, uint64_t address, int8_t *permission);

int transform_permission(char *data);
int find_permission(int8_t *permission);
block_t *find_block(arena_t *arena, const uint64_t address, unsigned int *idx);
int check_permission(list_t *minib_list, node_t *minib_node, uint64_t size,
					 int j, int mode);
void print_permissions(uint8_t permissions);

// ===== Auxiliary functions =====
int command_type(char *command);
int nr_of_parameters(char *line, char *delim);
int check_parameters(int type, int nr_param);
