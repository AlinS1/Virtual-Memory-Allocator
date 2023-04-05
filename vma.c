#include "vma.h"

#include "list.h"

// We initialize the arena.
arena_t *alloc_arena(const uint64_t size)
{
	arena_t *arena = (arena_t *)malloc(sizeof(arena_t));
	DIE(!arena, "malloc failed");
	arena->arena_size = size;
	arena->alloc_list = ll_create(sizeof(block_t));

	return arena;
}

// Free the memory of all the rw_buffers from a list of miniblocks from a
// single block.
void free_buffers(list_t *minib_list)
{
	node_t *curr_minib_node = minib_list->head;
	for (unsigned int i = 0; i < minib_list->total_elements; i++) {
		miniblock_t *curr_minib = (miniblock_t *)curr_minib_node->data;
		if (curr_minib->rw_buffer) {
			free(curr_minib->rw_buffer);
			curr_minib->rw_buffer = NULL;
		}
		if (curr_minib_node->next)
			curr_minib_node = curr_minib_node->next;
	}
}

// Free the memory of the arena.
void dealloc_arena(arena_t *arena)
{
	// For each block remaining in the list, we deallocate its miniblock
	// buffers, miniblock list and then the block itself.
	while (arena->alloc_list->total_elements) {
		node_t *curr = arena->alloc_list->head;
		block_t *curr_block = (block_t *)curr->data;
		list_t *curr_mb_list = (list_t *)curr_block->miniblock_list;
		free_buffers(curr_mb_list);
		ll_free(&curr_mb_list);
		free_node(arena->alloc_list, 0);
	}

	// deallocate the main block list. (the arena itself will be freed in the
	// main function)
	free(arena->alloc_list);
	arena->alloc_list = NULL;
}

// Concatenates a given(new) block to another given(old) block.
// idx = -1 -> add the new block after the old one. (1.OLD, 2.NEW)
// idx = 1 -> add the new block before the old one. (1.NEW, 2.OLD)
void concat_block(block_t *old_block, block_t *new_block, int idx)
{
	old_block->size += new_block->size;
	list_t *old_miniblock_list = (list_t *)old_block->miniblock_list;
	list_t *new_miniblock_list = (list_t *)new_block->miniblock_list;

	// We add the new one after the old one. (1.OLD, 2.NEW)
	if (idx == -1) {
		while (new_miniblock_list->total_elements) {
			// Add the first new block's node after the last old block's node.
			node_t *curr_minib = new_miniblock_list->head;
			ll_add_nth_node(old_miniblock_list,
							old_miniblock_list->total_elements,
							curr_minib->data);

			// free the old node memory, because of deep copy in ll_add_nth_node
			free_node(new_miniblock_list, 0);
		}
	}

	// We add the new one before the old one. (1.NEW, 2.OLD)
	if (idx == 1) {
		old_block->start_address = new_block->start_address;
		while (new_miniblock_list->total_elements) {
			// Add the last new block's node before the first old block's node.
			node_t *minib_current = new_miniblock_list->head;
			for (unsigned int i = 0; i < new_miniblock_list->total_elements - 1;
				 i++)
				minib_current = minib_current->next;
			ll_add_nth_node(old_miniblock_list, 0, minib_current->data);

			// free the old node memory, because of deep copy in ll_add_nth_node
			free_node(new_miniblock_list, new_miniblock_list->total_elements);
		}
	}
	// free the new block's memory, because we concatenated it in the old block.
	ll_free((list_t **)&new_block->miniblock_list);
}

// Handles the various errors a block allocation can give in terms of the arena
// not being previously allocated and in terms of the block's beginning and end
// address being out of the arena's borders.
int alloc_block_errors(arena_t *arena, uint64_t address, uint64_t end_addr_new)
{
	if (!arena) {
		printf("Arena was not allocated.\n");
		return 0;
	}
	if (address + 1 > arena->arena_size) {
		printf("The allocated address is outside the size of arena\n");
		return 0;
	}
	if (end_addr_new + 1 > arena->arena_size) {
		printf("The end address is past the size of the arena\n");
		return 0;
	}
	return 1;
}

