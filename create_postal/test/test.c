#include "../src/args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int check_status(
    const char *name,
    char *video_path,
    char *endpoint,
    postal_args_status_t expected
)
{
    char *argv[] = {
        "create_postal",
        video_path,
        endpoint,
        NULL,
    };
    postal_args_t args = {0};
    postal_args_status_t status = POSTAL_ARGS_OK;

    status = postal_args_parse(3, argv, &args);

    if (status != expected)
    {
        fprintf(
            stderr,
            "%s: expected status %d, got %d\n",
            name,
            expected,
            status
        );
        return 1;
    }

    return 0;
}

static int check_values(
    const char *name,
    char *video_path,
    char *endpoint,
    const uint8_t expected_addr[4],
    uint16_t expected_port
)
{
    char *argv[] = {
        "create_postal",
        video_path,
        endpoint,
        NULL,
    };
    postal_args_t args = {0};
    postal_args_status_t status = POSTAL_ARGS_OK;

    status = postal_args_parse(3, argv, &args);

    if (status != POSTAL_ARGS_OK)
    {
        fprintf(stderr, "%s: parse failed with status %d\n", name, status);
        return 1;
    }

    if (strcmp(args.video_path, video_path) != 0 ||
        memcmp(args.multicast_addr, expected_addr, 4) != 0 ||
        args.udp_port != expected_port)
    {
        fprintf(stderr, "%s: incorrect parsed values\n", name);
        return 1;
    }

    return 0;
}

static int test_argument_count(char *video_path)
{
    char *too_few_argv[] = {
        "create_postal",
        video_path,
        NULL,
    };
    char *too_many_argv[] = {
        "create_postal",
        video_path,
        "224.1.1.1",
        "6000",
        NULL,
    };
    postal_args_t args = {0};
    postal_args_status_t status = POSTAL_ARGS_OK;
    int failed = 0;

    status = postal_args_parse(2, too_few_argv, &args);
    if (status != POSTAL_ARGS_ERROR_COUNT)
    {
        fprintf(stderr, "too few arguments: wrong status\n");
        ++failed;
    }

    status = postal_args_parse(4, too_many_argv, &args);
    if (status != POSTAL_ARGS_ERROR_COUNT)
    {
        fprintf(stderr, "too many arguments: wrong status\n");
        ++failed;
    }

    return failed;
}

static int test_unreadable_file(char *video_path)
{
    char *argv[] = {
        "create_postal",
        video_path,
        "224.1.1.1",
        NULL,
    };
    postal_args_t args = {0};
    postal_args_status_t status = POSTAL_ARGS_OK;

    if (chmod(video_path, 0000) < 0)
    {
        perror("chmod");
        return 1;
    }

    if (access(video_path, R_OK) == 0)
    {
        if (chmod(video_path, 0600) < 0)
        {
            perror("chmod");
            return 1;
        }

        fprintf(stderr, "unreadable file: file is still readable\n");
        return 1;
    }

    status = postal_args_parse(3, argv, &args);

    if (chmod(video_path, 0600) < 0)
    {
        perror("chmod");
        return 1;
    }

    if (status != POSTAL_ARGS_ERROR_VIDEO_OPEN)
    {
        fprintf(stderr, "unreadable file: wrong status\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    // Числа используютяс в качестве расширения фа
    char video_path[] = "/tmp/create_postal_video.XXXXXX";
    char missing_path[] = "/invalid/video/path.mp4";
    uint8_t default_addr[4] = {224, 1, 1, 1};
    uint8_t custom_addr[4] = {239, 1, 2, 3};
    uint8_t lower_addr[4] = {224, 0, 0, 0};
    uint8_t upper_addr[4] = {239, 255, 255, 255};
    int video_fd = -1;
    int failed = 0;

    video_fd = mkstemp(video_path);
    if (video_fd < 0)
    {
        perror("mkstemp");
        return 1;
    }
    close(video_fd);

    failed += test_argument_count(video_path);
    failed += test_unreadable_file(video_path);

    failed += check_values(
        "default port",
        video_path,
        "224.1.1.1",
        default_addr,
        POSTAL_DEFAULT_PORT
    );
    failed += check_values(
        "custom port",
        video_path,
        "239.1.2.3:7000",
        custom_addr,
        7000
    );
    failed += check_values(
        "lower multicast boundary",
        video_path,
        "224.0.0.0:1",
        lower_addr,
        1
    );
    failed += check_values(
        "upper multicast boundary",
        video_path,
        "239.255.255.255:65535",
        upper_addr,
        65535
    );

    failed += check_status(
        "missing file",
        missing_path,
        "224.1.1.1",
        POSTAL_ARGS_ERROR_VIDEO_OPEN
    );
    failed += check_status(
        "invalid IPv4",
        video_path,
        "224.1.1.999",
        POSTAL_ARGS_ERROR_ADDRESS_FORMAT
    );
    failed += check_status(
        "below multicast range",
        video_path,
        "223.255.255.255",
        POSTAL_ARGS_ERROR_ADDRESS_NOT_MULTICAST
    );
    failed += check_status(
        "above multicast range",
        video_path,
        "240.0.0.0",
        POSTAL_ARGS_ERROR_ADDRESS_NOT_MULTICAST
    );
    failed += check_status(
        "zero port",
        video_path,
        "224.1.1.1:0",
        POSTAL_ARGS_ERROR_PORT_RANGE
    );
    failed += check_status(
        "port above range",
        video_path,
        "224.1.1.1:65536",
        POSTAL_ARGS_ERROR_PORT_RANGE
    );
    failed += check_status(
        "negative port",
        video_path,
        "224.1.1.1:-1",
        POSTAL_ARGS_ERROR_PORT_FORMAT
    );
    failed += check_status(
        "non-numeric port",
        video_path,
        "224.1.1.1:abc",
        POSTAL_ARGS_ERROR_PORT_FORMAT
    );

    unlink(video_path);

    if (failed != 0)
    {
        fprintf(stderr, "%d test(s) failed\n", failed);
        return 1;
    }

    printf("All argument tests passed\n");

    return 0;
}