#include <stddef.h>

/**
 * @file ре-мультиплексирование видео-файла в формат MPEG-TS
 */

#include "remux.h"

#include <io_exact.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/dict.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavcodec/bsf.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define OUT_FILENAME_TEMPLATE "/tmp/postal_XXXXXX"
#define OUT_FILE_EXTENTION "mpegts"
#define AVIO_CTX_BUF_SIZE (32 * 1024)
#define AV_DICT_MUXRATE_KEY "muxrate"
#define AV_DICT_DELAY_KEY "max_delay"
#define PCR_PERIOD_KEY "pcr_period"
#define PAT_PERIOD_KEY "pat_period"
#define SDT_PERIOD_KEY "sdt_period"
#define MPEGTS_FLAGS_KEY "mpegts_flags"
#define AV_DICT_DELAY_VAL 1000000 // 1 секунды
#define PCR_PERIOD_VAL 20
#define PAT_PERIOD_VAL 0.1
#define SDT_PERIOD_VAL 0.5
#define MPEGTS_FLAGS_VAL "+initial_discontinuity"

#define BITS_IN_BYTE 8.0
#define MIN_VID_DURATION 0.1 // минимальный размер видео в секундах
// множитель грубого преобразования
// видео с динамичным битрейтом (VBR) в
// видео со статичным битрейтом (CBR)
#define MUXRATE_MULTI 1.35

//маппинг потоков с контекстами фильтров
struct stream_context{
    int *mapping;                //маппинг: in_stream -> out_stream
    AVBSFContext **bsf_contexts; //массив контекстов фильтров
    size_t size;                 //размер маппинга
};

// map (id кодека; название)
struct codec_map_t{
    enum AVCodecID codec_id;
    const char *bsf_name;
};

static int write_packet(
    void *opaque,
    const uint8_t *buf,
    int buf_size
);
static int64_t seek_packet(
    void *opaque,
    int64_t offset,
    int whence
);

static int remux_step(
    const char *video_in,
    int     *fd,
    int64_t *muxrate,
    int64_t *out_duration
);
static int io_init(
    AVFormatContext *out_context,
    int *fd
);
static int setup_out_streams(
    AVFormatContext *in_context,
    AVFormatContext *out_context,
    struct stream_context *stream_context
);
static int process_packets(
    AVFormatContext *in_context,
    AVFormatContext *out_context,
    struct stream_context *stream_context,
    int64_t *duration_us
);
static const char *get_bsf_codec_name(const enum AVCodecID id);
static int is_audio_supported(const enum AVCodecID id);
static void free_stream_context(struct stream_context* stream_ctxs);
static int64_t bitrate_calc(
    int64_t duration_us,
    int64_t file_size
);

/**
 * @brief ре-мультиплексирование видео в MPEG-TS с прогонкой через bsf
 * для последующей трансляции через UDP.
 *
 * @param video_in путь до видео
 * @param fd дескриптор файла получившегося видео
 * @return int код ошибки
 */
remux_status_t remux(
    const char* video_in,
    int *fd,
    int64_t *bitrate
)
{
    int tmp_fd = -1;
    int ret = REMUX_OK;
    int64_t duration  = 0;
    int64_t file_size = 0;
    struct stat st    = {0};

    if (!video_in || !fd || !bitrate)
    {
        return REMUX_ARGERR;
    }

    av_log_set_level(AV_LOG_ERROR);

    ret = remux_step(
        video_in,
        &tmp_fd,
        NULL,
        &duration
    );
    if (ret < 0)
    {
        return ret;
    }

    if (fstat(tmp_fd, &st) < 0)
    {
        close(tmp_fd);
        return REMUX_BAD_VIDEO;
    }
    file_size = st.st_size;
    close(tmp_fd);

    if (duration <= 0)
    {
        return REMUX_BAD_VIDEO;
    }
    *bitrate = bitrate_calc(duration, file_size);

    ret = remux_step(
        video_in,
        fd,
        bitrate,
        NULL
    );
    return ret;
}

