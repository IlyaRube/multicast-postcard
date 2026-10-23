#include "../src/reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const uint8_t TEST_VIDEO[] = {1, 2, 3, 4};

static int init_valid_metadata(postal_metadata_t *metadata)
{
    const uint8_t multicast_addr[4] = {224, 1, 1, 1};
    postal_metadata_status_t status = POSTAL_METADATA_OK;

    status = postal_metadata_init(
        metadata,
        multicast_addr,
        6000,
        0,
        sizeof(TEST_VIDEO),
        1000
    );

    if (status != POSTAL_METADATA_OK)
    {
        fprintf(stderr, "metadata init failed: %d\n", status);
        return 1;
    }

    return 0;
}

static int create_test_file(
    char path[],
    const postal_metadata_t *metadata
)
{
    int fd = -1;
    ssize_t bytes_written = 0;

    fd = mkstemp(path);
    if (fd < 0)
    {
        perror("mkstemp");
        return 1;
    }

    bytes_written = write(fd, TEST_VIDEO, sizeof(TEST_VIDEO));
    if (bytes_written != (ssize_t)sizeof(TEST_VIDEO))
    {
        fprintf(stderr, "failed to write test video\n");
        close(fd);
        unlink(path);
        return 1;
    }

    bytes_written = write(fd, metadata, sizeof(*metadata));
    if (bytes_written != (ssize_t)sizeof(*metadata))
    {
        fprintf(stderr, "failed to write test metadata\n");
        close(fd);
        unlink(path);
        return 1;
    }

    if (close(fd) < 0)
    {
        perror("close");
        unlink(path);
        return 1;
    }

    return 0;
}

static int check_metadata_error(
    const char *name,
    const postal_metadata_t *metadata,
    postal_metadata_status_t expected
)
{
    char path[] = "/tmp/postal_reader_metadata_XXXXXX";
    postal_card card = {0};
    int status = POSTAL_READER_OK;

    if (create_test_file(path, metadata) != 0)
    {
        return 1;
    }

    status = postal_card_open_file(&card, path);
    unlink(path);

    if (status != POSTAL_READER_ERR_METADATA ||
        card.meta_status != expected)
    {
        fprintf(
            stderr,
            "%s: reader status %d, metadata status %d\n",
            name,
            status,
            card.meta_status
        );
        return 1;
    }

    return 0;
}

static int test_valid_metadata(void)
{
    char path[] = "/tmp/postal_reader_valid_XXXXXX";
    postal_metadata_t metadata = {0};
    postal_card card = {0};
    int status = POSTAL_READER_OK;
    int failed = 0;

    if (init_valid_metadata(&metadata) != 0)
    {
        return 1;
    }

    if (create_test_file(path, &metadata) != 0)
    {
        return 1;
    }

    status = postal_card_open_file(&card, path);
    if (status != POSTAL_READER_OK)
    {
        fprintf(stderr, "valid metadata: reader returned %d\n", status);
        failed = 1;
    }
    else if (memcmp(&card.meta, &metadata, sizeof(metadata)) != 0)
    {
        fprintf(stderr, "valid metadata: read structure differs\n");
        failed = 1;
    }

    postal_card_close(&card);
    unlink(path);

    return failed;
}

static int test_reader_errors(void)
{
    char empty_path[] = "/tmp/postal_reader_empty_XXXXXX";
    postal_card card = {0};
    int fd = -1;
    int status = POSTAL_READER_OK;
    int failed = 0;

    status = postal_card_open_file(NULL, "/invalid/postcard/path");
    if (status != POSTAL_READER_ERR_ARG)
    {
        fprintf(stderr, "null card: wrong status\n");
        ++failed;
    }

    status = postal_card_open_file(&card, NULL);
    if (status != POSTAL_READER_ERR_ARG)
    {
        fprintf(stderr, "null path: wrong status\n");
        ++failed;
    }

    status = postal_card_open_file(&card, "/invalid/postcard/path");
    if (status != POSTAL_READER_ERR_OPEN)
    {
        fprintf(stderr, "missing file: wrong status\n");
        ++failed;
    }

    fd = mkstemp(empty_path);
    if (fd < 0)
    {
        perror("mkstemp");
        return failed + 1;
    }

    if (close(fd) < 0)
    {
        perror("close");
        unlink(empty_path);
        return failed + 1;
    }

    status = postal_card_open_file(&card, empty_path);
    unlink(empty_path);

    if (status != POSTAL_READER_ERR_STAT)
    {
        fprintf(stderr, "empty file: wrong status\n");
        ++failed;
    }

    return failed;
}

