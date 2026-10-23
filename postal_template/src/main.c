/**
 * @file main.c
 * @brief Точка входа шаблона-открытки postal_template.
 *
 * При запуске открытка находит собственный файл, читает и проверяет вшитые
 * в его конец метаданные (#5281), после чего вещает видео по UDP-multicast
 * по кругу до получения SIGINT или SIGTERM (#5283).
 */

#include "broadcaster.h"
#include "reader.h"

#include <signal.h>
#include <stdio.h>

enum
{
    // Ширина колонки подписей в сводке об открытке
    CARD_INFO_LABEL_WIDTH = 17,
};

static volatile sig_atomic_t stop_requested = 0;

static void handle_stop_signal(int signum);
static int install_stop_handlers(void);
static void print_card_info(const postal_card *card);

/**
 * @brief Обработчик SIGINT/SIGTERM: запросить остановку вещания.
 *
 * @param signum номер сигнала
 */
static void handle_stop_signal(int signum)
{
    (void)signum;
    stop_requested = 1;
}

/**
 * @brief Установить обработчики сигналов остановки.
 *
 * SA_RESTART не выставляется, чтобы сигнал прерывал сон между пакетами.
 *
 * @return 0 при успехе, -1 при ошибке
 */
static int install_stop_handlers(void)
{
    struct sigaction action = {0};

    action.sa_handler = handle_stop_signal;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) != 0
        || sigaction(SIGTERM, &action, NULL) != 0)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief Напечатать сводку о найденных в открытке данных.
 *
 * @param card проверенная открытка
 */
static void print_card_info(const postal_card *card)
{
    printf("Postcard is intact, embedded data loaded:\n");
    printf(
        "  %-*s : %u.%u.%u.%u:%u\n",
        CARD_INFO_LABEL_WIDTH,
        "multicast address",
        (unsigned)card->meta.multicast_addr[0],
        (unsigned)card->meta.multicast_addr[1],
        (unsigned)card->meta.multicast_addr[2],
        (unsigned)card->meta.multicast_addr[3],
        (unsigned)card->meta.udp_port
    );
    printf(
        "  %-*s : offset %llu, size %llu bytes\n",
        CARD_INFO_LABEL_WIDTH,
        "video stream",
        (unsigned long long)card->meta.video_offset,
        (unsigned long long)card->meta.video_size
    );
    printf(
        "  %-*s : %llu bit/s\n",
        CARD_INFO_LABEL_WIDTH,
        "bitrate",
        (unsigned long long)card->meta.bitrate
    );
}

int main(void)
{
    postal_card card = {0};
    postal_reader_status_t postal_ret = postal_card_open(&card);
    postal_broadcast_status_t broadcast_ret = POSTAL_BROADCAST_OK;

    if (postal_ret != POSTAL_READER_OK)
    {
        fprintf(stderr, "Error: %s", postal_reader_status_string(postal_ret));

        // Для повреждённых метаданных модуль common сообщает точную причину
        if (postal_ret == POSTAL_READER_ERR_METADATA)
        {
            fprintf(stderr, " (%s)", postal_metadata_status_string(card.meta_status));
        }

        fprintf(stderr, "\n");
        return 1;
    }

    print_card_info(&card);

    if (install_stop_handlers() != 0)
    {
        fprintf(stderr, "Error: Failed to install signal handlers.\n");
        postal_card_close(&card);
        return 1;
    }

    printf("Broadcasting, press Ctrl+C to stop.\n");
    fflush(stdout);

    broadcast_ret = postal_broadcast_run(&card, &stop_requested);
    postal_card_close(&card);

    if (broadcast_ret != POSTAL_BROADCAST_OK)
    {
        fprintf(stderr, "Error: %s\n", postal_broadcast_status_string(broadcast_ret));
        return 1;
    }

    printf("Broadcast stopped.\n");
    return 0;
}