// Used whether the new block we want to allocate is between TWO already
// existing blocks.
void cases_of_alloc_block(arena_t *arena, int k, block_t *prev_b,
						  uint64_t prev_end, block_t *next_b,
						  uint64_t next_start, block_t *new_block,
						  uint64_t end_address_new)
{
	// Case 1: The new block is adjacent to the previous and the next blocks.
	// The result will be a single block made up of 3 blocks
	if ((prev_end == new_block->start_address - 1) &&
		(end_address_new == next_start - 1)) {
		// Concatenate the new block to the previous block.
		concat_block(prev_b, new_block, -1);
		// Concatenate the next block to the previously resulted block.
		concat_block(prev_b, next_b, -1);
		// Free the memory of the next block, because it was concatenated to the
		// previous one.
		free_node(arena->alloc_list, k);
		return;
	}

	// Case 2: The new block is only adjacent to the previous block.
	if (prev_end == new_block->start_address - 1) {
		// Concatenate the new block to the previous block.
		concat_block(prev_b, new_block, -1);
		return;
	}

	// Case 3: The new block is only adjacent to the next block.
	if (end_address_new == next_start - 1) {
		// Concatenate the new block to the next block.
		concat_block(next_b, new_block, 1);
		return;
	}

	// Case 4: The new block is not adjacent to any blocks, so we add it to the
	// list normally.
	ll_add_nth_node(arena->alloc_list, k, new_block);
}

// Creates a basic block(with given starting address and size) that is going to
// be put in the arena. -> function used in alloc_block
block_t *init_new_block(uint64_t address, uint64_t size)
{
	block_t *new_block = malloc(sizeof(block_t));
	DIE(!new_block, "malloc failed");
	new_block->size = size;
	new_block->start_address = address;
	new_block->miniblock_list = ll_create(sizeof(miniblock_t));

	// list of miniblocks
	miniblock_t *miniblock_l = malloc(sizeof(miniblock_t));
	DIE(!miniblock_l, "malloc failed");
	miniblock_l->size = size;
	miniblock_l->start_address = address;
	miniblock_l->perm = 6;	// default RW-
	miniblock_l->rw_buffer = NULL;
	ll_add_nth_node(new_block->miniblock_list, 0, miniblock_l);

	// we free miniblock_l, because we made a deep copy inside ll_add_nth_node.
	free(miniblock_l);
	return new_block;
}

// Create a block and add it in the list of blocks from the arena or, if
// adjacent to other previously existing blocks, concatenate it to other blocks.
void alloc_block(arena_t *arena, const uint64_t address, const uint64_t size)
{
	uint64_t end_address_new = address + size - 1;
	if (!alloc_block_errors(arena, address, end_address_new))
		return;
	block_t *new_block = init_new_block(address, size);

	// Case 1: There are no existing elements in the arena.
	if (arena->alloc_list->total_elements == 0) {
		ll_add_nth_node(arena->alloc_list, 0, new_block);
		free(new_block);
		return;
	}

	// Case 2: New block would be positioned before the first block in the list.
	node_t *first = arena->alloc_list->head;
	block_t *b_first = (block_t *)first->data;
	if (end_address_new < b_first->start_address) {
		// Verify if the new block is adjacent to the existing first block.
		if (end_address_new == b_first->start_address - 1) {
			// Concatenate the new block to the old block.
			concat_block(b_first, new_block, 1);
		} else {
			// Add the new block to the list of blocks in the arena.
			ll_add_nth_node(arena->alloc_list, 0, new_block);
		}
		free(new_block);  // because of deep copy in ll_add_nth_node
		return;
	}

	// Case 3: New block would be positioned after the last block in the list.
	node_t *last = arena->alloc_list->head;
	for (unsigned int i = 0; i < arena->alloc_list->total_elements - 1; i++)
		last = last->next;
	block_t *b_last = (block_t *)last->data;
	if (address > b_last->start_address + b_last->size - 1) {
		// Verify if the new block is adjacent to the existing last block.
		if (address == b_last->start_address + b_last->size) {
			// Concatenate the new block to the old block.
			concat_block(b_last, new_block, -1);
		} else {
			// Add the new block to the list of blocks in the arena.
			ll_add_nth_node(arena->alloc_list,
							arena->alloc_list->total_elements, new_block);
		}
		free(new_block);  // because of deep copy in ll_add_nth_node
		return;
	}

	// Case 4: New block would be positioned between two already existing ones.
	if (arena->alloc_list->total_elements >= 2) {
		node_t *previous = first;
		node_t *next_n = first->next;
		int k = 1;	// the index where the new block should be found.

		do {
			block_t *prev_b = (block_t *)previous->data;
			uint64_t prev_end = prev_b->start_address + prev_b->size - 1;
			block_t *next_b = (block_t *)next_n->data;
			uint64_t next_start = next_b->start_address;

			// Verify if the new block is found between the current blocks.
			if (prev_end < new_block->start_address &&
				end_address_new < next_start) {
				// Add the new block to the arena depending on whether it is
				// adjacent to any already existing blocks or not.
				cases_of_alloc_block(arena, k, prev_b, prev_end, next_b,
									 next_start, new_block, end_address_new);
				free(new_block);
				return;
			}
			previous = previous->next;
			next_n = next_n->next;
			k++;
		} while (next_n);
	}
	// Reach error only if a free zone is not found.
	printf("This zone was already allocated.\n");
	ll_free((list_t **)&new_block->miniblock_list);
	free(new_block);
}

