#include "vma.h"

arena_t *alloc_arena(const uint64_t size)
{
	arena_t *arena = (arena_t *)malloc(sizeof(arena_t));
	arena->arena_size = size;
	arena->alloc_list = ll_create(sizeof(block_t));

	return arena;
}

void dealloc_arena(arena_t *arena)
{
	while (arena->alloc_list->total_elements) {
		node_t *curr = arena->alloc_list->head;
		block_t *curr_block = (block_t *)curr->data;
		list_t *curr_mb_list = (list_t *)curr_block->miniblock_list;
		ll_free(&curr_mb_list);
		ll_remove_nth_node(arena->alloc_list, 0);
		free(curr->data);
		curr->data = NULL;
		free(curr);
		curr = NULL;
	}
	free(arena->alloc_list);
	arena->alloc_list = NULL;
}

// 0 -> add the new block at the beginning; 1 -> add the new block at the end
void concat_block(block_t *old_block, block_t *new_block, int idx)
{
	old_block->size += new_block->size;
	list_t *old_miniblock_list = (list_t *)old_block->miniblock_list;
	list_t *new_miniblock_list = (list_t *)new_block->miniblock_list;

	// We add the new one after the old one. (1.OLD, 2.NEW)
	if (idx == -1) {
		while (new_miniblock_list->total_elements) {
			// Add first new node after last old node.
			node_t *curr_minib = new_miniblock_list->head;
			ll_add_nth_node(old_miniblock_list,
							old_miniblock_list->total_elements,
							curr_minib->data);

			node_t *removed_node = ll_remove_nth_node(new_miniblock_list, 0);
			if (removed_node->data) {
				free(removed_node->data);
				removed_node->data = NULL;
			}
			free(removed_node);
			removed_node = NULL;
		}
	}

	// We add the new one before the old one. (1.NEW, 2.OLD)
	if (idx == 1) {
		old_block->start_address = new_block->start_address;
		while (new_miniblock_list->total_elements) {
			// Add last new node before first old node.
			node_t *minib_current = new_miniblock_list->head;
			for (unsigned int i = 0; i < new_miniblock_list->total_elements - 1;
				 i++)
				minib_current = minib_current->next;
			ll_add_nth_node(old_miniblock_list, 0, minib_current->data);
			node_t *removed_node = ll_remove_nth_node(
				new_miniblock_list, new_miniblock_list->total_elements);
			if (removed_node->data) {
				free(removed_node->data);
				removed_node->data = NULL;
			}
			free(removed_node);
			removed_node = NULL;
		}
	}
	// free de block vechi
	ll_free((list_t **)&new_block->miniblock_list);
}

void alloc_block(arena_t *arena, const uint64_t address, const uint64_t size)
{
	if (!arena) {
		printf("Arena was not allocated.\n");
		return;
	}
	uint64_t end_address_new = address + size - 1;

	if (address + 1 > arena->arena_size) {
		printf("The allocated address is outside the size of arena\n");
		return;
	}
	if (end_address_new + 1 > arena->arena_size) {
		printf("The end address is past the size of the arena\n");
		return;
	}

	// New block creation
	block_t *new_block = malloc(sizeof(block_t));
	new_block->size = size;
	new_block->start_address = address;
	// void* = list_t* ???
	new_block->miniblock_list = ll_create(sizeof(miniblock_t));

	// list of miniblocks
	miniblock_t *miniblock_l = malloc(sizeof(miniblock_t));
	miniblock_l->size = size;
	miniblock_l->start_address = address;
	miniblock_l->perm = 6;	// RW- ???
	ll_add_nth_node(new_block->miniblock_list, 0,
					miniblock_l);  // free old_data ???
	free(miniblock_l);			   //???

	// Add to the arena
	if (arena->alloc_list->total_elements == 0) {
		ll_add_nth_node(arena->alloc_list, 0, new_block);
		free(new_block);
		return;
	}

	// Before the first element
	node_t *first = arena->alloc_list->head;
	block_t *b_first = (block_t *)first->data;
	if (end_address_new < b_first->start_address) {
		if (end_address_new == b_first->start_address - 1) {
			// Connect to the old(next) block. (NEW, OLD)
			concat_block(b_first, new_block, 1);
		} else {
			ll_add_nth_node(arena->alloc_list, 0, new_block);
		}
		free(new_block);
		return;
	}

	// After the last element
	node_t *last = arena->alloc_list->head;
	for (unsigned int i = 0; i < arena->alloc_list->total_elements - 1; i++)
		last = last->next;

	block_t *b_last = (block_t *)last->data;
	if (address > b_last->start_address + b_last->size - 1) {
		if (address == b_last->start_address + b_last->size) {
			// Connect to the old(previous) block. (OLD, NEW)
			concat_block(b_last, new_block, -1);  // adresele ca parametri?
		} else {
			ll_add_nth_node(arena->alloc_list,
							arena->alloc_list->total_elements, new_block);
		}
		free(new_block);
		return;
	}
	// daca e last-ul fix inainte?
	if (arena->alloc_list->total_elements >= 2) {
		node_t *previous = first;
		node_t *next_n = first->next;
		int k = 1;

		// The new block is somewhere between two blocks
		do {
			block_t *prev_b = (block_t *)previous->data;
			uint64_t prev_end = prev_b->start_address + prev_b->size - 1;
			block_t *next_b = (block_t *)next_n->data;
			uint64_t next_start = next_b->start_address;
			if (prev_end < new_block->start_address &&
				end_address_new < next_start) {
				if ((prev_end == new_block->start_address - 1) &&
					(end_address_new == next_start - 1)) {
					// Connect to the old(previous & next) blocks. TODO
					concat_block(prev_b, new_block, -1);  //(OLD, NEW)
					concat_block(prev_b, next_b, -1);
					// dealocam block-ul 3;
					node_t *removed_node =
						ll_remove_nth_node(arena->alloc_list, k);
					if (removed_node->data) {
						free(removed_node->data);
						removed_node->data = NULL;
					}
					free(removed_node);
					removed_node = NULL;

					//  connect the existing created one(prev+new) with the
					//  existing next
				} else {
					if (prev_end == new_block->start_address - 1) {
						// Connect to the old(previous) block. (OLD, NEW)
						concat_block(prev_b, new_block, -1);
					} else {
						if (end_address_new == next_start - 1) {
							// Connect to the old(next) block. (NEW, OLD)
							concat_block(next_b, new_block, 1);
						} else {
							ll_add_nth_node(arena->alloc_list, k, new_block);
						}
					}
				}
				free(new_block);
				return;
			}
			previous = previous->next;
			next_n = next_n->next;
			k++;
		} while (next_n->next);
	}
	// Reach error only if a free zone is not found.
	printf("This zone was already allocated.\n");
	ll_free(new_block->miniblock_list);
	free(new_block);
}

