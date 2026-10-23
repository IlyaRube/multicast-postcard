#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "injector.h"

/**
 * @brief Prints usage manual
 * @param exe Current executable file (argv[0] in main)
 */
static void incorrect_usage(char *exe)
{
	fprintf(stderr, "Invalid arguments. Usage: %s <injectable file> <target file>\n", exe);
}

int main(int argc, char **argv) {
	if (argc != 3)
	{
		incorrect_usage(argv[0]);
		return -1;
	}

	int injectable_file = open(argv[1], O_RDONLY);
	if (injectable_file < 0)
	{
		fprintf(stderr, "open injectable: %s\n", strerror(errno));
		return -1;
	}
	int target_file = open(argv[2], O_WRONLY | O_APPEND);
	if (target_file < 0)
	{
		close(injectable_file);
		fprintf(stderr, "open target: %s\n", strerror(errno));
		return -1;
	}

	injector_status_t error_code = inject(injectable_file, target_file);

	close(injectable_file);
	close(target_file);

	if (error_code < 0)
	{
		fprintf(stderr, "inject: %s\n", injector_status_string(error_code));
		return -1;
	}

	return 0;
}