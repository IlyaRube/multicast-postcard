#ifndef POSTAL_ARGS_H
#define POSTAL_ARGS_H

#include <stdint.h>

#define POSTAL_DEFAULT_PORT UINT16_C(6000)

typedef enum postal_args_status
{
    POSTAL_ARGS_OK = 0,
    POSTAL_ARGS_ERROR_NULL = -1,
    POSTAL_ARGS_ERROR_COUNT = -2,
    POSTAL_ARGS_ERROR_VIDEO_OPEN = -3,
    POSTAL_ARGS_ERROR_VIDEO_TYPE = -4,
    POSTAL_ARGS_ERROR_ADDRESS_FORMAT = -5,
    POSTAL_ARGS_ERROR_ADDRESS_NOT_MULTICAST = -6,
    POSTAL_ARGS_ERROR_PORT_FORMAT = -7,
    POSTAL_ARGS_ERROR_PORT_RANGE = -8,
    POSTAL_ARGS_ERROR_VIDEO_NAME = -9,
} postal_args_status_t;

/**
 * @brief Проверенные аргументы командной строки create_postal.
 *
 * Поле multicast_addr хранит IPv4-адрес в сетевом порядке байт.
 */
typedef struct postal_args
{
    const char *video_path;
    uint8_t multicast_addr[4];
    uint16_t udp_port;
} postal_args_t;

/**
 * @brief Выполняет парсинг и проверку аргументов командной строки.
 *
 * @param argc Количество аргументов.
 * @param argv Аргументы командной строки.
 * @param args Структура для сохранения проверенных значений.
 *
 * @return POSTAL_ARGS_OK при успехе, иначе код ошибки.
 */
postal_args_status_t postal_args_parse(
    int argc,
    char *argv[],
    postal_args_t *args
);

/**
 * @brief Возвращает текстовое описание статуса парсинга аргументов.
 *
 * @param status Код статуса.
 *
 * @return Строка с описанием статуса.
 */
const char *postal_args_status_string(
    postal_args_status_t status
);

#endif /* POSTAL_ARGS_H */