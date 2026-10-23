
/**
 * @file metadata.c работа с метаданными postcard-файла
 */

#include "metadata.h"
#include "io_exact.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//вспомогательные функции
static int is_multicast_ipv4(const uint8_t address[4]);

static postal_metadata_status_t get_regular_file_size(
    int fd,
    uint64_t *file_size
);

static postal_metadata_status_t validate_video_range(
    const postal_metadata_t *metadata,
    uint64_t metadata_offset
);

/**
 * @brief проверка multicast IPv4 адреса
 */
static int is_multicast_ipv4(const uint8_t address[4])
{
    return address[0] >= 224 && address[0] <= 239;
}

/**
 * @brief получение размера обычного файла
 */
static postal_metadata_status_t get_regular_file_size(
    int fd,
    uint64_t *file_size
)
{
    struct stat file_stat = {0};

    if (file_size == NULL)
    {
        return POSTAL_METADATA_ERROR_NULL;
    }

    if (fstat(fd, &file_stat) < 0)
    {
        return POSTAL_METADATA_ERROR_IO;
    }

    if (!S_ISREG(file_stat.st_mode))
    {
        return POSTAL_METADATA_ERROR_IO;
    }

    if (file_stat.st_size < 0)
    {
        return POSTAL_METADATA_ERROR_IO;
    }

    *file_size = (uint64_t)file_stat.st_size;

    return POSTAL_METADATA_OK;
}

/**
 * @brief проверка расположения MPEG-TS перед блоком метаданных
 */
static postal_metadata_status_t validate_video_range(
    const postal_metadata_t *metadata,
    uint64_t metadata_offset
)
{
    if (metadata->video_offset > metadata_offset)
    {
        return POSTAL_METADATA_ERROR_VIDEO_RANGE;
    }

    if (metadata->video_size >
        metadata_offset - metadata->video_offset)
    {
        return POSTAL_METADATA_ERROR_VIDEO_RANGE;
    }

    return POSTAL_METADATA_OK;
}

postal_metadata_status_t postal_metadata_init(
    postal_metadata_t *metadata,
    const uint8_t multicast_addr[4],
    uint16_t udp_port,
    uint64_t video_offset,
    uint64_t video_size,
    uint64_t bitrate
)
{
    if (metadata == NULL || multicast_addr == NULL)
    {
        return POSTAL_METADATA_ERROR_NULL;
    }

    *metadata = (postal_metadata_t){0};

    metadata->magic = POSTAL_METADATA_MAGIC;
    metadata->version = POSTAL_METADATA_VERSION;
    metadata->metadata_size = (uint16_t)sizeof(*metadata);

    memcpy(
        metadata->multicast_addr,
        multicast_addr,
        sizeof(metadata->multicast_addr)
    );

    metadata->udp_port = udp_port;
    metadata->video_offset = video_offset;
    metadata->video_size = video_size;
    metadata->bitrate = bitrate;

    return postal_metadata_validate(metadata);
}

postal_metadata_status_t postal_metadata_validate(
    const postal_metadata_t *metadata
)
{
    if (metadata == NULL)
    {
        return POSTAL_METADATA_ERROR_NULL;
    }

    if (metadata->magic != POSTAL_METADATA_MAGIC)
    {
        return POSTAL_METADATA_ERROR_MAGIC;
    }

    if (metadata->version != POSTAL_METADATA_VERSION)
    {
        return POSTAL_METADATA_ERROR_VERSION;
    }

    if (metadata->metadata_size != sizeof(*metadata))
    {
        return POSTAL_METADATA_ERROR_SIZE;
    }

    if (!is_multicast_ipv4(metadata->multicast_addr))
    {
        return POSTAL_METADATA_ERROR_MULTICAST_ADDRESS;
    }

    if (metadata->udp_port == 0)
    {
        return POSTAL_METADATA_ERROR_PORT;
    }

    if (metadata->video_size == 0)
    {
        return POSTAL_METADATA_ERROR_VIDEO_RANGE;
    }

    if (metadata->bitrate == 0)
    {
        return POSTAL_METADATA_ERROR_BITRATE;
    }

    return POSTAL_METADATA_OK;
}

postal_metadata_status_t postal_metadata_write(
    int fd,
    const postal_metadata_t *metadata
)
{
    uint64_t current_size = 0;
    postal_metadata_status_t status = POSTAL_METADATA_OK;
    off_t original_end = 0;

    if (metadata == NULL)
    {
        return POSTAL_METADATA_ERROR_NULL;
    }

    status = get_regular_file_size(
        fd,
        &current_size
    );

    if (status != POSTAL_METADATA_OK)
    {
        return status;
    }

    status = validate_video_range(
        metadata,
        current_size
    );

    if (status != POSTAL_METADATA_OK)
    {
        return status;
    }

    original_end = lseek(
        fd,
        0,
        SEEK_END
    );

    if (original_end < 0)
    {
        return POSTAL_METADATA_ERROR_IO;
    }

    ssize_t written = write_exact(
        fd,
        metadata,
        sizeof(*metadata)
    );

    if (written < 0)
    {
        int result = ftruncate(fd, original_end);
        (void)result;

        return POSTAL_METADATA_ERROR_IO;
    }

    return POSTAL_METADATA_OK;
}

postal_metadata_status_t postal_metadata_read(
    int fd,
    postal_metadata_t *metadata
)
{
    uint64_t postcard_size = 0;
    uint64_t metadata_offset = 0;
    postal_metadata_status_t status = POSTAL_METADATA_OK;

    if (metadata == NULL)
    {
        return POSTAL_METADATA_ERROR_NULL;
    }

    status = get_regular_file_size(
        fd,
        &postcard_size
    );

    if (status != POSTAL_METADATA_OK)
    {
        return status;
    }

    if (postcard_size < sizeof(*metadata))
    {
        return POSTAL_METADATA_ERROR_FILE_TOO_SMALL;
    }

    metadata_offset = postcard_size - sizeof(*metadata);

    ssize_t read = pread_exact(
        fd,
        metadata,
        sizeof(*metadata),
        (off_t)metadata_offset
    );

    if (read < 0)
    {
        return POSTAL_METADATA_ERROR_IO;
    }

    status = postal_metadata_validate(metadata);

    if (status != POSTAL_METADATA_OK)
    {
        return status;
    }

    return validate_video_range(
        metadata,
        metadata_offset
    );
}

const char *postal_metadata_status_string(
    postal_metadata_status_t status
)
{
    switch (status)
    {
        case POSTAL_METADATA_OK:
            return "Metadata is valid";

        case POSTAL_METADATA_ERROR_NULL:
            return "Null metadata argument";

        case POSTAL_METADATA_ERROR_IO:
            return "Metadata I/O error";

        case POSTAL_METADATA_ERROR_FILE_TOO_SMALL:
            return "File is too small to contain metadata";

        case POSTAL_METADATA_ERROR_MAGIC:
            return "Invalid metadata magic number";

        case POSTAL_METADATA_ERROR_VERSION:
            return "Unsupported metadata version";

        case POSTAL_METADATA_ERROR_SIZE:
            return "Invalid metadata structure size";

        case POSTAL_METADATA_ERROR_MULTICAST_ADDRESS:
            return "Invalid multicast IPv4 address";

        case POSTAL_METADATA_ERROR_PORT:
            return "Invalid UDP port";

        case POSTAL_METADATA_ERROR_VIDEO_RANGE:
            return "Invalid embedded video location";

        case POSTAL_METADATA_ERROR_BITRATE:
            return "Invalid video bitrate";

        default:
            return "[METADATA] Unknown error";
    }
}
