#include "args.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static postal_args_status_t validate_video_file(
    const char *path
);

static postal_args_status_t parse_address_and_port(
    const char *value,
    uint8_t multicast_addr[4],
    uint16_t *udp_port
);

static postal_args_status_t parse_port(
    const char *value,
    uint16_t *udp_port
);

static postal_args_status_t validate_video_file(
    const char *path
)
{
    size_t length = strlen(path);
    if (length < 2 || path[length - 1] == '.')
    {
        return POSTAL_ARGS_ERROR_VIDEO_NAME;
    }
    const char *ptr = &path[length - 2];
    int name_valid = 0;
    while (*ptr != '/' && *ptr != '\\' && ptr != path)
    {
        if (*ptr == '.')
        {
            name_valid = 1;
            break;
        }
        ptr--;
    }
    if (!name_valid)
    {
        return POSTAL_ARGS_ERROR_VIDEO_NAME;
    }

    struct stat file_stat = {0};
    int fd = open(path, O_RDONLY);

    if (fd < 0)
    {
        return POSTAL_ARGS_ERROR_VIDEO_OPEN;
    }

    if (fstat(fd, &file_stat) < 0)
    {
        close(fd);
        return POSTAL_ARGS_ERROR_VIDEO_OPEN;
    }

    close(fd);

    if (!S_ISREG(file_stat.st_mode))
    {
        return POSTAL_ARGS_ERROR_VIDEO_TYPE;
    }

    return POSTAL_ARGS_OK;
}

static postal_args_status_t parse_port(
    const char *value,
    uint16_t *udp_port
)
{
    unsigned long port = 0;

    if (*value == '\0')
    {
        return POSTAL_ARGS_ERROR_PORT_FORMAT;
    }

    for (size_t i = 0; value[i] != '\0'; ++i)
    {
        if (value[i] < '0' || value[i] > '9')
        {
            return POSTAL_ARGS_ERROR_PORT_FORMAT;
        }

        port = port * 10 + (unsigned long)(value[i] - '0');

        if (port > UINT16_MAX)
        {
            return POSTAL_ARGS_ERROR_PORT_RANGE;
        }
    }

    if (port == 0)
    {
        return POSTAL_ARGS_ERROR_PORT_RANGE;
    }

    *udp_port = (uint16_t)port;

    return POSTAL_ARGS_OK;
}

static postal_args_status_t parse_address_and_port(
    const char *value,
    uint8_t multicast_addr[4],
    uint16_t *udp_port
)
{
    char address_text[INET_ADDRSTRLEN] = {0};
    struct in_addr address = {0};
    const char *separator = strchr(value, ':');
    size_t address_length = 0;
    postal_args_status_t status = POSTAL_ARGS_OK;

    if (separator == NULL)
    {
        address_length = strlen(value);
        *udp_port = POSTAL_DEFAULT_PORT;
    }
    else
    {
        if (strchr(separator + 1, ':') != NULL)
        {
            return POSTAL_ARGS_ERROR_ADDRESS_FORMAT;
        }

        status = parse_port(separator + 1, udp_port);

        if (status != POSTAL_ARGS_OK)
        {
            return status;
        }

        address_length = (size_t)(separator - value);
    }

    if (address_length == 0 ||
        address_length >= sizeof(address_text))
    {
        return POSTAL_ARGS_ERROR_ADDRESS_FORMAT;
    }

    memcpy(address_text, value, address_length);
    address_text[address_length] = '\0';

    if (inet_pton(AF_INET, address_text, &address) != 1)
    {
        return POSTAL_ARGS_ERROR_ADDRESS_FORMAT;
    }

    memcpy(
        multicast_addr,
        &address.s_addr,
        sizeof(address.s_addr)
    );

    if (multicast_addr[0] < 224 ||
        multicast_addr[0] > 239)
    {
        return POSTAL_ARGS_ERROR_ADDRESS_NOT_MULTICAST;
    }

    return POSTAL_ARGS_OK;
}

postal_args_status_t postal_args_parse(
    int argc,
    char *argv[],
    postal_args_t *args
)
{
    postal_args_t parsed = {0};
    postal_args_status_t status = POSTAL_ARGS_OK;

    if (argv == NULL || args == NULL)
    {
        return POSTAL_ARGS_ERROR_NULL;
    }

    if (argc != 3)
    {
        return POSTAL_ARGS_ERROR_COUNT;
    }

    status = validate_video_file(argv[1]);

    if (status != POSTAL_ARGS_OK)
    {
        return status;
    }

    status = parse_address_and_port(
        argv[2],
        parsed.multicast_addr,
        &parsed.udp_port
    );

    if (status != POSTAL_ARGS_OK)
    {
        return status;
    }

    parsed.video_path = argv[1];
    *args = parsed;

    return POSTAL_ARGS_OK;
}

const char *postal_args_status_string(
    postal_args_status_t status
)
{
    switch (status)
    {
        case POSTAL_ARGS_OK:
            return "Success";

        case POSTAL_ARGS_ERROR_NULL:
            return "Null argument";

        case POSTAL_ARGS_ERROR_COUNT:
            return
                "Usage: create_postal "
                "<video_file> <multicast_address[:port]>";

        case POSTAL_ARGS_ERROR_VIDEO_OPEN:
            return
                "Video file does not exist or is not readable";

        case POSTAL_ARGS_ERROR_VIDEO_TYPE:
            return
            "Video path does not refer to a regular file";

        case POSTAL_ARGS_ERROR_VIDEO_NAME:
            return "Video file name must have an extension";

        case POSTAL_ARGS_ERROR_ADDRESS_FORMAT:
            return "Invalid IPv4 address format";

        case POSTAL_ARGS_ERROR_ADDRESS_NOT_MULTICAST:
            return
                "IPv4 address is outside multicast range "
                "224.0.0.0/4";

        case POSTAL_ARGS_ERROR_PORT_FORMAT:
            return "UDP port must be an integer";

        case POSTAL_ARGS_ERROR_PORT_RANGE:
            return "UDP port must be in range 1-65535";

        default:
            return "[ARGS] Unknown error";
    }
}