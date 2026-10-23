#include "reader.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *const SELF_EXE_PATH = "/proc/self/exe";

static void postal_card_reset(postal_card *card);

static void postal_card_reset(postal_card *card)
{
    *card = (postal_card){0};
    card->fd = -1;
    card->map = MAP_FAILED;
    card->meta_status = POSTAL_METADATA_OK;
}

postal_reader_status_t postal_card_open(postal_card *card)
{
    return postal_card_open_file(card, SELF_EXE_PATH);
}

postal_reader_status_t postal_card_open_file(
    postal_card *card,
    const char *path
)
{
    struct stat file_stat = {0};

    if (card == NULL || path == NULL)
    {
        return POSTAL_READER_ERR_ARG;
    }

    postal_card_reset(card);

    card->fd = open(path, O_RDONLY);
    if (card->fd < 0)
    {
        return POSTAL_READER_ERR_OPEN;
    }

    if (fstat(card->fd, &file_stat) != 0
        || !S_ISREG(file_stat.st_mode)
        || file_stat.st_size <= 0)
    {
        postal_card_close(card);
        return POSTAL_READER_ERR_STAT;
    }

    card->map_size = (size_t)file_stat.st_size;

    // Модуль common проверяет сигнатуру, версию, размер структуры, multicast-адрес,
    // порт, битрейт и то, что видео-поток не выходит за границы файла
    card->meta_status = postal_metadata_read(card->fd, &card->meta);
    if (card->meta_status != POSTAL_METADATA_OK)
    {
        postal_card_close(card);
        return POSTAL_READER_ERR_METADATA;
    }

    card->map = mmap(NULL, card->map_size, PROT_READ, MAP_PRIVATE, card->fd, 0);
    if (card->map == MAP_FAILED)
    {
        postal_card_close(card);
        return POSTAL_READER_ERR_MMAP;
    }

    card->video = card->map + card->meta.video_offset;
    card->video_size = card->meta.video_size;
    return POSTAL_READER_OK;
}

void postal_card_close(postal_card *card)
{
    if (card == NULL)
    {
        return;
    }

    if (card->map != MAP_FAILED && card->map != NULL)
    {
        munmap((void *)card->map, card->map_size);
    }

    card->map = MAP_FAILED;

    if (card->fd >= 0)
    {
        close(card->fd);
        card->fd = -1;
    }

    card->video = NULL;
    card->video_size = 0;
    card->map_size = 0;
}

const char *postal_reader_status_string(postal_reader_status_t status)
{
    switch (status)
    {
        case POSTAL_READER_OK:
            return "Success";

        case POSTAL_READER_ERR_ARG:
            return "Invalid arguments for reading the postcard";

        case POSTAL_READER_ERR_OPEN:
            return "Couldn't open the file";

        case POSTAL_READER_ERR_STAT:
            return "Failed to retrieve the size of the postcard file";

        case POSTAL_READER_ERR_MMAP:
            return "Failed to load the postcard file into memory";

        case POSTAL_READER_ERR_METADATA:
            return "Postcard metadata is missing or corrupted";

        default:
            return "[POSTAL_READER] Unknown error";
    }
}