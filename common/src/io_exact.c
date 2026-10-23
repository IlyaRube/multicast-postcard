#include "io_exact.h"

#include <errno.h>
#include <unistd.h>

ssize_t write_exact(
	int fd, 
	const void *buffer, 
	size_t n
)
{
	const char *buffer_ptr = buffer;
	size_t bytes_written = 0;
	while (bytes_written < n)
	{
		ssize_t bytes_written_now = write(
			fd,
			buffer_ptr + bytes_written,
			n - bytes_written
		);

		if (bytes_written_now < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return -1;
		}

		if (bytes_written_now == 0)
		{
			return -1;
		}

		bytes_written += bytes_written_now;
	}
	return (ssize_t)bytes_written;
}

ssize_t read_exact(
	int fd, 
	void *buffer, 
	size_t n
)
{
	char *buffer_ptr = buffer;
	size_t bytes_read = 0;
	while (bytes_read < n)
	{
		ssize_t bytes_read_now = read(
			fd,
			buffer_ptr + bytes_read,
			n - bytes_read
		);

		if (bytes_read_now < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return -1;
		}

		if (bytes_read_now == 0)
		{
			return -1;
		}

		bytes_read += bytes_read_now;
	}
	return (ssize_t)bytes_read;
}

ssize_t pwrite_exact(
	int fd, 
	void *buffer, 
	size_t n, 
	off_t offset
)
{
	char *buffer_ptr = buffer;
	size_t bytes_written = 0;
	while (bytes_written < n)
	{
		ssize_t bytes_written_now = pwrite(
			fd,
			buffer_ptr + bytes_written,
			n - bytes_written,
			offset + bytes_written
		);

		if (bytes_written_now < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return -1;
		}

		if (bytes_written_now == 0)
		{
			return -1;
		}

		bytes_written += bytes_written_now;
	}
	return (ssize_t)bytes_written;
}

ssize_t pread_exact(
	int fd, 
	void *buffer, 
	size_t n, 
	off_t offset
)
{
	char *buffer_ptr = buffer;
	size_t bytes_read = 0;
	while (bytes_read < n)
	{
		ssize_t bytes_read_now = pread(
			fd,
			buffer_ptr + bytes_read,
			n - bytes_read,
			offset + bytes_read
		);

		if (bytes_read_now < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return -1;
		}

		if (bytes_read_now == 0)
		{
			return -1;
		}

		bytes_read += bytes_read_now;
	}
	return (ssize_t)bytes_read;
}

ssize_t copy_file_exact(
	int to,
	int from,
	off_t *read_offset,
	size_t length
	)
{
	size_t bytes_copied = 0;

	while (bytes_copied < length)
	{
		ssize_t bytes_copied_now = sendfile(
			to,
			from,
			read_offset,
			length - bytes_copied
		);

		if (bytes_copied_now < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return -1;
		}

		if (bytes_copied_now == 0)
		{
			return -1;
		}

		bytes_copied += bytes_copied_now;
	}
	return (ssize_t)bytes_copied;
}