// Eliminates a miniblock from the arena.
void free_block(arena_t *arena, const uint64_t address)
{
	if (!arena || arena->alloc_list->total_elements == 0) {
		printf("Invalid address for free.\n");
		return;
	}
	unsigned int i;
	block_t *curr_block = find_block(arena, address, &i);
	if (!curr_block) {
		printf("Invalid address for free.\n");	// No block was found.
		return;
	}

	list_t *minib_list = (list_t *)curr_block->miniblock_list;
	node_t *minib_curr_node = minib_list->head;

	for (unsigned int j = 0; j < minib_list->total_elements; j++) {
		miniblock_t *minib_curr = (miniblock_t *)minib_curr_node->data;
		if (minib_curr->start_address == address) {
			// Miniblock to be freed is found.

			// Case 1: The block has only one miniblock so we free it whole.
			if (minib_list->total_elements == 1) {
				ll_free(&minib_list);
				free_node(arena->alloc_list, i);
				return;
			}

			// Case 2: First or last miniblock in a list of miniblocks.
			if (j == 0 || j == minib_list->total_elements - 1) {
				// inceput sau end of block
				if (j == 0)
					curr_block->start_address += minib_curr->size;
				curr_block->size -= minib_curr->size;
				free_node(minib_list, j);
				return;
			}

			// Case 3: The miniblock to be freed is somewhere in the middle.
			// number of miniblock -> j, number of block -> i
			minib_curr_node = minib_curr_node->next;  // To not lose the next.

			curr_block->size -= minib_curr->size;  // Decrease the block size
			free_node(minib_list, j);			   // Free miniblock to be freed

			// Create a new block with the miniblocks after the freed miniblock.
			block_t *new_block = malloc(sizeof(block_t));
			DIE(!new_block, "malloc failed");
			new_block->size = 0;

			// First miniblock in the new block's miniblock list.
			miniblock_t *new_minib = (miniblock_t *)minib_curr_node->data;
			new_block->start_address = new_minib->start_address;

			new_block->miniblock_list = ll_create(sizeof(miniblock_t));
			list_t *new_minib_list = (list_t *)new_block->miniblock_list;

			// Add the miniblocks after the freed miniblock to the new list.
			node_t *next = minib_curr_node->next;
			while (minib_list->total_elements - j) {
				ll_add_nth_node(new_minib_list, new_minib_list->total_elements,
								minib_curr_node->data);
				new_minib = (miniblock_t *)minib_curr_node->data;
				new_block->size += new_minib->size;
				free_node(minib_list, j);  // free the old node's memory

				if (minib_list->total_elements - j == 0)
					break;
				minib_curr_node = next;
				if (next->next)
					next = next->next;
			}
			curr_block->size -= new_block->size;  // Update the old block's size
			// Add the new block to the list of blocks.
			ll_add_nth_node(arena->alloc_list, i + 1, new_block);
			free(new_block);
			return;
		}
		minib_curr_node = minib_curr_node->next;
	}
	printf("Invalid address for free.\n");
}