void free_block(arena_t *arena, const uint64_t address)
{
	if (!arena ||
		arena->alloc_list->total_elements == 0) {  // Nu avem ce sa freeuim
		printf("Invalid address for free.\n");
		return;
	}

	node_t *curr = arena->alloc_list->head;
	for (unsigned int i = 0; i < arena->alloc_list->total_elements; i++) {
		block_t *curr_block = (block_t *)curr->data;
		uint64_t curr_ending = curr_block->start_address + curr_block->size - 1;
		if (curr_block->start_address <= address &&
			address <= curr_ending) {  // gasim in ce block e
			list_t *minib_list = (list_t *)curr_block->miniblock_list;
			node_t *minib_curr_node = minib_list->head;

			for (unsigned int j = 0; j < minib_list->total_elements;
				 j++) {	 // gasire miniblock
				miniblock_t *minib_curr = (miniblock_t *)minib_curr_node->data;

				if (minib_curr->start_address == address) {	 // gasit
					// printf("MB gasit:%lX %lX\n", minib_curr->start_address,
					//	   minib_curr->size);
					if (minib_list->total_elements == 1) {
						// list de mini are un sg elem
						// dealloc tot block ul ca are doar un element
						ll_free(&minib_list);
						ll_remove_nth_node(arena->alloc_list, i);
						free(curr->data);
						curr->data = NULL;
						free(curr);
						curr = NULL;
						return;
					}

					// inceput sau end of block;

					if (j == 0 || j == minib_list->total_elements - 1) {
						if (j == 0)
							curr_block->start_address += minib_curr->size;

						curr_block->size -= minib_curr->size;

						node_t *removed_node =
							ll_remove_nth_node(minib_list, j);
						if (removed_node->data) {
							free(removed_node->data);
							removed_node->data = NULL;
						}
						free(removed_node);
						removed_node = NULL;

						return;
					}

					// in mijloc;  nr miniblock = j, nr block = i

					// not lose the next
					minib_curr_node = minib_curr_node->next;

					// free miniblock to be freed
					curr_block->size -= minib_curr->size;
					node_t *removed_node = ll_remove_nth_node(minib_list, j);
					if (removed_node->data) {
						free(removed_node->data);
						removed_node->data = NULL;
					}
					free(removed_node);
					removed_node = NULL;

					// create new block
					block_t *new_block = malloc(sizeof(block_t));
					new_block->size = 0;

					// first miniblock
					miniblock_t *new_minib =
						(miniblock_t *)minib_curr_node->data;
					new_block->start_address = new_minib->start_address;

					new_block->miniblock_list = ll_create(sizeof(miniblock_t));
					list_t *new_minib_list =
						(list_t *)new_block->miniblock_list;

					node_t *next = minib_curr_node->next;
					while (minib_list->total_elements - j) {
						ll_add_nth_node(new_minib_list,
										new_minib_list->total_elements,
										minib_curr_node->data);
						new_minib = (miniblock_t *)minib_curr_node->data;
						new_block->size += new_minib->size;
						// printf("Telems:%d\n", minib_list->total_elements);
						removed_node = ll_remove_nth_node(minib_list, j);
						if (removed_node->data) {
							free(removed_node->data);
							removed_node->data = NULL;
						}
						free(removed_node);
						removed_node = NULL;

						if (minib_list->total_elements - j == 0)
							break;

						minib_curr_node = next;
						if (next->next)
							next = next->next;
					}

					curr_block->size -= new_block->size;

					ll_add_nth_node(arena->alloc_list, i + 1, new_block);

					return;
				}

				minib_curr_node = minib_curr_node->next;
			}
		}
		curr = curr->next;
	}

	printf("Invalid address for free.\n");
}

