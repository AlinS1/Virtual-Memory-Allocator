#include "vma.h"
#define NMAX_LINE 100

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

int n_parameters(char *line, char *delim)
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

	if (type == 6 && nr_param < 4)	// WRITE
		ok = 0;

	if (type == 7 && nr_param != 1)	 // PMAP
		ok = 0;

	if (type == 8 && nr_param != 3)	 // MPROTECT
		ok = 0;

	if (ok == 0)
		for (int i = 0; i < nr_param; i++)
			printf("Invalid command. Please try again.\n");
	return ok;
}

int main(void)
{
	char *command;
	char line[NMAX_LINE];
	char line_copy[NMAX_LINE];
	char delim[] = " \n";

	arena_t *arena = NULL;
	char *param;
	uint64_t size, address;

	while (fgets(line, NMAX_LINE, stdin)) {
		strcpy(line_copy, line);
		int nr_param = n_parameters(line_copy, delim);

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
					break;

				case 6:	 // WRITE
					break;

				case 7:	 // PMAP
					pmap(arena);
					break;

				case 8:	 // MPROTECT
					break;
			}
	}
	return 0;
}