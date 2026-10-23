#ifndef POSTAL_TEMPLATE_BROADCASTER_H
#define POSTAL_TEMPLATE_BROADCASTER_H

#include "reader.h"

#include <signal.h>

typedef enum postal_broadcast_status
{
    POSTAL_BROADCAST_OK         = 0,
    POSTAL_BROADCAST_ERR_ARG    = -1, // некорректные аргументы
    POSTAL_BROADCAST_ERR_SOCKET = -2, // не удалось создать UDP-сокет
    POSTAL_BROADCAST_ERR_CLOCK  = -3, // не удалось получить текущее время
    POSTAL_BROADCAST_ERR_SEND   = -4, // не удалось отправить дейтаграмму
    POSTAL_BROADCAST_ERR_MEMORY = -5, // не удалось выделить память
    POSTAL_BROADCAST_ERR_STREAM = -6, // видео не является MPEG-TS
} postal_broadcast_status_t;

/**
 * @brief Вещать видео открытки по UDP-multicast по кругу до запроса остановки.
 *
 * Видео отправляется дейтаграммами из целого числа MPEG-TS пакетов, интервал
 * между дейтаграммами рассчитывается по битрейту из метаданных. По достижении
 * конца видео передача начинается с начала, при этом PCR, PTS и DTS
 * продолжают монотонно возрастать, а Continuity Counter каждого PID
 * продолжает счёт без разрыва. Сокет закрывается при любом исходе.
 *
 * @param card           проверенная открытка
 * @param stop_requested флаг остановки, выставляется обработчиком сигнала
 * @return код из postal_broadcast_status
 */
postal_broadcast_status_t postal_broadcast_run(
    const postal_card *card,
    const volatile sig_atomic_t *stop_requested
);

/**
 * @brief Получить текстовое описание кода ошибки вещания.
 *
 * @param status код из postal_broadcast_status
 * @return строка с описанием ошибки
 */
const char *postal_broadcast_status_string(postal_broadcast_status_t status);

#endif
