#include "list.h"
#include "vma.h"
#define NMAX_LINE 100

int main(void)
{
	char *command;
	char line[NMAX_LINE], line_copy[NMAX_LINE];
	char delim[] = "\n ";
	arena_t *arena = NULL;
	char *param;
	uint64_t size, address;

	while (1) {
		fgets(line, NMAX_LINE, stdin);
		if (line[0] == '\n')
			continue;
		strcpy(line_copy, line);

		int nr_param = nr_of_parameters(line_copy, delim);
		command = strtok(line, delim);
		int type = command_type(command);
		int ok = check_parameters(type, nr_param);

		if (ok)
			switch (type) {
				case 1:	 // ALLOC_ARENA
					param = strtok(NULL, delim);
					size = atol(param);
					arena = alloc_arena(size);
					break;

				case 2:	 // DEALLOC_ARENA
					dealloc_arena(arena);
					free(arena);
					arena = NULL;
					exit(0);
					break;

				case 3:	 // ALLOC_BLOCK
					param = strtok(NULL, delim);
					address = atol(param);
					param = strtok(NULL, delim);
					size = atol(param);
					alloc_block(arena, address, size);
					break;

				case 4:	 // FREE_BLOCK
					param = strtok(NULL, delim);
					address = atol(param);
					free_block(arena, address);
					break;

				case 5:	 // READ
					param = strtok(NULL, delim);
					address = atol(param);
					param = strtok(NULL, delim);
					size = atol(param);
					read(arena, address, size);
					break;

				case 6:	 // WRITE
					param = strtok(NULL, delim);
					address = atol(param);
					param = strtok(NULL, delim);
					size = atol(param);
					int8_t *data = (int8_t *)create_string(size);
					write(arena, address, size, data);
					break;

				case 7:	 // PMAP
					pmap(arena);
					break;

				case 8:	 // MPROTECT
					param = strtok(NULL, delim);
					address = atol(param);
					param = strtok(NULL, "\n");
					int8_t *permission = (int8_t *)param;
					mprotect(arena, address, permission);
					break;
			}
	}
	return 0;
}
