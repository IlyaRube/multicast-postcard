#ifndef IO_EXACT_H
#define IO_EXACT_H

#include <sys/sendfile.h>

/**
 * @brief функция, гарантированно пишущая указанное количество байт в файл
 * @param fd файловый дескриптор
 * @param buffer буфер данных
 * @param n количество байт
 * @return количество записанных байт или -1
 */
ssize_t write_exact(
	int fd, 
	const void *buffer, 
	size_t n
);

/**
 * @brief функция, гарантированно читающая указанное количество байт из файла
 * @param fd файловый дескриптор
 * @param buffer буфер данных
 * @param n количество байт
 * @return количество записанных байт или -1
 */
ssize_t read_exact(
	int fd, 
	void *buffer, 
	size_t n
);


/**
 * @brief функция, гарантированно пишущая указанное количество байт со смещением в файл
 * @param fd файловый дескриптор
 * @param buffer буфер данных
 * @param offset смещение
 * @param n количество байт
 * @return количество записанных байт или -1
 */
ssize_t pwrite_exact(
	int fd, 
	void *buffer, 
	size_t n, 
	off_t offset
);

/**
 * @brief функция, гарантированно читающая указанное количество байт со смещением из файла
 * @param fd файловый дескриптор
 * @param buffer буфер данных
 * @param offset смещение
 * @param n количество байт
 * @return количество записанных байт или -1
 */
ssize_t pread_exact(
	int fd, 
	void *buffer, 
	size_t n, 
	off_t offset
);

/**
 * @brief функция, копирующая заданное количество байт из файла from в файл to
 * @param to целевой файл
 * @param from файл-источник
 * @param read_offset смещение для записи (если NULL, то используется смещение дескриптора)
 * @param length количество байт
 * @return количество скопированных байт
 */
ssize_t copy_file_exact(
	int to,
	int from,
	off_t *read_offset,
	size_t length
);

#endif //IO_EXACT_H
