#ifndef BINARY_H
#define BINARY_H

#include <fcntl.h>
#include <stdlib.h>

/**
 * @brief структура, содержащая информацию о положении и размере двоичного массива данных в файле
 */
typedef struct blob_location
{
	off_t offset; // Смещение данных
	ssize_t length; // Длина массива данных
} blob_location_t;

#endif //BINARY_H
