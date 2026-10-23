#include "ts_loop.h"

#include <stddef.h>
#include <string.h>

enum
{
    TS_SYNC_BYTE = 0x47,
    TS_NULL_PID = 0x1FFF,
    TS_HEADER_SIZE = 4,
    // длина (1 байт) + флаги (1 байт) + PCR (6 байт)
    TS_PCR_MIN_AF_LENGTH = 7,
    // PES: start code (3) + stream_id (1) + длина (2) + флаги (2) + длина заголовка (1)
    PES_HEADER_SIZE = 9,
    PES_TIMESTAMP_SIZE = 5,
    PCR_FIELD_SIZE = 6,
    // Сколько потоков отслеживается для проверки PTS/DTS
    MAX_TRACKED_STREAMS = 32,
    // Continuity Counter — 4-битный счётчик
    TS_CC_MASK = 0x0F,
};

// Поля заголовка TS-пакета
#define TS_TRANSPORT_ERROR_INDICATOR(packet)    ((packet)[1] & 0x80)
#define TS_PAYLOAD_UNIT_START_INDICATOR(packet) ((packet)[1] & 0x40)
#define TS_PID(packet)                          ((uint16_t)((((packet)[1] & 0x1F) << 8) | (packet)[2]))
#define TS_HAS_ADAPTATION_FIELD(packet)         ((packet)[3] & 0x20)
#define TS_HAS_PAYLOAD(packet)                  ((packet)[3] & 0x10)
#define TS_CONTINUITY_COUNTER(packet)           ((uint8_t)((packet)[3] & TS_CC_MASK))
#define TS_SET_CONTINUITY_COUNTER(packet, cc) \
    ((packet)[3] = (uint8_t)(((packet)[3] & ~TS_CC_MASK) | ((cc) & TS_CC_MASK)))

// Поля adaptation field, начинающегося сразу за заголовком TS-пакета
#define TS_AF_LENGTH(packet)   ((packet)[TS_HEADER_SIZE])
#define TS_AF_PCR_FLAG(packet) ((packet)[TS_HEADER_SIZE + 1] & 0x10)

// Поля заголовка PES
#define PES_HAS_START_CODE(pes)      ((pes)[0] == 0x00 && (pes)[1] == 0x00 && (pes)[2] == 0x01)
#define PES_STREAM_ID(pes)           ((pes)[3])
// Расширенный заголовок PES начинается с битов '10'
#define PES_HAS_OPTIONAL_MARKER(pes) (((pes)[6] & 0xC0) == 0x80)
#define PES_HAS_PTS(pes)             ((pes)[7] & 0x80)
#define PES_HAS_DTS(pes)             (((pes)[7] & 0xC0) == 0xC0)

// PCR base, PTS и DTS — 33-битные счётчики частотой 90 кГц
static const uint64_t TIMESTAMP_MASK = (UINT64_C(1) << 33) - 1;
static const uint64_t TIMESTAMP_HZ = 90000;
static const uint64_t PCR_EXT_PER_BASE = 300;

/**
 * @brief Положение полей внутри TS-пакета, требующих коррекции.
 *
 * Смещения отсчитываются от начала пакета, 0 означает отсутствие поля.
 */
typedef struct
{
    uint16_t pid;
    int has_payload;
    size_t pcr;
    size_t pts;
    size_t dts;
} ts_fields;

/**
 * @brief Метки времени одного элементарного потока, собранные при анализе.
 */
typedef struct
{
    uint16_t pid;
    uint64_t count;
    uint64_t first;
    uint64_t prev;
    uint64_t last;
} stream_times;

static int parse_packet(
    const uint8_t *packet,
    ts_fields *fields
);
static void parse_pes_header(
    const uint8_t *packet,
    size_t pos,
    ts_fields *fields
);
static int pes_has_optional_header(uint8_t stream_id);
static uint64_t read_timestamp(const uint8_t *field);
static void write_timestamp(
    uint8_t *field,
    uint64_t value
);
static uint64_t read_pcr(const uint8_t *field);
static void write_pcr_base(
    uint8_t *field,
    uint64_t base
);
static void track_stream(
    stream_times *streams,
    size_t *stream_count,
    uint16_t pid,
    uint64_t timestamp
);
static uint64_t estimate_duration(
    const uint8_t *video,
    uint64_t video_size,
    uint64_t bitrate
);

/**
 * @brief Разобрать заголовок TS-пакета и найти поля PCR, PTS и DTS.
 *
 * @param packet TS-пакет
 * @param fields [out] найденные поля
 * @return 0 при успехе, -1 если пакет повреждён
 */
