#ifndef POSTAL_TEMPLATE_TS_LOOP_H
#define POSTAL_TEMPLATE_TS_LOOP_H

#include <stdint.h>

enum
{
    POSTAL_TS_PACKET_SIZE = 188,
    POSTAL_TS_PID_COUNT = 8192,
};

typedef enum postal_ts_loop_status
{
    POSTAL_TS_LOOP_OK         = 0,
    
    POSTAL_TS_LOOP_ERR_ARG    = -1, // некорректные аргументы
    POSTAL_TS_LOOP_ERR_STREAM = -2, // видео не похоже на MPEG-TS
} postal_ts_loop_status_t;

/**
 * @brief Состояние зацикленного вещания MPEG-TS.
 *
 * При повторной передаче видео с начала временные метки PCR, PTS и DTS
 * сдвигаются на длительность уже переданных проходов, а поле Continuity
 * Counter каждого PID сдвигается так, чтобы первый пакет нового прохода
 * продолжал счёт от последнего пакета предыдущего.
 */
typedef struct
{
    uint64_t loop_duration;                   // длительность одного прохода, такты 90 кГц
    uint64_t time_offset;                     // текущий сдвиг PCR/PTS/DTS, такты 90 кГц
    uint8_t cc_used[POSTAL_TS_PID_COUNT];     // PID встречался в пакетах с полезной нагрузкой
    uint8_t cc_first[POSTAL_TS_PID_COUNT];    // CC первого пакета PID в видео
    uint8_t cc_last[POSTAL_TS_PID_COUNT];     // CC последнего пакета PID в видео
    uint8_t cc_offset[POSTAL_TS_PID_COUNT];   // текущий сдвиг CC для PID
} postal_ts_loop;

/**
 * @brief Проанализировать видео и подготовить состояние зацикливания.
 *
 * Длительность прохода вычисляется по PCR (интерполяцией на весь размер
 * видео), а при их отсутствии — по битрейту из метаданных. Длительность
 * дополнительно увеличивается, если иначе PTS/DTS какого-либо потока
 * перестали бы возрастать на стыке проходов.
 *
 * @param loop       [out] состояние зацикливания
 * @param video      начало MPEG-TS
 * @param video_size размер MPEG-TS в байтах, кратный POSTAL_TS_PACKET_SIZE
 * @param bitrate    битрейт видео, бит/с
 * @return код из postal_ts_loop_status
 */
postal_ts_loop_status_t postal_ts_loop_init(
    postal_ts_loop *loop,
    const uint8_t *video,
    uint64_t video_size,
    uint64_t bitrate
);

/**
 * @brief Скорректировать PCR, PTS, DTS и Continuity Counter пакета.
 *
 * Пакеты с нарушенной структурой остаются без изменений.
 *
 * @param loop   состояние зацикливания
 * @param packet [in, out] копия TS-пакета размером POSTAL_TS_PACKET_SIZE
 */
void postal_ts_loop_patch(
    const postal_ts_loop *loop,
    uint8_t *packet
);

/**
 * @brief Перейти к следующему проходу видео.
 *
 * Вызывается после отправки последнего пакета видео, перед отправкой первого.
 *
 * @param loop [in, out] состояние зацикливания
 */
void postal_ts_loop_advance(postal_ts_loop *loop);

const char *ts_loop_status_string(postal_ts_loop_status_t status);

#endif
