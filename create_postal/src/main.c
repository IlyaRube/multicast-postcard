#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include "args.h"
#include "remux.h"
#include "compiler.h"

#define SELF_EXE_PATH "/proc/self/exe"

enum
{
	MAX_FILE_PATH_LENGTH = 1023
};

int main(int argc, char **argv)
{
	postal_args_t args = {0};
	postal_args_status_t args_status = postal_args_parse(argc, argv, &args);
	if (args_status < 0)
	{
		fprintf(stderr, "Error: %s\n", postal_args_status_string(args_status));
		return -1;
	}
	int mpegts_fd = 0;
	int64_t bitrate = 0;
	remux_status_t remux_status = remux(args.video_path, &mpegts_fd, &bitrate);
	if (remux_status < 0)
	{
		fprintf(stderr, "Error: %s\n", remux_status_string(remux_status));
		return -1;
	}

	char copy[MAX_FILE_PATH_LENGTH + 1];
	strncpy(copy, args.video_path, MAX_FILE_PATH_LENGTH);
	char *output_name = basename(copy);
	char *output_name_ptr = &output_name[strlen(output_name) - 1];
	while (output_name_ptr > output_name)
	{
		if (*output_name_ptr == '.')
		{
			*output_name_ptr = '\0';
			break;
		}
		output_name_ptr--;
	}
	if (output_name_ptr == output_name)
	{
		close(mpegts_fd);
		fprintf(stderr, "Error: source file has no extension\n");
		return -1;
	}
	int self_fd = open(SELF_EXE_PATH, O_RDONLY);
	if (self_fd < 0)
	{
		close(mpegts_fd);
		fprintf(stderr, "Error: cannot open: %s\n", SELF_EXE_PATH);
		return -1;
	}

	compiler_status_t compile_status = compile(output_name, self_fd, mpegts_fd, args.multicast_addr, args.udp_port, bitrate);
	if (compile_status < 0)
	{
		close(mpegts_fd);
		close(self_fd);
		fprintf(stderr, "Error: %s\n", compiler_status_string(compile_status));
		return -1;
	}

	return 0;
}
