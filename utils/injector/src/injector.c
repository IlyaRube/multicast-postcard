#include "injector.h"

#include <io_exact.h>

enum
{
	BUFFER_SIZE = 4096
};

const char *injector_status_string(injector_status_t status)
{
	switch (status)
	{
	case INJECTOR_OK:
		return "Injection successful";
	case INJECTOR_ERROR_WRITE:
		return "Failed to write target file";
	case INJECTOR_ERROR_READ:
		return "Failed to read injectable file";
	default:
		return "[INJECTOR] Unknown error";
	}
}

injector_status_t inject(int injectable_file, int target_file)
{
	char buffer[BUFFER_SIZE] = {0};

	lseek(injectable_file, 0, SEEK_SET);
	off_t injected_start = lseek(target_file, 0, SEEK_END);

	ssize_t written_total = 0;

	while (1)
	{
		ssize_t bytes_read = read(injectable_file, buffer, BUFFER_SIZE);
		if (bytes_read < 0)
		{
			int result = ftruncate(target_file, injected_start);
			(void)result;
			return INJECTOR_ERROR_READ;
		}
		if (bytes_read == 0)
		{
			break;
		}
		ssize_t bytes_written = write_exact(target_file, buffer, bytes_read);
		if (bytes_written < 0)
		{
			int result = ftruncate(target_file, injected_start);
			(void)result;
			return INJECTOR_ERROR_WRITE;
		}
		written_total += bytes_written;
	}

	blob_location_t injected_metadata = {
		.offset = injected_start,
		.length = written_total
	};

	ssize_t bytes_written = write_exact(target_file, &injected_metadata, sizeof(injected_metadata));
	if (bytes_written < 0)
	{
		int result = ftruncate(target_file, injected_start);
		(void)result;
		return INJECTOR_ERROR_WRITE;
	}

	return INJECTOR_OK;
}