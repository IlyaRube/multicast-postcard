#ifndef REMUX_H
#define REMUX_H

/**
 * @file интерфейс ре-мультиплексирования видео-файла в формат MPEG-TS 
 */

#include <stdint.h>

typedef enum remux_status{
    REMUX_OK = 0,

    REMUX_NOMEM                 = -1,  // ошибка выделения памяти
    REMUX_ARGERR                = -2,  // дан нулевой указатель для файлового дескриптора
    REMUX_TMPFILE_ERR           = -3,  // не удалось создать временный файл
    REMUX_INPUT_OPEN_ERR        = -4,  // не удалось открыть входной файл
    REMUX_STREAM_INFO_ERR       = -5,  // не удалось получить информацию о потоках
    REMUX_OUT_CTX_ERR           = -6,  // не удалось создать выходной контекст
    REMUX_OUT_STREAM_ERR        = -7,  // не удалось инициализировать выходные потоки
    REMUX_IO_INIT_ERR           = -8,  // не удалось создать контекст ввода
    REMUX_WRITE_HEADER_ERR      = -9,  // не удалось записать заголовок в выходной файл
    REMUX_PROCESS_ERR           = -10, // ошибка при обработке пакетов
    REMUX_NO_SUPPORTED_VIDEO    = -11, // не найден видеопоток с поддерживаемым кодеком
    REMUX_BAD_VIDEO             = -12, // не удалось создать промежуточное видео
} remux_status_t;

/**
 * @brief функция преобразовывает входное видео в MPEG-TS формат и сохраняет во временный файл
 * 
 * @param video_in имя файла входного видео
 * @param fd дескриптор файла получившегося видео. Следует закрыть после использования
 * @param bitrate рассчитанный битрейт потока
 * @return int код ошибки
 */
remux_status_t remux(
    const char* video_in, 
    int *fd,
    int64_t *bitrate
);

/**
 * @brief преобразует код ошибки в читаемую строку
 * 
 * @param status код ошибки типа remux_status_t
 * @return const char* строка с описанием ошибки
 */
const char *remux_status_string(remux_status_t status);

#endif //REMUX_H
