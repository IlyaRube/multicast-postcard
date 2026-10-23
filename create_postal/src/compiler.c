#include "compiler.h"

#include <binary.h>
#include <fcntl.h>
#include <io_exact.h>
#include <unistd.h>

compiler_status_t compile(
	const char *output_file,
	int self_fd,
	int mpegts_fd,
	uint8_t ip[4],
	uint16_t port,
	uint64_t bitrate
	)
{
	int output = open(output_file, O_WRONLY | O_CREAT, 0700);
	if (output < 0)
	{
		return COMPILER_ERROR_CREATE_OUTPUT;
	}

	blob_location_t template_location = {0};
	lseek(self_fd, -(off_t)sizeof(template_location), SEEK_END);
	if (read_exact(self_fd, &template_location, sizeof(template_location)) < 0)
	{
		close(output);
		unlink(output_file);
		return COMPILER_ERROR_COPY_TEMPLATE;
	}

	off_t offset = template_location.offset;
	if (copy_file_exact(output, self_fd, &offset, template_location.length) < 0)
	{
		close(output);
		unlink(output_file);
		return COMPILER_ERROR_COPY_TEMPLATE;
	}

	off_t video_offset = lseek(output, 0, SEEK_CUR);
	ssize_t video_size = lseek(mpegts_fd, 0, SEEK_END);
	lseek(mpegts_fd, 0, SEEK_SET);
	if (copy_file_exact(output, mpegts_fd, NULL, video_size) < 0)
	{
		close(output);
		unlink(output_file);
		return COMPILER_ERROR_COPY_MPEGTS;
	}

	postal_metadata_t metadata;
	if (postal_metadata_init(&metadata, ip, port, video_offset, video_size, bitrate) < 0)
	{
		close(output);
		unlink(output_file);
		return COMPILER_INVALID_METADATA;
	}
	if (postal_metadata_write(output, &metadata) < 0)
	{
		close(output);
		unlink(output_file);
		return COMPILER_ERROR_WRITE_METADATA;
	}

	close(output);
	return COMPILER_OK;
}

const char* compiler_status_string(compiler_status_t status)
{
	switch (status)
	{
		case COMPILER_OK:
			return "Success";

		case COMPILER_INVALID_METADATA:
			return "Invalid metadata";

		case COMPILER_ERROR_WRITE_METADATA:
			return "Failed to write metadata";
		
		case COMPILER_ERROR_COPY_MPEGTS:
			return "Failed to copy video stream";
		
		case COMPILER_ERROR_COPY_TEMPLATE:
			return "Failed to copy postal template";
		
		case COMPILER_ERROR_CREATE_OUTPUT:
			return "Failed to create output file";
		
		default:
			return "[COMPILER] Unknown error";
	}
}
