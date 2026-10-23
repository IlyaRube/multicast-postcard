#ifndef POSTAL_TEMPLATE_READER_H
#define POSTAL_TEMPLATE_READER_H

#include <metadata.h>
#include <stddef.h>
#include <stdint.h>

typedef enum postal_reader_status
{
    POSTAL_READER_OK           = 0,
    POSTAL_READER_ERR_ARG      = -1, // некорректные аргументы
    POSTAL_READER_ERR_OPEN     = -2, // не удалось открыть файл
    POSTAL_READER_ERR_STAT     = -3, // не удалось получить размер файла
    POSTAL_READER_ERR_MMAP     = -4, // не удалось отобразить файл в память
    POSTAL_READER_ERR_METADATA = -5, // метаданные отсутствуют или повреждены
} postal_reader_status_t;

/**
 * @brief Открытая и проверенная открытка.
 *
 * Файл открытки целиком отображается в память только для чтения, а поле
 * video указывает на начало MPEG-TS внутри этого отображения.
 */
typedef struct
{
    int fd;                               // дескриптор собственного файла
    const uint8_t *map;                   // начало memory-map всего файла
    size_t map_size;                      // размер файла в байтах
    const uint8_t *video;                 // начало MPEG-TS внутри отображения
    uint64_t video_size;                  // размер MPEG-TS в байтах
    postal_metadata_t meta;               // проверенная копия метаданных
    postal_metadata_status_t meta_status; // результат проверки метаданных
} postal_card;

/**
 * @brief Найти собственный файл, прочитать и проверить вшитые данные.
 *
 * @param card [out] структура открытки (заполняется при POSTAL_READER_OK)
 * @return код из postal_reader_status
 */
postal_reader_status_t postal_card_open(postal_card *card);

/**
 * @brief То же, что postal_card_open(), но для указанного файла.
 *
 * @param card [out] структура открытки (заполняется при POSTAL_READER_OK)
 * @param path путь к файлу открытки
 * @return код из postal_reader_status
 */
postal_reader_status_t postal_card_open_file(
    postal_card *card,
    const char *path
);

/**
 * @brief Освободить ресурсы, занятые postal_card_open
 *
 * @param card открытка
 */
void postal_card_close(postal_card *card);

/**
 * @brief Получить текстовое описание кода ошибки чтения открытки.
 *
 * @param status код из postal_reader_status
 * @return строка с описанием ошибки
 */
const char *postal_reader_status_string(postal_reader_status_t status);

#endif