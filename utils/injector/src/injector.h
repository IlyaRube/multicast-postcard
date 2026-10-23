#ifndef INJECTOR_H
#define INJECTOR_H

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "binary.h"

typedef enum injector_status
{
	INJECTOR_OK = 0,
	INJECTOR_ERROR_READ = -1,
	INJECTOR_ERROR_WRITE = -2,
} injector_status_t;

/**
 * @brief Преобразует код ошибки в человеко-читаемый текст 
 * @param status код ошибки
 * @return описание ошибки
 */
const char *injector_status_string(injector_status_t status);

/**
 * @brief Встраивает @p injectable_file в конец @p target_file
 * @param injectable_file файл, который записывается в @p target_file
 * @param target_file целевой файл
 * @return Код ошибки
 * @retval ERR_SUCCESS Успешное завершение 
 * @retval ERR_READ Не удалось прочитать @p injectable_file
 * @retval ERR_WRITE Не удалось записать в @p target_file
 */
injector_status_t inject(int injectable_file, int target_file);


#endif //INJECTOR_H
