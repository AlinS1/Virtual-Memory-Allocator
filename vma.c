#include "vma.h"

arena_t *alloc_arena(const uint64_t size)
{
	arena_t *arena = (arena_t *)malloc(sizeof(arena_t));
	DIE(!arena, "malloc failed");
	arena->arena_size = size;
	arena->alloc_list = ll_create(sizeof(block_t));

	return arena;
}

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

void dealloc_arena(arena_t *arena)
{
	while (arena->alloc_list->total_elements) {
		node_t *curr = arena->alloc_list->head;
		block_t *curr_block = (block_t *)curr->data;
		list_t *curr_mb_list = (list_t *)curr_block->miniblock_list;
		// pt fiecare minib - free(rw->buffer)
		free_buffers(curr_mb_list);
		ll_free(&curr_mb_list);
		free_node(arena->alloc_list, 0);
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

			free_node(new_miniblock_list, 0);
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
			free_node(new_miniblock_list, new_miniblock_list->total_elements);
		}
	}
	// free de block vechi
	ll_free((list_t **)&new_block->miniblock_list);
}

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

void cases_of_alloc_block(arena_t *arena, int k, block_t *prev_b,
						  uint64_t prev_end, block_t *next_b,
						  uint64_t next_start, block_t *new_block,
						  uint64_t end_address_new)
{
	if ((prev_end == new_block->start_address - 1) &&
		(end_address_new == next_start - 1)) {
		// Connect to the previous block, then the sum to the next.
		concat_block(prev_b, new_block, -1);  //(OLD, NEW)
		concat_block(prev_b, next_b, -1);
		free_node(arena->alloc_list, k);  // free block 3;
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
}

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
	// free old_data ???
	free(miniblock_l);
	return new_block;
}