// Prints a given number of characters(size) starting from a certain given
// address.
void read(arena_t *arena, uint64_t address, uint64_t size)
{
	if (!arena || arena->alloc_list->total_elements == 0) {
		printf("Invalid address for read.\n");
		return;
	}

	unsigned int i;
	block_t *curr_block = find_block(arena, address, &i);
	if (!curr_block) {
		printf("Invalid address for read.\n");
		return;
	}

	list_t *minib_list = (list_t *)curr_block->miniblock_list;
	node_t *minib_curr_node = minib_list->head;	 // miniblock node
	uint64_t end_block_curr = curr_block->start_address + curr_block->size - 1;

	for (unsigned int j = 0; j < minib_list->total_elements; j++) {
		miniblock_t *minib_curr = (miniblock_t *)minib_curr_node->data;
		uint64_t end_minib = minib_curr->start_address + minib_curr->size - 1;

		if (minib_curr->start_address <= address && address <= end_minib) {
			// Found first miniblock from which we read.

			if (!check_permission(minib_list, minib_curr_node, size, j, 4)) {
				printf("Invalid permissions for read.\n");
				return;
			}

			if (end_block_curr - address + 1 < size) {
				printf("Warning: size was bigger than the block size.");
				size = end_block_curr - address + 1;
				printf(" Reading %ld characters.\n", size);
			}

			unsigned int idx_total_read = 0;  // how many chars have been read

			// Reading from the first miniblock(the current one).
			if (minib_curr->start_address != address) {
				unsigned int start = minib_curr->start_address;
				unsigned int which_byte = 0;  // which byte in current miniblock

				// Reach the address inside the miniblock where we should start
				// reading from. (could be at the middle of a miniblock)
				while (start != address) {
					which_byte++;
					start++;
				}

				char *data = (char *)minib_curr->rw_buffer;
				while (which_byte < minib_curr->size && idx_total_read < size) {
					printf("%c", data[which_byte]);
					which_byte++;
					idx_total_read++;
				}
				j++;
			}
			// Continue the reading from the following miniblocks.
			for (unsigned int l = j; l < minib_list->total_elements; l++) {
				unsigned int idx = 0;
				char *data = (char *)minib_curr->rw_buffer;

				while (idx < minib_curr->size && idx_total_read < size) {
					printf("%c", data[idx]);
					idx++;
					idx_total_read++;
				}
				// Stop if there are no elements left or the size is reached.
				if (l == minib_list->total_elements - 1 ||
					idx_total_read == size)
					break;
				minib_curr_node = minib_curr_node->next;
				minib_curr = (miniblock_t *)minib_curr_node->data;
			}
			printf("\n");
			return;
		}
	}
	printf("Invalid address for read.\n");
}

// Creates the string of data that we will use in the write function.
char *create_string(uint64_t size)
{
	// Get the remaining characters from the line we got the command from.
	char *param = strtok(NULL, "\n");
	int8_t *data = (int8_t *)param;

	char *data_string = malloc(sizeof(char) * (size + 1));
	DIE(!data_string, "malloc failed");

	// If there are no other characters on the command line, we add the "new
	// line" character that is actually the last character, but the strtok can't
	// recognize it. Then, we add the string terminator.
	if (!data) {
		data_string[0] = '\n';
		data_string[1] = '\0';
	}

	if (data) {
		// Copy the remaining characters from the command line to the data
		// string.
		for (unsigned int i = 0; i < strlen((char *)data); i++)
			data_string[i] = (char)data[i];

		data_string[strlen((char *)data)] = '\0';

		unsigned int len = strlen(data_string);
		// If the current string length is smaller with 1 char compared to the
		// size we are looking for, it means we just reached an end of line so
		// we add the "new line" character.
		if (len == size - 1) {
			data_string[len] = '\n';
			data_string[len + 1] = '\0';
		}
	}

	unsigned int len = strlen(data_string);
	if (len < size) {
		// If the first line contained other characters except from "\n", we
		// make sure we add the "new line" char at the end, because we will have
		// to read from the following lines in order to reach the given size.
		if (data_string[0] != '\n') {
			data_string[len] = '\n';
			data_string[len + 1] = '\0';
		}
		len = strlen(data_string);
		while (len < size) {
			fscanf(stdin, "%c", &data_string[len]);
			// every time we add a new character, we add the string terminator.
			data_string[len + 1] = '\0';
			len = strlen(data_string);
		}
	}

	return data_string;
}