const char *remux_status_string(remux_status_t status)
{
    switch (status)
    {
        case REMUX_OK:
            return "Success";

        case REMUX_NOMEM:
            return "Memory allocation failed";

        case REMUX_ARGERR:
            return "Null pointer passed for file descriptor";

        case REMUX_TMPFILE_ERR:
            return "Failed to create temporary file";

        case REMUX_INPUT_OPEN_ERR:
            return "Failed to open input file";

        case REMUX_STREAM_INFO_ERR:
            return "Failed to retrieve stream information";

        case REMUX_OUT_CTX_ERR:
            return "Failed to allocate output context";

        case REMUX_OUT_STREAM_ERR:
            return "Failed to initialize output streams";

        case REMUX_IO_INIT_ERR:
            return "Failed to initialize I/O context";

        case REMUX_WRITE_HEADER_ERR:
            return "Failed to write header to output file";
            
        case REMUX_PROCESS_ERR:
            return "Error processing and writing packets";

        case REMUX_NO_SUPPORTED_VIDEO:
            return "No supported video codec found";

        case REMUX_BAD_VIDEO:
            return "Failed to create temporary video";

        default:
            return "[REMUX] Unknown error";
    }
}

// write_packet и seek_packet это callback'и
// для собственного контекста (необходимо,
// чтобы не переоткрывать дескриптор видео)

/**
 * @brief правило записи в файл
 *
 * @param opaque указатель на дескриптор
 * @param buf записываемый буфер
 * @param buf_size размер буфера
 * @return int количество записанных байтов или ошибку
 */
static int write_packet(
    void* opaque,
    const uint8_t *buf,
    int buf_size
)
{
    ssize_t written =  write_exact(
        *(int *) opaque,
        (void*)buf,
        (size_t) buf_size
    );
    return written < 0 ? AVERROR(errno) : written;
}

/**
 * @brief правило позиционирования в файле
 *
 * @param opaque указатель на дескриптор
 * @param offset смещение
 * @param whence точка отсчёта
 * @return int64_t новое смещение
 */
static int64_t seek_packet(
    void *opaque,
    int64_t offset,
    int whence
)
{
    int fd = *(int *)opaque;

    //при попытке FFmpeg получить размер конечного файла
    //возвращается код, сигнализирующий,
    //что невозможно знать конечного размер файла,
    //потому что файл, как минимум, формируется налету
    if (whence == AVSEEK_SIZE)
    {
        return -1;
    }
    off_t ret = lseek(fd, offset, whence);
    return (ret < (off_t)0) ? AVERROR(errno) : ret;
}

/**
 * @brief ремультиплексирование видео, шаги по большей части совпадают,
 * поэтому за них отвечает одна функция, у которой, в зависимости от шага,
 ? меняются подаваемые на вход аргументы
 *
 * @param video_in входное видео
 * @param fd дескриптор получившегося файла
 * @param muxrate второй шаг - подача muxrate, для кодирования со статичным битрейтом
 * @param out_duration первый шаг - вычисление длины видео по меткам pts
 * @return int код ошибки
 */