void alloc_block(arena_t *arena, const uint64_t address, const uint64_t size)
{
	uint64_t end_address_new = address + size - 1;

	if (!alloc_block_errors(arena, address, end_address_new))
		return;

	// New block creation
	block_t *new_block = init_new_block(address, size);

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

	if (arena->alloc_list->total_elements >= 2) {
		node_t *previous = first;
		node_t *next_n = first->next;
		int k = 1;

		do {  // The new block is somewhere between two blocks
			block_t *prev_b = (block_t *)previous->data;
			uint64_t prev_end = prev_b->start_address + prev_b->size - 1;
			block_t *next_b = (block_t *)next_n->data;
			uint64_t next_start = next_b->start_address;

			if (prev_end < new_block->start_address &&
				end_address_new < next_start) {
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

void free_block(arena_t *arena, const uint64_t address)
{
	if (!arena || arena->alloc_list->total_elements == 0) {
		// Nu avem ce sa freeuim
		printf("Invalid address for free.\n");
		return;
	}

	node_t *curr = arena->alloc_list->head;
	for (unsigned int i = 0; i < arena->alloc_list->total_elements; i++) {
		block_t *curr_block = (block_t *)curr->data;
		uint64_t curr_ending = curr_block->start_address + curr_block->size - 1;
		if (curr_block->start_address <= address && address <= curr_ending) {
			// gasim in ce block e
			list_t *minib_list = (list_t *)curr_block->miniblock_list;
			node_t *minib_curr_node = minib_list->head;

			for (unsigned int j = 0; j < minib_list->total_elements; j++) {
				// gasire miniblock
				miniblock_t *minib_curr = (miniblock_t *)minib_curr_node->data;

				if (minib_curr->start_address == address) {	 // gasit
					if (minib_list->total_elements == 1) {
						// dealloc tot block ul ca list de mini are doar un elem
						ll_free(&minib_list);
						free_node(arena->alloc_list, i);
						return;
					}
					if (j == 0 || j == minib_list->total_elements - 1) {
						// inceput sau end of block
						if (j == 0)
							curr_block->start_address += minib_curr->size;
						curr_block->size -= minib_curr->size;
						free_node(minib_list, j);
						return;
					}

					// in mijloc;  nr miniblock = j, nr block = i

					// not lose the next
					minib_curr_node = minib_curr_node->next;

					// free miniblock to be freed
					curr_block->size -= minib_curr->size;
					free_node(minib_list, j);

					// create new block
					block_t *new_block = malloc(sizeof(block_t));
					DIE(!new_block, "malloc failed");
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
						free_node(minib_list, j);

						if (minib_list->total_elements - j == 0)
							break;
						minib_curr_node = next;
						if (next->next)
							next = next->next;
					}

					curr_block->size -= new_block->size;
					ll_add_nth_node(arena->alloc_list, i + 1, new_block);
					free(new_block);
					return;
				}
				minib_curr_node = minib_curr_node->next;
			}
		}
		curr = curr->next;
	}
	printf("Invalid address for free.\n");
}

void read(arena_t *arena, uint64_t address, uint64_t size)
{
	if (!arena || arena->alloc_list->total_elements == 0) {
		printf("Invalid address for read.\n");
		return;
	}

	block_t *curr_block = find_block(arena, address);
	if (!curr_block) {
		printf("Invalid address for read.\n");
		return;
	}

	list_t *minib_list = (list_t *)curr_block->miniblock_list;
	node_t *minib_curr_node = minib_list->head;	 // nod de minib
	uint64_t end_block_curr = curr_block->start_address + curr_block->size - 1;

	for (unsigned int j = 0; j < minib_list->total_elements; j++) {
		// gasire miniblock
		miniblock_t *minib_curr =
			(miniblock_t *)minib_curr_node->data;  // minib
		unsigned int end_minib =
			minib_curr->start_address + minib_curr->size - 1;
		if (minib_curr->start_address <= address &&
			address <= end_minib) {	 // add poate fi in mijlocul unui mb
			// gasit miniblock

			if (!check_permission(minib_list, minib_curr_node, size, j, 4)) {
				printf("Invalid permissions for read.\n");
				return;
			}

			if (end_block_curr - address + 1 < size) {
				printf("Warning: size was bigger than the block size.");
				printf(" Reading %ld characters.\n",
					   end_block_curr - address + 1);
			}

			unsigned int idx_total_read = 0;

			if (minib_curr->start_address != address) {
				unsigned int start = minib_curr->start_address;
				unsigned int which_byte = 0;
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

			for (unsigned int l = j; l < minib_list->total_elements; l++) {
				unsigned int idx = 0;
				char *data = (char *)minib_curr->rw_buffer;

				while (idx < minib_curr->size && idx_total_read < size) {
					printf("%c", data[idx]);
					idx++;
					idx_total_read++;
				}

				if (l == minib_list->total_elements - 1)
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

void write(arena_t *arena, const uint64_t address, const uint64_t size,
		   int8_t *data)
{
	char *data_string = malloc(sizeof(char) * size);
	DIE(!data_string, "malloc failed");
	if (!data)
		data_string[0] = '\n';

	if (data) {
		for (unsigned int i = 0; i < strlen((char *)data); i++)
			data_string[i] = (char)data[i];

		if (strlen(data_string) == size - 1)
			// because strtok doesn't get the last enter
			data_string[strlen(data_string)] = '\n';
	}

	if (strlen(data_string) < size) {
		if (data_string[0] != '\n')
			data_string[strlen(data_string)] = '\n';
		while (strlen(data_string) < size)
			fscanf(stdin, "%c", &data_string[strlen(data_string)]);
		//??? Acum sau mai incolo???
		char last_enter;
		fscanf(stdin, "%c", &last_enter);
	}

	if (!arena || arena->alloc_list->total_elements == 0) {
		printf("Invalid address for write.\n");
		return;
	}

	block_t *curr_block = find_block(arena, address);
	if (!curr_block) {
		printf("Invalid address for write.\n");
		return;
	}

	list_t *minib_list = (list_t *)curr_block->miniblock_list;
	node_t *minib_curr_node = minib_list->head;	 // nod de minib
	uint64_t end_block_curr = curr_block->start_address + curr_block->size - 1;

	for (unsigned int j = 0; j < minib_list->total_elements; j++) {
		// gasire miniblock
		miniblock_t *minib_curr = (miniblock_t *)minib_curr_node->data;
		uint64_t end_mb_curr = minib_curr->start_address + minib_curr->size - 1;
		if (minib_curr->start_address <= address && address <= end_mb_curr) {
			if (!check_permission(minib_list, minib_curr_node, size, j, 2)) {
				printf("Invalid permissions for write.\n");
				return;
			}

			if (end_block_curr - minib_curr->start_address + 1 < size) {
				printf("Warning: size was bigger than the block size.");
				printf(" Writing %ld characters.\n",
					   end_block_curr - address + 1);
			}

			unsigned int idx_data = 0;

			for (unsigned int l = j; l < minib_list->total_elements; l++) {
				unsigned int idx_minib = 0;
				minib_curr->rw_buffer = malloc(minib_curr->size * sizeof(char));
				DIE(!minib_curr->rw_buffer, "malloc failed");

				while (idx_minib < minib_curr->size &&
					   idx_data < strlen(data_string)) {
					if (strlen(data_string) - idx_data < minib_curr->size) {
						memcpy(minib_curr->rw_buffer, data_string + idx_data,
							   strlen(data_string) - idx_data);
						idx_data += strlen(data_string) - idx_data;
						idx_minib += strlen(data_string) - idx_data;
					} else {
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

void pmap(const arena_t *arena)
{
	if (!arena)
		return;

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

void mprotect(arena_t *arena, uint64_t address, int8_t *permission)
{
	int8_t perm = find_permission(permission);
	block_t *curr_block = find_block(arena, address);
	if (!curr_block) {
		printf("Invalid address for mprotect.\n");
		return;
	}

	list_t *minib_list = (list_t *)curr_block->miniblock_list;
	node_t *minib_curr_node = minib_list->head;	 // nod de minib

	for (unsigned int j = 0; j < minib_list->total_elements; j++) {
		// gasire miniblock
		miniblock_t *minib_curr = (miniblock_t *)minib_curr_node->data;
		// minib

		if (minib_curr->start_address == address) {
			minib_curr->perm = perm;
			return;
			// printf("Perm:%d\n", minib_curr->perm);
		}
		minib_curr_node = minib_curr_node->next;
	}

	printf("Invalid address for mprotect.\n");
}

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

int find_permission(int8_t *permission)
{
	int final_permission = 0;
	char *data = (char *)permission;
	char delim[] = "|\n ";
	char *perm = strtok(data, delim);
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

block_t *find_block(arena_t *arena, const uint64_t address)
{
	node_t *curr = arena->alloc_list->head;	 // nod de block
	for (unsigned int i = 0; i < arena->alloc_list->total_elements; i++) {
		block_t *curr_block = (block_t *)curr->data;  // block
		uint64_t curr_ending = curr_block->start_address + curr_block->size - 1;
		if (curr_block->start_address <= address && address <= curr_ending) {
			// gasit block
			return curr_block;
		}
		curr = curr->next;
	}
	return NULL;
}

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
		// printf("perm:%d\n", minib_curr->perm);
		if ((minib_curr->perm & mask) == 0)
			return 0;  // nu are perm peste tot

		if (l == minib_list->total_elements - 1)
			break;
		minib_curr_node = minib_curr_node->next;
		minib_curr = (miniblock_t *)minib_curr_node->data;
	}

	return 1;  // ok
}

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

// ===========
// LINKED-LIST
// ===========

list_t *ll_create(unsigned int data_size)
{
	list_t *ll;

	ll = malloc(sizeof(*ll));
	DIE(!ll, "malloc failed");

	ll->head = NULL;
	ll->data_size = data_size;
	ll->total_elements = 0;

	return ll;
}

void ll_add_nth_node(list_t *list, unsigned int n, const void *new_data)
{
	node_t *prev, *curr;
	node_t *new_node;

	if (!list)
		return;

	/* n >= list->size inseamna adaugarea unui nou nod la finalul
	 * listei. */
	if (n > list->total_elements)
		n = list->total_elements;

	curr = list->head;
	prev = NULL;
	while (n > 0) {
		prev = curr;
		curr = curr->next;
		--n;
	}

	new_node = malloc(sizeof(*new_node));
	DIE(!new_node, "malloc failed");
	new_node->data = malloc(list->data_size);
	DIE(!new_node->data, "malloc failed");
	memcpy(new_node->data, new_data, list->data_size);

	new_node->next = curr;
	if (!prev) /* Adica n == 0. */
		list->head = new_node;
	else
		prev->next = new_node;

	list->total_elements++;
}

node_t *ll_remove_nth_node(list_t *list, unsigned int n)
{
	node_t *prev, *curr;

	if (!list || !list->head)
		return NULL;

	/* n >= list->size - 1 inseamna eliminarea nodului de la finalul
	 * listei.
	 */
	if (n > list->total_elements - 1)
		n = list->total_elements - 1;

	curr = list->head;
	prev = NULL;
	while (n > 0) {
		prev = curr;
		curr = curr->next;
		--n;
	}

	if (!prev) /* Adica n == 0. */
		list->head = curr->next;
	else
		prev->next = curr->next;

	list->total_elements--;

	return curr;
}

unsigned int ll_get_size(list_t *list)
{
	if (!list)
		return 0;

	return list->total_elements;
}

void ll_free(list_t **pp_list)
{
	if (!pp_list || !*pp_list)
		return;

	while (ll_get_size(*pp_list) > 0)
		free_node(*pp_list, 0);

	free(*pp_list);
	*pp_list = NULL;
}

void free_node(list_t *list, int idx)
{
	node_t *removed_node = ll_remove_nth_node(list, idx);
	if (removed_node->data) {
		free(removed_node->data);
		removed_node->data = NULL;
	}
	free(removed_node);
	removed_node = NULL;
}

// ===================
// AUXILIARY FUNCTIONS
// ===================

int command_type(char *command)
{
	// Translate the commands into numbers so we will be able to use switch
	// case.
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

int check_parameters(int type, int nr_param)
{
	int ok = 1;
	if (type == 0)
		ok = 0;
	if (type == 1 && nr_param != 2)	 // ALLOC_ARENA
		ok = 0;
	if (type == 2 && nr_param != 1)	 // DEALLOC_ARENA
		ok = 0;
	if (type == 3 && nr_param != 3)	 // ALLOC_BLOCK
		ok = 0;
	if (type == 4 && nr_param != 2)	 // FREE_BLOCK
		ok = 0;

	if (type == 5 && nr_param != 3)	 // READ
		ok = 0;

	if (type == 6 && nr_param < 3)	// WRITE
		ok = 0;

	if (type == 7 && nr_param != 1)	 // PMAP
		ok = 0;

	if (type == 8 && nr_param < 3)	// MPROTECT
		ok = 0;

	if (ok == 0)
		for (int i = 0; i < nr_param; i++)
			printf("Invalid command. Please try again.\n");
	return ok;
}
