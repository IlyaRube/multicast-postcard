#ifndef COMPILER_H
#define COMPILER_H

#include <metadata.h>

/**
 * @brief коды ошибок компилятора
 */
typedef enum compiler_status
{
	COMPILER_OK = 0, // успешная операция

	COMPILER_INVALID_METADATA = -1, // некорректные метаданные
	COMPILER_ERROR_WRITE_METADATA = -2, // ошибка записи метаданных
	COMPILER_ERROR_COPY_MPEGTS = -3, // ошибка копирования видеопотока
	COMPILER_ERROR_COPY_TEMPLATE = -4, // ошибка копирования шаблона
	COMPILER_ERROR_CREATE_OUTPUT = -5, // ошибка создания выходного файла
} compiler_status_t;

/**
 * @brief функция для генерации открытки
 * @param output_file путь к выходному файлу-открытке
 * @param self_fd дескриптор /proc/self/exe
 * @param mpegts_fd дескриптор файла MPEG-TS
 * @param ip multicast IP-адрес трансляции
 * @param port порт трансляции
 * @param bitrate битрейт видеопотока
 * @return код ошибки или COMPILER_OK, если успех
 */
compiler_status_t compile(
	const char *output_file,
	int self_fd,
	int mpegts_fd,
	uint8_t ip[4],
	uint16_t port,
	uint64_t bitrate
	);

/**
 * @brief функция для преобразования кода ошибки в читаемую строку
 * @param status код ошибки
 * @return строка с описанием ошибки
 */
const char * compiler_status_string(compiler_status_t status);

#endif //COMPILER_H