static int remux_step(
    const char *video_in,
    int     *fd,
    int64_t *muxrate,
    int64_t *out_duration
)
{
    AVFormatContext *in_context    = NULL;
    AVFormatContext *out_context   = NULL;
    AVDictionary    *muxer_options = NULL;

    const char *in_filename = video_in;
    char out_filename[] = OUT_FILENAME_TEMPLATE;
    int ret = 0;
    struct stream_context stream_context = {0};

    if (!fd)
    {
        return REMUX_ARGERR;
    }

    // создание временного файла
    *fd = mkstemp(out_filename);
    if (*fd == -1)
    {
        return REMUX_TMPFILE_ERR;
    }
    // отвязать файл необходимо, чтобы его не было видно в ФС и
    // при close он сам удалился
    if (unlink(out_filename) < 0)
    {
        close(*fd);
        *fd = -1;
        return REMUX_TMPFILE_ERR;
    }

    // открытие входного файла
    ret = avformat_open_input(
        &in_context,
        in_filename,
        0,
        0
    );
    if (ret < 0)
    {
        ret = REMUX_INPUT_OPEN_ERR;
        goto end;
    }

    // сканирование потоков (видео, аудио и субтитров) в видео-контейнере
    ret = avformat_find_stream_info(in_context, 0);
    if (ret < 0)
    {
        ret = REMUX_STREAM_INFO_ERR;
        goto end;
    }

    // создание выходного контекста для записи медиафайла
    ret = avformat_alloc_output_context2(
        &out_context,
        NULL,
        OUT_FILE_EXTENTION,
        out_filename
    );
    if (ret < 0 || !out_context)
    {
        ret = REMUX_OUT_CTX_ERR;
        goto end;
    }

    // сопоставление выходящих потоков к входящим, настройка bsf (bitstream filter)
    ret = setup_out_streams(in_context, out_context, &stream_context);
    if (ret < 0)
    {
        goto end;
    }

    // привязка callback к контексту
    ret = io_init(out_context, fd);
    if (ret < 0)
    {
        goto end;
    }

    if(muxrate && *muxrate > 0)
    {
        char args[256];
        snprintf(
            args,
            sizeof(args),
            AV_DICT_MUXRATE_KEY "=%" PRId64
            ":" AV_DICT_DELAY_KEY "=%d"
            ":" MPEGTS_FLAGS_KEY "=%s" // Сглаживает старт потока
            ":" PCR_PERIOD_KEY "=%d"                      // Писать PCR каждые 20мс для стабильности плеера
            ":" PAT_PERIOD_KEY "=%lf"                     // Чаще слать таблицы PSI (каждые 100мс), чтобы плеер сразу видел аудио-трек
            ":" SDT_PERIOD_KEY "=%lf",
            *muxrate,
            AV_DICT_DELAY_VAL,
            MPEGTS_FLAGS_VAL,
            PCR_PERIOD_VAL,
            PAT_PERIOD_VAL,
            SDT_PERIOD_VAL
        );

        ret = av_dict_parse_string(
            &muxer_options,
            args,
            "=",
            ":",
            0
        );
        if(ret < 0)
        {
            ret = REMUX_PROCESS_ERR;
            goto end;
        }
    }

    // инициализация мультиплексора и запись головы медифайла в контекст
    ret = avformat_write_header(out_context, muxer_options ? &muxer_options : NULL);

    if (ret < 0)
    {
        ret = REMUX_WRITE_HEADER_ERR;
        goto end;
    }

    // обработка пакетов
    ret = process_packets(in_context, out_context, &stream_context, out_duration);
    if (ret < 0)
    {
        goto end;
    }

    // запись хвоста в файл
    ret = av_write_trailer(out_context);
    if(ret < 0)
    {
        ret = REMUX_PROCESS_ERR;
        goto end;
    }

    end:
        free_stream_context(&stream_context);

        av_dict_free(&muxer_options);

        if (in_context)
        {
            avformat_close_input(&in_context);
        }

        if (out_context)
        {
            if (out_context->pb)
            {
                av_freep(&out_context->pb->buffer);
                avio_context_free(&out_context->pb);
            }
            avformat_free_context(out_context);
        }

        if(ret != REMUX_OK && *fd != -1)
        {
            close(*fd);
            *fd = -1;
        }

        return ret;
}

/**
 * @brief инициализация своих правил ввода. необходимо для записи видео через дескриптор
 *
 * @param out_context контекст выходного видео, правила записи которого будут изменены
 * @param fd дескриптор выходного видео
 * @return int код ошибки
 */
static int io_init(
    AVFormatContext *out_context,
    int *fd
)
{
    unsigned char *avio_buffer = av_malloc(AVIO_CTX_BUF_SIZE);
    if (!avio_buffer)
    {
        return REMUX_NOMEM;
    }

    // привязка собственных правил записи и перемещения по файлу
    AVIOContext *avio_context = avio_alloc_context(
        avio_buffer,
        AVIO_CTX_BUF_SIZE,
        1,
        fd,
        NULL,
        &write_packet,
        &seek_packet
    );
    if (!avio_context)
    {
        av_free(avio_buffer);
        return REMUX_IO_INIT_ERR;
    }

    out_context->pb = avio_context;

    return 0;
}

/**
 * @brief настройка выходных потоков, инициализация фильтров
 *
 * @param in_context контекст входного видео
 * @param out_context контекст входного видео
 * @param stream_context маппинг потоков и контекст фильтров
 * в этой структуре в результате выполнения функции
 * будут лежать соответствия входных потоков к выходным: mapping[вх] = вых
 * -1 в соответствии значит, что поток не является стандартным (это не аудио, видео или субтитры)
 *
 * @return int код ошибки
 */