// void read(arena_t *arena, uint64_t address, uint64_t size)
// {
// }

// void write(arena_t *arena, const uint64_t address, const uint64_t size,
// 		   int8_t *data)
// {
// }

void pmap(const arena_t *arena)
{
	// Total memory: 0x10000 bytes
	// Free memory: 0xFFE2 bytes
	// Number of allocated blocks: 3
	// Number of allocated miniblocks: 3
	if (!arena) {
		return;
	}
	printf("Total memory: 0x%lX bytes\n", arena->arena_size);

	uint64_t free_memory = arena->arena_size;
	uint64_t nr_miniblocks = 0;

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

	// Block 1 begin
	// Zone: 0x1000 - 0x100A
	// Block 1 end
	curr_node_b = arena->alloc_list->head;
	for (unsigned int i = 0; i < arena->alloc_list->total_elements; i++) {
		block_t *curr_block = (block_t *)curr_node_b->data;
		list_t *miniblock_list = (list_t *)curr_block->miniblock_list;
		printf("\nBlock %d begin\n", i + 1);
		printf("Zone: 0x%lX - 0x%lX\n", curr_block->start_address,
			   curr_block->start_address + curr_block->size);

		// Miniblock 1:        0x1000      -       0x100A      | RW-
		node_t *curr_node_minib = miniblock_list->head;
		for (unsigned int j = 0; j < miniblock_list->total_elements; j++) {
			miniblock_t *curr_miniblock = (miniblock_t *)curr_node_minib->data;

			printf("Miniblock %d:\t\t0x%lX\t\t-\t\t0x%lX\t\t| RW-\n", j + 1,
				   curr_miniblock->start_address,
				   curr_miniblock->start_address + curr_miniblock->size);

			// RW- din octal TODO
			curr_node_minib = curr_node_minib->next;
		}

		printf("Block %d end\n", i + 1);
		curr_node_b = curr_node_b->next;
	}
}

// void mprotect(arena_t *arena, uint64_t address, int8_t *permission)
// {
// }

list_t *ll_create(unsigned int data_size)
{
	list_t *ll;

	ll = malloc(sizeof(*ll));

	ll->head = NULL;
	ll->data_size = data_size;
	ll->total_elements = 0;

	return ll;
}

void ll_add_nth_node(list_t *list, unsigned int n, const void *new_data)
{
	node_t *prev, *curr;
	node_t *new_node;

	if (!list) {
		return;
	}

	/* n >= list->size inseamna adaugarea unui nou nod la finalul listei. */
	if (n > list->total_elements) {
		n = list->total_elements;
	}

	curr = list->head;
	prev = NULL;
	while (n > 0) {
		prev = curr;
		curr = curr->next;
		--n;
	}

	new_node = malloc(sizeof(*new_node));
	new_node->data = malloc(list->data_size);
	memcpy(new_node->data, new_data, list->data_size);

	new_node->next = curr;
	if (prev == NULL) {
		/* Adica n == 0. */
		list->head = new_node;
	} else {
		prev->next = new_node;
	}

	list->total_elements++;
}

node_t *ll_remove_nth_node(list_t *list, unsigned int n)
{
	node_t *prev, *curr;

	if (!list || !list->head) {
		return NULL;
	}

	/* n >= list->size - 1 inseamna eliminarea nodului de la finalul listei.
	 */
	if (n > list->total_elements - 1) {
		n = list->total_elements - 1;
	}

	curr = list->head;
	prev = NULL;
	while (n > 0) {
		prev = curr;
		curr = curr->next;
		--n;
	}

	if (prev == NULL) {
		/* Adica n == 0. */
		list->head = curr->next;
	} else {
		prev->next = curr->next;
	}

	list->total_elements--;

	return curr;
}

unsigned int ll_get_size(list_t *list)
{
	if (!list) {
		return 0;
	}

	return list->total_elements;
}

void ll_free(list_t **pp_list)
{
	node_t *currNode;

	if (!pp_list || !*pp_list) {
		return;
	}

	while (ll_get_size(*pp_list) > 0) {
		currNode = ll_remove_nth_node(*pp_list, 0);
		free(currNode->data);
		currNode->data = NULL;
		free(currNode);
		currNode = NULL;
	}

	free(*pp_list);
	*pp_list = NULL;
}