static int parse_packet(
    const uint8_t *packet,
    ts_fields *fields
)
{
    size_t pos = TS_HEADER_SIZE;

    *fields = (ts_fields){0};

    // Пакеты без синхробайта или с transport_error_indicator не трогаем
    if (packet[0] != TS_SYNC_BYTE || TS_TRANSPORT_ERROR_INDICATOR(packet))
    {
        return -1;
    }

    fields->pid = TS_PID(packet);
    fields->has_payload = TS_HAS_PAYLOAD(packet) != 0;

    if (TS_HAS_ADAPTATION_FIELD(packet))
    {
        size_t af_length = TS_AF_LENGTH(packet);

        if (TS_HEADER_SIZE + 1 + af_length > POSTAL_TS_PACKET_SIZE)
        {
            return -1;
        }

        if (af_length >= TS_PCR_MIN_AF_LENGTH && TS_AF_PCR_FLAG(packet))
        {
            fields->pcr = TS_HEADER_SIZE + 2;
        }

        pos += 1 + af_length;
    }

    // PES-заголовок находится только в пакете с payload_unit_start_indicator
    if (fields->has_payload && TS_PAYLOAD_UNIT_START_INDICATOR(packet))
    {
        parse_pes_header(packet, pos, fields);
    }

    return 0;
}

/**
 * @brief Найти PTS и DTS в PES-заголовке, начинающемся в пакете.
 *
 * @param packet TS-пакет
 * @param pos    смещение начала полезной нагрузки
 * @param fields [in, out] найденные поля
 */
static void parse_pes_header(
    const uint8_t *packet,
    size_t pos,
    ts_fields *fields
)
{
    const uint8_t *pes = packet + pos;
    size_t available = POSTAL_TS_PACKET_SIZE - pos;

    if (available < PES_HEADER_SIZE
        || !PES_HAS_START_CODE(pes)
        || !pes_has_optional_header(PES_STREAM_ID(pes))
        || !PES_HAS_OPTIONAL_MARKER(pes))
    {
        return;
    }

    if (PES_HAS_PTS(pes) && available >= PES_HEADER_SIZE + PES_TIMESTAMP_SIZE)
    {
        fields->pts = pos + PES_HEADER_SIZE;
    }

    if (PES_HAS_DTS(pes) && available >= PES_HEADER_SIZE + 2 * PES_TIMESTAMP_SIZE)
    {
        fields->dts = pos + PES_HEADER_SIZE + PES_TIMESTAMP_SIZE;
    }
}

/**
 * @brief Проверить, есть ли у PES с данным stream_id расширенный заголовок.
 *
 * @param stream_id идентификатор потока PES
 * @return 1 если заголовок с PTS/DTS возможен, иначе 0
 */
static int pes_has_optional_header(uint8_t stream_id)
{
    switch (stream_id)
    {
        case 0xBC: // program_stream_map
        case 0xBE: // padding_stream
        case 0xBF: // private_stream_2
        case 0xF0: // ECM
        case 0xF1: // EMM
        case 0xF2: // DSMCC
        case 0xF8: // H.222.1 type E
        case 0xFF: // program_stream_directory
            return 0;

        default:
            return 1;
    }
}

/**
 * @brief Прочитать 33-битную метку PTS/DTS из PES-заголовка.
 *
 * @param field начало 5-байтового поля
 * @return значение метки, такты 90 кГц
 */
static uint64_t read_timestamp(const uint8_t *field)
{
    return ((uint64_t)(field[0] & 0x0E) << 29)
        | ((uint64_t)field[1] << 22)
        | ((uint64_t)(field[2] & 0xFE) << 14)
        | ((uint64_t)field[3] << 7)
        | ((uint64_t)field[4] >> 1);
}

/**
 * @brief Записать 33-битную метку PTS/DTS, сохранив префикс и marker-биты.
 *
 * @param field [in, out] начало 5-байтового поля
 * @param value значение метки, такты 90 кГц
 */
static void write_timestamp(
    uint8_t *field,
    uint64_t value
)
{
    field[0] = (uint8_t)((field[0] & 0xF1) | ((value >> 29) & 0x0E));
    field[1] = (uint8_t)(value >> 22);
    field[2] = (uint8_t)((field[2] & 0x01) | ((value >> 14) & 0xFE));
    field[3] = (uint8_t)(value >> 7);
    field[4] = (uint8_t)((field[4] & 0x01) | ((value << 1) & 0xFE));
}

/**
 * @brief Прочитать PCR из adaptation field.
 *
 * @param field начало 6-байтового поля PCR
 * @return значение PCR, такты 27 МГц
 */