static int setup_out_streams(
    AVFormatContext *in_context,
    AVFormatContext *out_context,
    struct stream_context *stream_context
)
{
    int stream_index = 0;
    int is_video_stream_found = 0;
    int is_audio_stream_found = 0;

    stream_context->size = in_context->nb_streams;
    stream_context->mapping = av_calloc(stream_context->size, sizeof(*stream_context->mapping));
    stream_context->bsf_contexts = av_calloc(stream_context->size, sizeof(*stream_context->bsf_contexts));

    if (!stream_context->mapping || !stream_context->bsf_contexts)
    {
        return REMUX_NOMEM;
    }

    // проход по всем потокам в контейнере
    for (size_t i = 0; i < in_context->nb_streams; i++)
    {
        AVStream *out_stream = NULL;
        AVStream *in_stream = in_context->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        const AVBitStreamFilter *bsf = NULL;
        const char *bsf_codec_name = NULL;

        // запись в маппинг только поддерживаемых кодеков
        if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if (is_video_stream_found)
            {
                stream_context->mapping[i] = -1;
                continue;
            }

            bsf_codec_name = get_bsf_codec_name(in_codecpar->codec_id);
            if(!bsf_codec_name)
            {
                stream_context->mapping[i] = -1;
                continue;
            }

            is_video_stream_found = 1;
        }
        else if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            if (is_audio_stream_found)
            {
                stream_context->mapping[i] = -1;
                continue;
            }

            if(!is_audio_supported(in_codecpar->codec_id))
            {
                stream_context->mapping[i] = -1;
                continue;
            }

            is_audio_stream_found = 1;
        }
        else
        {
            stream_context->mapping[i] = -1;
            continue;
        }

        // пара входной поток -> выходной поток
        stream_context->mapping[i] = stream_index++;

        // создание нового потока в выходном контексте
        out_stream = avformat_new_stream(out_context, NULL);
        if (!out_stream)
        {
            return REMUX_OUT_STREAM_ERR;
        }

        // если видеопоток
        if (bsf_codec_name){
            bsf = av_bsf_get_by_name(bsf_codec_name);
            AVBSFContext **bsf_contexts = stream_context->bsf_contexts; // алиас для краткости

            if (!bsf)
            {
                return REMUX_OUT_STREAM_ERR;
            }

            if (av_bsf_alloc(bsf, &bsf_contexts[i]) < 0)
            {
                return REMUX_OUT_STREAM_ERR;
            }

            // копирование данных из входного кодека в фильтр
            if (avcodec_parameters_copy(bsf_contexts[i]->par_in, in_codecpar) < 0)
            {
                return REMUX_OUT_STREAM_ERR;
            }

            // копирование временной базы для правильного пересчёта временных меток внутри фильтра
            bsf_contexts[i]->time_base_in = in_stream->time_base;

            // инициализация фильтра
            if (av_bsf_init(bsf_contexts[i]) < 0)
            {
                return REMUX_OUT_STREAM_ERR;
            }

            // копирование параметров кодека (изменённые фильтром) в выходной поток
            if (avcodec_parameters_copy(out_stream->codecpar, bsf_contexts[i]->par_out) < 0)
            {
                return REMUX_OUT_STREAM_ERR;
            }
        }
        else
        {
            // простое копирование аудиопотока
            if (avcodec_parameters_copy(out_stream->codecpar, in_codecpar) < 0)
            {
                return REMUX_OUT_STREAM_ERR;
            }
        }

        out_stream->codecpar->codec_tag = 0;
    }

    // если нет поддерживаемого видео - ошибка
    // транслировать видео можно без аудио,
    // поэтому проверки на его наличие аудио нет
    if(!is_video_stream_found){
        return REMUX_NO_SUPPORTED_VIDEO;
    }

    return REMUX_OK;
}

/**
 * @brief чтение и обработка пакетов через bsf, если это необходимо, и запись их в выходной контекст
 *
 * @param in_context контекст входного видео
 * @param out_context контекст конечного видео
 * @param stream_context контекст потоков
 * @return int код ошибки
 */