// Writes a number of characters in the miniblocks' buffers starting from a
// given address.
void write(arena_t *arena, const uint64_t address, const uint64_t size,
		   int8_t *data)
{
	if (!arena || arena->alloc_list->total_elements == 0) {
		printf("Invalid address for write.\n");
		free(data);
		return;
	}

	char *data_string = (char *)data;

	unsigned int i;
	block_t *curr_block = find_block(arena, address, &i);
	if (!curr_block) {
		printf("Invalid address for write.\n");
		free(data_string);
		return;
	}

	list_t *minib_list = (list_t *)curr_block->miniblock_list;
	node_t *minib_curr_node = minib_list->head;	 // nod de minib
	uint64_t end_block_curr = curr_block->start_address + curr_block->size - 1;

	for (unsigned int j = 0; j < minib_list->total_elements; j++) {
		miniblock_t *minib_curr = (miniblock_t *)minib_curr_node->data;
		uint64_t end_mb_curr = minib_curr->start_address + minib_curr->size - 1;
		if (minib_curr->start_address <= address && address <= end_mb_curr) {
			// Miniblock found
			if (!check_permission(minib_list, minib_curr_node, size, j, 2)) {
				printf("Invalid permissions for write.\n");
				return;
			}

			if (end_block_curr - address + 1 < size) {
				printf("Warning: size was bigger than the block size.");
				printf(" Writing %ld characters.\n",
					   end_block_curr - address + 1);
			}

			unsigned int idx_data = 0;	// the index of the current char in data

			for (unsigned int l = j; l < minib_list->total_elements; l++) {
				unsigned int idx_minib = 0;	 // index of curr byte in miniblock
				minib_curr->rw_buffer = malloc(minib_curr->size * sizeof(char));
				DIE(!minib_curr->rw_buffer, "malloc failed");

				while (idx_minib < minib_curr->size &&
					   idx_data < strlen(data_string)) {
					// Verify if there are enough characters left in data string
					if (strlen(data_string) - idx_data < minib_curr->size) {
						// Copy the remaining characters from data string.
						memcpy(minib_curr->rw_buffer, data_string + idx_data,
							   strlen(data_string) - idx_data);
						idx_data += strlen(data_string) - idx_data;
						idx_minib += strlen(data_string) - idx_data;
					} else {
						// Copy as many characters as the miniblock supports.
						memcpy(minib_curr->rw_buffer, data_string + idx_data,
							   minib_curr->size);
						idx_data += minib_curr->size;
						idx_minib += minib_curr->size;
					}
				}
				if (l == minib_list->total_elements - 1)
					break;
				minib_curr_node = minib_curr_node->next;
				minib_curr = (miniblock_t *)minib_curr_node->data;
			}
			free(data_string);
			return;
		}
	}
	free(data_string);
	printf("Invalid address for write.\n");
}