static uint64_t read_pcr(const uint8_t *field)
{
    uint64_t base = ((uint64_t)field[0] << 25)
        | ((uint64_t)field[1] << 17)
        | ((uint64_t)field[2] << 9)
        | ((uint64_t)field[3] << 1)
        | ((uint64_t)field[4] >> 7);
    uint64_t extension = ((uint64_t)(field[4] & 0x01) << 8) | field[5];

    return base * PCR_EXT_PER_BASE + extension;
}

/**
 * @brief Записать базу PCR, сохранив reserved-биты и расширение.
 *
 * @param field [in, out] начало 6-байтового поля PCR
 * @param base  база PCR, такты 90 кГц
 */
static void write_pcr_base(
    uint8_t *field,
    uint64_t base
)
{
    field[0] = (uint8_t)(base >> 25);
    field[1] = (uint8_t)(base >> 17);
    field[2] = (uint8_t)(base >> 9);
    field[3] = (uint8_t)(base >> 1);
    field[4] = (uint8_t)((field[4] & 0x7F) | ((base & 0x01) << 7));
}

/**
 * @brief Запомнить метку времени декодирования элементарного потока.
 *
 * Потоки сверх MAX_TRACKED_STREAMS игнорируются.
 *
 * @param streams      [in, out] таблица потоков
 * @param stream_count [in, out] число потоков в таблице
 * @param pid          PID потока
 * @param timestamp    DTS (или PTS, если DTS нет), такты 90 кГц
 */
static void track_stream(
    stream_times *streams,
    size_t *stream_count,
    uint16_t pid,
    uint64_t timestamp
)
{
    stream_times *stream = NULL;

    for (size_t i = 0; i < *stream_count; i++)
    {
        if (streams[i].pid == pid)
        {
            stream = &streams[i];
            break;
        }
    }

    if (stream == NULL)
    {
        if (*stream_count >= MAX_TRACKED_STREAMS)
        {
            return;
        }

        stream = &streams[(*stream_count)++];
        *stream = (stream_times) {
            .pid = pid,
            .first = timestamp,
            .last = timestamp
        };
    }

    stream->prev = stream->last;
    stream->last = timestamp;
    stream->count++;
}

/**
 * @brief Вычислить длительность одного прохода видео.
 *
 * @param video      начало MPEG-TS
 * @param video_size размер MPEG-TS в байтах
 * @param bitrate    битрейт видео из метаданных, бит/с
 * @return длительность прохода, такты 90 кГц
 */
static uint64_t estimate_duration(
    const uint8_t *video,
    uint64_t video_size,
    uint64_t bitrate
)
{
    stream_times streams[MAX_TRACKED_STREAMS];
    size_t stream_count = 0;
    uint64_t pcr_first = 0;
    uint64_t pcr_last = 0;
    uint64_t pcr_first_pos = 0;
    uint64_t pcr_last_pos = 0;
    uint64_t pcr_count = 0;
    uint16_t pcr_pid = 0;
    uint64_t duration = 0;

    for (uint64_t pos = 0; pos < video_size; pos += POSTAL_TS_PACKET_SIZE)
    {
        const uint8_t *packet = video + pos;
        ts_fields fields;

        if (parse_packet(packet, &fields) != 0)
        {
            continue;
        }

        // Для оценки длительности используется PCR одного (первого найденного) PID
        if (fields.pcr != 0 && (pcr_count == 0 || fields.pid == pcr_pid))
        {
            if (pcr_count == 0)
            {
                pcr_pid = fields.pid;
                pcr_first = read_pcr(packet + fields.pcr);
                pcr_first_pos = pos;
            }

            pcr_last = read_pcr(packet + fields.pcr);
            pcr_last_pos = pos;
            pcr_count++;
        }

        if (fields.dts != 0)
        {
            track_stream(streams, &stream_count, fields.pid, read_timestamp(packet + fields.dts));
        }
        else if (fields.pts != 0)
        {
            track_stream(streams, &stream_count, fields.pid, read_timestamp(packet + fields.pts));
        }
    }

    if (pcr_count >= 2 && pcr_last_pos > pcr_first_pos)
    {
        // Разность по модулю 2^33 * 300 учитывает переполнение PCR внутри видео
        uint64_t pcr_modulus = (TIMESTAMP_MASK + 1) * PCR_EXT_PER_BASE;
        uint64_t pcr_span = (pcr_last + pcr_modulus - pcr_first) % pcr_modulus;
        double pcr_per_byte = (double)pcr_span / (double)(pcr_last_pos - pcr_first_pos);

        duration = (uint64_t)(pcr_per_byte * (double)video_size / (double)PCR_EXT_PER_BASE + 0.5);
    }
    else
    {
        duration = video_size * 8 * TIMESTAMP_HZ / bitrate;
    }

    // Первая метка нового прохода должна идти после последней метки предыдущего
    // с тем же шагом, что и между двумя последними кадрами потока
    for (size_t i = 0; i < stream_count; i++)
    {
        if (streams[i].count >= 2)
        {
            uint64_t span = (streams[i].last - streams[i].first) & TIMESTAMP_MASK;
            uint64_t step = (streams[i].last - streams[i].prev) & TIMESTAMP_MASK;

            if (duration < span + step)
            {
                duration = span + step;
            }
        }
    }

    return duration > 0 ? duration : 1;
}