static int test_metadata_errors(void)
{
    postal_metadata_t metadata = {0};
    int failed = 0;

    if (init_valid_metadata(&metadata) != 0)
    {
        return 1;
    }

    metadata.magic = 0;

    failed += check_metadata_error(
        "invalid magic",
        &metadata,
        POSTAL_METADATA_ERROR_MAGIC
    );

    if (init_valid_metadata(&metadata) != 0)
    {
        return failed + 1;
    }

    metadata.version = POSTAL_METADATA_VERSION + 1;

    failed += check_metadata_error(
        "invalid version",
        &metadata,
        POSTAL_METADATA_ERROR_VERSION
    );

    if (init_valid_metadata(&metadata) != 0)
    {
        return failed + 1;
    }

    metadata.metadata_size = 0;

    failed += check_metadata_error(
        "invalid metadata size",
        &metadata,
        POSTAL_METADATA_ERROR_SIZE
    );

    if (init_valid_metadata(&metadata) != 0)
    {
        return failed + 1;
    }

    metadata.multicast_addr[0] = 192;

    failed += check_metadata_error(
        "invalid multicast address",
        &metadata,
        POSTAL_METADATA_ERROR_MULTICAST_ADDRESS
    );

    if (init_valid_metadata(&metadata) != 0)
    {
        return failed + 1;
    }

    metadata.udp_port = 0;

    failed += check_metadata_error(
        "invalid port",
        &metadata,
        POSTAL_METADATA_ERROR_PORT
    );

    if (init_valid_metadata(&metadata) != 0)
    {
        return failed + 1;
    }

    metadata.bitrate = 0;

    failed += check_metadata_error(
        "invalid bitrate",
        &metadata,
        POSTAL_METADATA_ERROR_BITRATE
    );

    return failed;
}

static int test_file_too_small(void)
{
    char path[] = "/tmp/postal_reader_short_XXXXXX";
    postal_card card = {0};
    int fd = -1;
    int status = POSTAL_READER_OK;
    ssize_t bytes_written = 0;

    fd = mkstemp(path);
    if (fd < 0)
    {
        perror("mkstemp");
        return 1;
    }

    bytes_written = write(fd, TEST_VIDEO, sizeof(TEST_VIDEO));
    if (bytes_written != (ssize_t)sizeof(TEST_VIDEO))
    {
        fprintf(stderr, "failed to write short file\n");
        close(fd);
        unlink(path);
        return 1;
    }

    if (close(fd) < 0)
    {
        perror("close");
        unlink(path);
        return 1;
    }

    status = postal_card_open_file(&card, path);
    unlink(path);

    if (status != POSTAL_READER_ERR_METADATA ||
        card.meta_status != POSTAL_METADATA_ERROR_FILE_TOO_SMALL)
    {
        fprintf(stderr, "short file: wrong status\n");
        return 1;
    }

    return 0;
}

static int test_video_out_of_bounds(void)
{
    char path[] = "/tmp/postal_reader_range_XXXXXX";
    postal_metadata_t metadata = {0};
    postal_card card = {0};
    int status = POSTAL_READER_OK;

    if (init_valid_metadata(&metadata) != 0)
    {
        return 1;
    }

    metadata.video_size = sizeof(TEST_VIDEO) + 1;

    if (create_test_file(path, &metadata) != 0)
    {
        return 1;
    }

    status = postal_card_open_file(&card, path);
    unlink(path);

    if (status != POSTAL_READER_ERR_METADATA ||
        card.meta_status != POSTAL_METADATA_ERROR_VIDEO_RANGE)
    {
        fprintf(stderr, "video range: wrong status\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    int failed = 0;

    failed += test_valid_metadata();
    failed += test_reader_errors();
    failed += test_metadata_errors();
    failed += test_file_too_small();
    failed += test_video_out_of_bounds();

    if (failed != 0)
    {
        fprintf(stderr, "%d test(s) failed\n", failed);
        return 1;
    }

    printf("All reader tests passed\n");

    return 0;
}