static int process_packets(
    AVFormatContext *in_context,
    AVFormatContext *out_context,
    struct stream_context *stream_context,
    int64_t *duration_us
)
{
    AVPacket *pkt = NULL;
    int status = REMUX_OK;
    int read_status = 0;
    int64_t first_video_time = AV_NOPTS_VALUE;
    int64_t last_video_time  = AV_NOPTS_VALUE;

    pkt = av_packet_alloc();
    if (!pkt)
    {
        return REMUX_NOMEM;
    }

    // пока есть кадры
    while ((read_status = av_read_frame(in_context, pkt)) >= 0)
    {
        AVStream *in_stream;
        AVStream *out_stream;
        int in_stream_ind = pkt->stream_index;
        int out_stream_ind = -1;
        AVBSFContext *bsf_ctx = NULL;

        if (in_stream_ind < 0 ||
            (unsigned int)in_stream_ind >= stream_context->size)
        {
            status = REMUX_PROCESS_ERR;
            goto end;
        }
        out_stream_ind = stream_context->mapping[in_stream_ind];

        // игнорирование пакетов, не попавших в mapping
        if (out_stream_ind < 0)
        {
            av_packet_unref(pkt);
            continue;
        }

        in_stream  = in_context->streams[in_stream_ind];
        out_stream = out_context->streams[out_stream_ind];
        bsf_ctx = stream_context->bsf_contexts[in_stream_ind];

        // назначение целевого индекса пакета
        pkt->stream_index = out_stream_ind;

        // расчёт времени видео при первой прогонке
        if (duration_us && in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            int64_t current_pts = pkt->pts != AV_NOPTS_VALUE ? pkt->pts : pkt->dts;

            if (current_pts != AV_NOPTS_VALUE)
            {
                int64_t frame_duration = pkt->duration > 0 ? pkt->duration : 0;
                int64_t start_time = av_rescale_q(
                    current_pts,
                    in_stream->time_base,
                    AV_TIME_BASE_Q
                );
                int64_t end_time = av_rescale_q(
                    current_pts + frame_duration,
                    in_stream->time_base,
                    AV_TIME_BASE_Q
                );

                if (first_video_time == AV_NOPTS_VALUE ||
                    start_time < first_video_time)
                {
                    first_video_time = start_time;
                }
                if (last_video_time == AV_NOPTS_VALUE ||
                    end_time > last_video_time)
                {
                    last_video_time = end_time;
                }
            }
        }

        // если видеопоток с фильтром
        if (bsf_ctx)
        {
            int bsf_status = 0;

            // конвертация временных меток для входа в фильтр
            av_packet_rescale_ts(pkt, in_stream->time_base, bsf_ctx->time_base_in);

            // "отправка" пакета на фильтрацию bsf. при успехе пакет станет пустым
            if (av_bsf_send_packet(bsf_ctx, pkt) < 0)
            {
                status = REMUX_PROCESS_ERR;
                goto end;
            }

            // пока у фильтра есть кадры (может быть больше 1)
            while ((bsf_status = av_bsf_receive_packet(bsf_ctx, pkt)) == 0)
            {
                // явное обозначение принадлежности кадра к выходному потоку
                pkt->stream_index = out_stream_ind;

                // конвертация временных меток, нужно для преобразования time_base
                av_packet_rescale_ts(pkt, bsf_ctx->time_base_out, out_stream->time_base);

                // поток не позиционируемый (отправка по сети)
                pkt->pos = -1;

                // запись пакета в выходной файл
                if (av_interleaved_write_frame(out_context, pkt) < 0)
                {
                    status = REMUX_PROCESS_ERR;
                    goto end;
                }

                av_packet_unref(pkt);
            }

            if (bsf_status != AVERROR(EAGAIN) && bsf_status != AVERROR_EOF)
            {
                status = REMUX_PROCESS_ERR;
                goto end;
            }
        }
        // аудио/субтитры
        else
        {
            // простое копирование потока без фильтра
            av_packet_rescale_ts(
                pkt,
                in_stream->time_base,
                out_stream->time_base
            );
            pkt->pos = -1;

            if (av_interleaved_write_frame(out_context, pkt) < 0)
            {
                status = REMUX_PROCESS_ERR;
                goto end;
            }

            av_packet_unref(pkt);
        }
    }

    // ошибка чтения
    if (read_status != AVERROR_EOF)
    {
        status = REMUX_PROCESS_ERR;
        goto end;
    }

    // подана длительность - необходимо её рассчитать
    if (duration_us)
    {
        if (first_video_time != AV_NOPTS_VALUE &&
            last_video_time > first_video_time)
        {
            *duration_us = last_video_time - first_video_time;
        }
        else if (in_context->duration > 0)
        {
            *duration_us = in_context->duration;
        }
        else
        {
            *duration_us = 0;
        }
    }

    // выполняется если выше не возникло ошибок
    // сброс остаточных кадров из фильтров
    for(size_t i = 0; i < stream_context->size; i++)
    {
        int out_stream_ind = stream_context->mapping[i];
        AVBSFContext *bsf_ctx = stream_context->bsf_contexts[i];
        AVStream *out_stream = NULL;
        int bsf_status = 0;

        // потоки, у которых нет фильтра и которых нет в маппинге, игнорируются
        if(out_stream_ind < 0 || !bsf_ctx)
        {
            continue;
        }

        out_stream = out_context->streams[out_stream_ind];

        // NULL это сигнал для фильтра отпустить имеющиеся у него кадры
        if (av_bsf_send_packet(bsf_ctx, NULL) < 0)
        {
            status = REMUX_PROCESS_ERR;
            goto end;
        }

        // пока у фильтра есть кадры (может быть больше 1)
        while ((bsf_status = av_bsf_receive_packet(bsf_ctx, pkt)) == 0)
        {
            // явное обозначение принадлежности кадра к выходному потоку
            pkt->stream_index = out_stream_ind;

            // конвертация временных меток, нужно для преобразования time_base
            av_packet_rescale_ts(pkt, bsf_ctx->time_base_out, out_stream->time_base);

            // поток не позиционируемый (отправка по сети)
            pkt->pos = -1;

            // запись пакета в выходной файл
            if (av_interleaved_write_frame(out_context, pkt) < 0)
            {
                status = REMUX_PROCESS_ERR;
                goto end;
            }

            av_packet_unref(pkt);
        }

        if (bsf_status != AVERROR(EAGAIN) &&
            bsf_status != AVERROR_EOF)
        {
            status = REMUX_PROCESS_ERR;
            goto end;
        }
    }


    end:
        av_packet_free(&pkt);
        return status;
}