// Print the details of the arena(memory, blocks, miniblocks)
void pmap(const arena_t *arena)
{
	if (!arena)
		return;

	printf("Total memory: 0x%lX bytes\n", arena->arena_size);

	uint64_t free_memory = arena->arena_size;
	uint64_t nr_miniblocks = 0;

	// Find the number of miniblocks and free memory
	node_t *curr_node_b = arena->alloc_list->head;
	for (unsigned int i = 0; i < arena->alloc_list->total_elements; i++) {
		block_t *curr_block = (block_t *)curr_node_b->data;
		free_memory -= curr_block->size;

		list_t *miniblock_list = (list_t *)curr_block->miniblock_list;
		nr_miniblocks += miniblock_list->total_elements;

		curr_node_b = curr_node_b->next;
	}
	printf("Free memory: 0x%lX bytes\n", free_memory);
	printf("Number of allocated blocks: %d\n",
		   arena->alloc_list->total_elements);
	printf("Number of allocated miniblocks: %ld\n", nr_miniblocks);

	// Iterate through the list of blocks and then through each block's
	// miniblock list and show details about each of them.
	curr_node_b = arena->alloc_list->head;
	for (unsigned int i = 0; i < arena->alloc_list->total_elements; i++) {
		block_t *curr_block = (block_t *)curr_node_b->data;
		list_t *miniblock_list = (list_t *)curr_block->miniblock_list;
		printf("\nBlock %d begin\n", i + 1);
		printf("Zone: 0x%lX - 0x%lX\n", curr_block->start_address,
			   curr_block->start_address + curr_block->size);

		node_t *curr_node_minib = miniblock_list->head;
		for (unsigned int j = 0; j < miniblock_list->total_elements; j++) {
			miniblock_t *curr_miniblock = (miniblock_t *)curr_node_minib->data;

			printf("Miniblock %d:\t\t0x%lX\t\t-\t\t0x%lX\t\t| ", j + 1,
				   curr_miniblock->start_address,
				   curr_miniblock->start_address + curr_miniblock->size);
			print_permissions(curr_miniblock->perm);

			curr_node_minib = curr_node_minib->next;
		}
		printf("Block %d end\n", i + 1);
		curr_node_b = curr_node_b->next;
	}
}

// Changes the permissions of a certain miniblock.
void mprotect(arena_t *arena, uint64_t address, int8_t *permission)
{
	int8_t perm = find_permission(permission);
	unsigned int i;
	block_t *curr_block = find_block(arena, address, &i);
	if (!curr_block) {
		printf("Invalid address for mprotect.\n");
		return;
	}

	list_t *minib_list = (list_t *)curr_block->miniblock_list;
	node_t *minib_curr_node = minib_list->head;

	for (unsigned int j = 0; j < minib_list->total_elements; j++) {
		miniblock_t *minib_curr = (miniblock_t *)minib_curr_node->data;
		if (minib_curr->start_address == address) {
			// Found the miniblock from the given address.
			// Change the miniblock's permission to the new one.
			minib_curr->perm = perm;
			return;
		}
		minib_curr_node = minib_curr_node->next;
	}
	printf("Invalid address for mprotect.\n");
}

// Transforms the string parameters of the MPROTECT command into a number in
// base 8 in order to easily work with permissions.
// Read -> 4; Write -> 2; Execute -> 1;
int transform_permission(char *data)
{
	if (strcmp(data, "PROT_NONE") == 0)
		return 0;
	if (strcmp(data, "PROT_READ") == 0)
		return 4;
	if (strcmp(data, "PROT_WRITE") == 0)
		return 2;
	if (strcmp(data, "PROT_EXEC") == 0)
		return 1;
	return -1;
}

// Creates the final number in base 8 which represents a miniblock's
// permissions.
// It could be 0(none) or any sum of these: 4(read), 2(write), 1(execute).
int find_permission(int8_t *permission)
{
	int final_permission = 0;
	char *data = (char *)permission;
	char delim[] = "|\n ";
	char *perm = strtok(data, delim);

	// For each parameter in the command, we verify what kind of permission we
	// add to the miniblock.
	while (perm) {
		int type = transform_permission(perm);

		if (type == 0)
			final_permission = 0;
		if (type == 1 || type == 2 || type == 4)
			final_permission += type;
		perm = strtok(NULL, delim);
	}

	return final_permission;
}

