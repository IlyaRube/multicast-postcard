#ifndef POSTAL_METADATA_H
#define POSTAL_METADATA_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @file metadata.h интерфейс работы с метаданными postcard-файла
 */

#define POSTAL_METADATA_MAGIC UINT32_C(0x504F5354)
#define POSTAL_METADATA_VERSION UINT16_C(1)

typedef enum postal_metadata_status
{
    POSTAL_METADATA_OK = 0,

    POSTAL_METADATA_ERROR_NULL = -1,
    POSTAL_METADATA_ERROR_IO = -2,
    POSTAL_METADATA_ERROR_FILE_TOO_SMALL = -3,
    POSTAL_METADATA_ERROR_MAGIC = -4,
    POSTAL_METADATA_ERROR_VERSION = -5,
    POSTAL_METADATA_ERROR_SIZE = -6,
    POSTAL_METADATA_ERROR_MULTICAST_ADDRESS = -7,
    POSTAL_METADATA_ERROR_PORT = -8,
    POSTAL_METADATA_ERROR_VIDEO_RANGE = -9,
    POSTAL_METADATA_ERROR_BITRATE = -10,
} postal_metadata_status_t;

/**
 * @brief метаданные, записываемые в конец postcard-файла
 */
typedef struct postal_metadata
{
    uint32_t magic;              // идентификатор метаданных
    uint16_t version;            // версия формата
    uint16_t metadata_size;      // размер структуры
    uint8_t multicast_addr[4];   // multicast IPv4 адрес
    uint16_t udp_port;           // UDP порт
    uint16_t padding;            // выравнивание перед 64-битными полями
    uint64_t video_offset;       // смещение MPEG-TS от начала файла
    uint64_t video_size;         // размер MPEG-TS
    uint64_t bitrate;            // битрейт MPEG-TS
} postal_metadata_t;

static_assert(sizeof(postal_metadata_t) == 40,
              "postal_metadata_t must be exactly 40 bytes");

static_assert(offsetof(postal_metadata_t, magic) == 0,
              "Unexpected magic offset");

static_assert(offsetof(postal_metadata_t, version) == 4,
              "Unexpected version offset");

static_assert(offsetof(postal_metadata_t, metadata_size) == 6,
              "Unexpected metadata_size offset");

static_assert(offsetof(postal_metadata_t, multicast_addr) == 8,
              "Unexpected multicast_addr offset");

static_assert(offsetof(postal_metadata_t, udp_port) == 12,
              "Unexpected udp_port offset");

static_assert(offsetof(postal_metadata_t, padding) == 14,
              "Unexpected padding offset");

static_assert(offsetof(postal_metadata_t, video_offset) == 16,
              "Unexpected video_offset offset");

static_assert(offsetof(postal_metadata_t, video_size) == 24,
              "Unexpected video_size offset");

static_assert(offsetof(postal_metadata_t, bitrate) == 32,
              "Unexpected bitrate offset");

/**
 * @brief заполняет структуру метаданных
 *
 * @param metadata структура метаданных
 * @param multicast_addr multicast IPv4 адрес
 * @param udp_port UDP порт
 * @param video_offset смещение MPEG-TS от начала файла
 * @param video_size размер MPEG-TS
 * @param bitrate битрейт MPEG-TS
 * @return postal_metadata_status_t код ошибки
 */
postal_metadata_status_t postal_metadata_init(
    postal_metadata_t *metadata,
    const uint8_t multicast_addr[4],
    uint16_t udp_port,
    uint64_t video_offset,
    uint64_t video_size,
    uint64_t bitrate
);

/**
 * @brief проверяет корректность полей метаданных
 *
 * @param metadata структура метаданных
 * @return postal_metadata_status_t код ошибки
 */
postal_metadata_status_t postal_metadata_validate(
    const postal_metadata_t *metadata
);

/**
 * @brief записывает метаданные в конец postcard-файла
 *
 * @param fd дескриптор postcard-файла
 * @param metadata структура метаданных
 * @return postal_metadata_status_t код ошибки
 */
postal_metadata_status_t postal_metadata_write(
    int fd,
    const postal_metadata_t *metadata
);

/**
 * @brief читает метаданные из конца postcard-файла
 *
 * @param fd дескриптор postcard-файла
 * @param metadata структура для прочитанных метаданных
 * @return postal_metadata_status_t код ошибки
 */
postal_metadata_status_t postal_metadata_read(
    int fd,
    postal_metadata_t *metadata
);

/**
 * @brief преобразует код ошибки в строку
 *
 * @param status код ошибки
 * @return const char* строка с описанием ошибки
 */
const char *postal_metadata_status_string(
    postal_metadata_status_t status
);

#endif /* POSTAL_METADATA_H */