postal_ts_loop_status_t postal_ts_loop_init(
    postal_ts_loop *loop,
    const uint8_t *video,
    uint64_t video_size,
    uint64_t bitrate
)
{
    if (loop == NULL
        || video == NULL
        || video_size == 0
        || video_size % POSTAL_TS_PACKET_SIZE != 0
        || bitrate == 0)
    {
        return POSTAL_TS_LOOP_ERR_ARG;
    }

    if (video[0] != TS_SYNC_BYTE)
    {
        return POSTAL_TS_LOOP_ERR_STREAM;
    }

    memset(loop, 0, sizeof(*loop));

    for (uint64_t pos = 0; pos < video_size; pos += POSTAL_TS_PACKET_SIZE)
    {
        const uint8_t *packet = video + pos;
        ts_fields fields;

        // CC увеличивается только в пакетах с полезной нагрузкой, у null-пакетов не определён
        if (parse_packet(packet, &fields) != 0
            || !fields.has_payload
            || fields.pid == TS_NULL_PID)
        {
            continue;
        }

        if (!loop->cc_used[fields.pid])
        {
            loop->cc_used[fields.pid] = 1;
            loop->cc_first[fields.pid] = TS_CONTINUITY_COUNTER(packet);
        }

        loop->cc_last[fields.pid] = TS_CONTINUITY_COUNTER(packet);
    }

    loop->loop_duration = estimate_duration(video, video_size, bitrate);
    return POSTAL_TS_LOOP_OK;
}

void postal_ts_loop_patch(
    const postal_ts_loop *loop,
    uint8_t *packet
)
{
    ts_fields fields;

    if (loop == NULL
        || packet == NULL
        || parse_packet(packet, &fields) != 0)
    {
        return;
    }

    if (fields.pid != TS_NULL_PID && loop->cc_used[fields.pid])
    {
        TS_SET_CONTINUITY_COUNTER(packet, TS_CONTINUITY_COUNTER(packet) + loop->cc_offset[fields.pid]);
    }

    if (loop->time_offset == 0)
    {
        return;
    }

    if (fields.pcr != 0)
    {
        uint64_t base = read_pcr(packet + fields.pcr) / PCR_EXT_PER_BASE;

        write_pcr_base(packet + fields.pcr, (base + loop->time_offset) & TIMESTAMP_MASK);
    }

    if (fields.pts != 0)
    {
        uint64_t pts = read_timestamp(packet + fields.pts);

        write_timestamp(packet + fields.pts, (pts + loop->time_offset) & TIMESTAMP_MASK);
    }

    if (fields.dts != 0)
    {
        uint64_t dts = read_timestamp(packet + fields.dts);

        write_timestamp(packet + fields.dts, (dts + loop->time_offset) & TIMESTAMP_MASK);
    }
}

void postal_ts_loop_advance(postal_ts_loop *loop)
{
    if (loop == NULL)
    {
        return;
    }

    loop->time_offset = (loop->time_offset + loop->loop_duration) & TIMESTAMP_MASK;

    // Первый пакет нового прохода должен получить CC = последний + 1
    for (size_t pid = 0; pid < POSTAL_TS_PID_COUNT; pid++)
    {
        if (loop->cc_used[pid])
        {
            unsigned step = (unsigned)loop->cc_last[pid] + 1 - loop->cc_first[pid];

            loop->cc_offset[pid] = (uint8_t)((loop->cc_offset[pid] + step) & TS_CC_MASK);
        }
    }
}

const char *ts_loop_status_string(postal_ts_loop_status_t status){
    switch (status) 
    {
        case POSTAL_TS_LOOP_OK:
            return "Success";

        case POSTAL_TS_LOOP_ERR_ARG:
            return "Wrong argument send to loop";

        case POSTAL_TS_LOOP_ERR_STREAM:
            return "Postal is not MPEG-TS";
            
        default:
            return "[TS_LOOP] Unknown error";
    }
}