// Finds whether there is an allocated block at a given address and returns its
// address if found.
// idx's address is given as parameter in order to change its value -> of great
// use in the free_block function.
block_t *find_block(arena_t *arena, const uint64_t address, unsigned int *idx)
{
	node_t *curr = arena->alloc_list->head;	 // block node
	for (unsigned int i = 0; i < arena->alloc_list->total_elements; i++) {
		block_t *curr_block = (block_t *)curr->data;  // block
		uint64_t curr_ending = curr_block->start_address + curr_block->size - 1;

		// Verify if the address is found somewhere in that block.
		if (curr_block->start_address <= address && address <= curr_ending) {
			*idx = i;
			return curr_block;
		}
		curr = curr->next;
	}
	return NULL;
}

// Verifies if we have permissions to do a certain action on a series of
// miniblocks starting from a given miniblock until we reach a given size.
// mode = 4 -> READ; mode = 2 -> WRITE
int check_permission(list_t *minib_list, node_t *minib_node, uint64_t size,
					 int j, int mode)
{
	uint64_t mask = mode;

	node_t *minib_curr_node = minib_node;
	miniblock_t *minib_curr = (miniblock_t *)minib_curr_node->data;
	unsigned int idx_data = 0;

	for (unsigned int l = j; l < minib_list->total_elements && idx_data < size;
		 l++) {
		idx_data += minib_curr->size;

		// Verify if we have the permission to do the given action on the
		// current miniblock through bitwise operations (bitwise AND).
		if ((minib_curr->perm & mask) == 0)
			return 0;

		if (l == minib_list->total_elements - 1)
			break;
		minib_curr_node = minib_curr_node->next;
		minib_curr = (miniblock_t *)minib_curr_node->data;
	}

	// All the miniblocks verify the permissions.
	return 1;
}

// Prints the permissions of a certain miniblock. Uses bitwise
// operations(bitwise AND)
void print_permissions(uint8_t permissions)
{
	if ((permissions & 4) != 0)
		printf("R");
	else
		printf("-");
	if ((permissions & 2) != 0)
		printf("W");
	else
		printf("-");
	if ((permissions & 1) != 0)
		printf("X");
	else
		printf("-");
	printf("\n");
}

// ===================
// AUXILIARY FUNCTIONS
// ===================

// Translates the string commands into numbers so we will be able to use switch
// case.
int command_type(char *command)
{
	if (strcmp(command, "ALLOC_ARENA") == 0)
		return 1;
	if (strcmp(command, "DEALLOC_ARENA") == 0)
		return 2;
	if (strcmp(command, "ALLOC_BLOCK") == 0)
		return 3;
	if (strcmp(command, "FREE_BLOCK") == 0)
		return 4;
	if (strcmp(command, "READ") == 0)
		return 5;
	if (strcmp(command, "WRITE") == 0)
		return 6;
	if (strcmp(command, "PMAP") == 0)
		return 7;
	if (strcmp(command, "MPROTECT") == 0)
		return 8;
	return 0;
}

// Determines the number of words on a line. This will help us to check if there
// are enough parameters for a certain command.
int nr_of_parameters(char *line, char *delim)
{
	int nr = 0;
	char *word = strtok(line, delim);

	while (word) {
		nr++;
		word = strtok(NULL, delim);
	}

	return nr;
}

// Verifies whether a command has the necessary amount of parameters.
// If not, we print an error for each parameter.
int check_parameters(int type, int nr_param)
{
	int ok = 1;
	if (type == 0)
		ok = 0;
	if (type == 1 && nr_param != 2)	 // ALLOC_ARENA + size
		ok = 0;
	if (type == 2 && nr_param != 1)	 // DEALLOC_ARENA
		ok = 0;
	if (type == 3 && nr_param != 3)	 // ALLOC_BLOCK + address + size
		ok = 0;
	if (type == 4 && nr_param != 2)	 // FREE_BLOCK + address
		ok = 0;

	if (type == 5 && nr_param != 3)	 // READ + address + size
		ok = 0;

	if (type == 6 && nr_param < 3)	// WRITE + address + size + data
		ok = 0;

	if (type == 7 && nr_param != 1)	 // PMAP
		ok = 0;

	if (type == 8 && nr_param < 3)	// MPROTECT + address + new_permissions
		ok = 0;

	if (ok == 0)
		for (int i = 0; i < nr_param; i++)
			printf("Invalid command. Please try again.\n");
	return ok;
}