/**
 * @brief получить имя кодека для фильтра
 *
 * @param id идентификатор кодека
 * @return const char* имя кодека. NULL, если кодека с таким id нет
 */
static const char *get_bsf_codec_name(const enum AVCodecID id)
{
    static const struct codec_map_t bsf_rules[] =
    {
        { AV_CODEC_ID_H264, "h264_mp4toannexb" },
        { AV_CODEC_ID_HEVC, "hevc_mp4toannexb" },
        { AV_CODEC_ID_AV1,  "av1_metadata" },
        { AV_CODEC_ID_NONE, NULL } // маркер конца массива
    };

    for(int i = 0; bsf_rules[i].codec_id != AV_CODEC_ID_NONE; i++)
    {
        if(id == bsf_rules[i].codec_id)
        {
            return bsf_rules[i].bsf_name;
        }
    }
    return NULL;
}

/**
 * @brief поддерживается ли аудиокодек MPEG-TS
 *
 * @param id идентификатор кодека
 * @return const int 1 - истина, 0 - ложь
 */
static int is_audio_supported(const enum AVCodecID id)
{
    return (id == AV_CODEC_ID_AAC ||
            id == AV_CODEC_ID_MP3 ||
            id == AV_CODEC_ID_MP2);
}

/**
 * @brief освобождение памяти маппинга потоков и контекста фильтров
 *
 * @param stream_context контекст потоков
 */
static void free_stream_context(struct stream_context* stream_context)
{
    if (stream_context->bsf_contexts) {
        for (size_t i = 0; i < stream_context->size; i++) {
            if (stream_context->bsf_contexts[i]) av_bsf_free(&stream_context->bsf_contexts[i]);
        }
        av_freep(&stream_context->bsf_contexts);
    }
    av_freep(&stream_context->mapping);
}

/**
 * @brief функция подсчёта битрейта
 *
 * @param duration_us длительность видео в микросекундах
 * @param file_size размер видео-файла
 * @return int64_t битрейт
 */
static int64_t bitrate_calc(
    int64_t duration_us,
    int64_t file_size
)
{
    double duration_sec = (double)duration_us / AV_TIME_BASE;
    int64_t avg_bitrate;

    duration_sec = duration_sec < MIN_VID_DURATION ? MIN_VID_DURATION : duration_sec;
    avg_bitrate = (int64_t)((file_size * BITS_IN_BYTE) / duration_sec);

    return (int64_t)(avg_bitrate * MUXRATE_MULTI);
}
