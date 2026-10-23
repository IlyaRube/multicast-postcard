#include "broadcaster.h"
#include "ts_loop.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

enum
{
    // 7 TS-пакетов = 1316 байт, помещается в Ethernet MTU вместе с заголовками
    TS_PACKETS_PER_DATAGRAM = 7,
    DATAGRAM_SIZE = POSTAL_TS_PACKET_SIZE * TS_PACKETS_PER_DATAGRAM,
};

static const long NSEC_PER_SEC = 1000000000L;

static void add_nsec(
    struct timespec *moment,
    uint64_t nsec
);
static int open_multicast_socket(
    const postal_metadata_t *meta,
    struct sockaddr_in *dest
);

/**
 * @brief Сдвинуть момент времени на заданное число наносекунд.
 *
 * @param moment [in, out] момент времени
 * @param nsec сдвиг в наносекундах
 */
static void add_nsec(
    struct timespec *moment,
    uint64_t nsec
)
{
    moment->tv_sec += (time_t)(nsec / NSEC_PER_SEC);
    moment->tv_nsec += (long)(nsec % NSEC_PER_SEC);

    if (moment->tv_nsec >= NSEC_PER_SEC)
    {
        moment->tv_sec++;
        moment->tv_nsec -= NSEC_PER_SEC;
    }
}

/**
 * @brief Создать UDP-сокет и адрес назначения из метаданных.
 *
 * @param meta проверенные метаданные открытки
 * @param dest [out] multicast-адрес и порт назначения
 * @return дескриптор сокета или -1 при ошибке
 */
static int open_multicast_socket(
    const postal_metadata_t *meta,
    struct sockaddr_in *dest
)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        return -1;
    }

    *dest = (struct sockaddr_in){0};
    dest->sin_family = AF_INET;
    dest->sin_port = htons(meta->udp_port);
    memcpy(&dest->sin_addr.s_addr, meta->multicast_addr, sizeof(meta->multicast_addr));

    uint8_t ttl = 16;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
    {
        close(sock);
        return -1;
    }
    return sock;
}

postal_broadcast_status_t postal_broadcast_run(
    const postal_card *card,
    const volatile sig_atomic_t *stop_requested
)
{
    struct sockaddr_in dest = {0};
    struct timespec next_send = {0};
    uint8_t datagram[DATAGRAM_SIZE];
    postal_ts_loop *loop = NULL;
    uint64_t stream_size = 0;
    uint64_t offset = 0;
    uint64_t interval_nsec = 0;
    int status = POSTAL_BROADCAST_OK;
    int sock = -1;

    if (card == NULL
        || stop_requested == NULL
        || card->video == NULL
        || card->video_size == 0
        || card->meta.bitrate == 0)
    {
        return POSTAL_BROADCAST_ERR_ARG;
    }

    // Передаются только целые TS-пакеты, неполный хвост видео отбрасывается
    stream_size = card->video_size - card->video_size % POSTAL_TS_PACKET_SIZE;

    loop = malloc(sizeof(*loop));
    if (loop == NULL)
    {
        return POSTAL_BROADCAST_ERR_MEMORY;
    }

    if (postal_ts_loop_init(loop, card->video, stream_size, card->meta.bitrate) != POSTAL_TS_LOOP_OK)
    {
        free(loop);
        return POSTAL_BROADCAST_ERR_STREAM;
    }

    sock = open_multicast_socket(&card->meta, &dest);
    if (sock < 0)
    {
        free(loop);
        return POSTAL_BROADCAST_ERR_SOCKET;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &next_send) != 0)
    {
        close(sock);
        free(loop);
        return POSTAL_BROADCAST_ERR_CLOCK;
    }

    while (!*stop_requested)
    {
        uint64_t remaining = stream_size - offset;
        size_t chunk_size = remaining < DATAGRAM_SIZE ? (size_t)remaining : DATAGRAM_SIZE;
        ssize_t sent = 0;

        // Видео в памяти только для чтения: метки времени и CC правятся в копии
        memcpy(datagram, card->video + offset, chunk_size);
        for (size_t pos = 0; pos < chunk_size; pos += POSTAL_TS_PACKET_SIZE)
        {
            postal_ts_loop_patch(loop, datagram + pos);
        }

        sent = sendto(
            sock,
            datagram,
            chunk_size,
            0,
            (const struct sockaddr *)&dest,
            sizeof(dest)
        );

        if (sent < 0)
        {
            // Отправка прервана сигналом: дейтаграмма не ушла, повторяем её
            // с того же смещения (или выходим, если это был запрос остановки)
            if (errno == EINTR)
            {
                continue;
            }

            status = POSTAL_BROADCAST_ERR_SEND;
            break;
        }

        offset += chunk_size;
        if (offset >= stream_size)
        {
            // Дошли до конца видео, возвращаемся в начало видео с продолжением
            // PCR/PTS/DTS и Continuity Counter
            offset = 0;
            postal_ts_loop_advance(loop);
        }

        interval_nsec = (uint64_t)chunk_size * 8 * NSEC_PER_SEC / card->meta.bitrate;
        add_nsec(&next_send, interval_nsec);

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_send, NULL);
    }

    close(sock);
    free(loop);
    return status;
}

const char *postal_broadcast_status_string(postal_broadcast_status_t status)
{
    switch (status)
    {
        case POSTAL_BROADCAST_OK:
            return "Success";

        case POSTAL_BROADCAST_ERR_ARG:
            return "Invalid arguments for broadcasting the postcard";

        case POSTAL_BROADCAST_ERR_SOCKET:
            return "Failed to create the UDP socket";

        case POSTAL_BROADCAST_ERR_CLOCK:
            return "Failed to get the current time";

        case POSTAL_BROADCAST_ERR_SEND:
            return "Failed to send the datagram";

        case POSTAL_BROADCAST_ERR_MEMORY:
            return "Not enough memory for broadcasting the postcard";

        case POSTAL_BROADCAST_ERR_STREAM:
            return "Postcard video is not a valid MPEG-TS stream";

        default:
            return "[POSTAL_BROADCAST] Unknown error";
    }
}
