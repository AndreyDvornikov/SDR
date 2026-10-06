#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>   // printf
#include <stdlib.h>  // free, EXIT_*
#include <stdint.h>
#include <complex.h>

int main(void)
{
    // ---- Создание сущности устройства ----
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    if (1) {
        SoapySDRKwargs_set(&args, "uri", "usb:");
    } else {
        SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1");
    }
    SoapySDRKwargs_set(&args, "direct", "1");
    SoapySDRKwargs_set(&args, "timestamp_every", "1920");
    SoapySDRKwargs_set(&args, "loopback", "0");
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL)
    {
        printf("SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }

    // ---- Настройка RX / TX ----
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, 1e6) != 0)
        printf("setSampleRate rx fail: %s\n", SoapySDRDevice_lastError());
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, 800e6, NULL) != 0)
        printf("setFrequency rx fail: %s\n", SoapySDRDevice_lastError());
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, 1e6) != 0)
        printf("setSampleRate tx fail: %s\n", SoapySDRDevice_lastError());
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, 800e6, NULL) != 0)
        printf("setFrequency tx fail: %s\n", SoapySDRDevice_lastError());
    printf("SoapySDRDevice_getFrequency tx: %lf\n",
           SoapySDRDevice_getFrequency(sdr, SOAPY_SDR_TX, 0));

    // ---- Каналы ----
    size_t channels[] = {0}; // {0} or {0, 1}
    // TODO: с {0, 1} разобраться — 2 канала не работают
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);

    // ---- RX stream ----
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(
        sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    if (rxStream == NULL)
    {
        printf("setupStream rx fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }
    if (SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels[0], 30.0) != 0)
        printf("setGain rx fail: %s\n", SoapySDRDevice_lastError());

    // ---- TX stream ----
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(
        sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    if (txStream == NULL)
    {
        printf("setupStream tx fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }
    if (SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels[0], -40.0) != 0)
        printf("setGain tx fail: %s\n", SoapySDRDevice_lastError());

    // ---- MTU ----
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);
    printf("MTU - TX: %zu, RX: %zu\n", tx_mtu, rx_mtu);

    int16_t tx_buff[2 * tx_mtu];
    int16_t rx_buffer[2 * rx_mtu];

    // Заполнение tx_buff: первые 16 бит - I, вторые 16 бит - Q.
    // Пила (как в твоём main.cpp):
    for (size_t i = 0; i < 2 * tx_mtu; i += 2)
    {
        if (i < tx_mtu) {
            tx_buff[i]     = (int16_t)i;
            tx_buff[i + 1] = (int16_t)i;
        } else {
            tx_buff[i]     = (int16_t)((2 * tx_mtu) - i);
            tx_buff[i + 1] = (int16_t)((2 * tx_mtu) - i);
        }
    }

    // Фиксированные байты в TX-буфере:
    // FFFF FFFF [TS_0]00 ... [TS_7]00 FFFF FFFF
    for (size_t i = 0; i < 2; i++)
    {
        tx_buff[0 + i]  = 0xffff;
        tx_buff[10 + i] = 0xffff;
    }

    // ---- Запуск стримов ----
    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0);

    printf("Start test...\n");
    const long timeoutUs = 400000; // взято из srsRAN
    long long last_time = 0;

    FILE *file = fopen("txdata.pcm", "w");
    if (!file) {
        printf("fopen txdata.pcm failed\n");
        // тут можно было бы аккуратно всё закрыть, но для теста оставим как есть
    }

    size_t iteration_count = 10;

    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++)
    {
        void *rx_buffs[] = {rx_buffer};
        int flags = 0;
        long long timeNs = 0;

        int sr = SoapySDRDevice_readStream(
            sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        if (sr < 0)
        {
            // Пропускаем ошибку чтения (обычно таймаут)
            printf("readStream rx error/timeout: %i\n", sr);
            continue;
        }

        printf("Buffer: %zu - Samples: %i, Flags: %i, Time: %lld, TimeDiff: %lld\n",
               buffers_read, sr, flags, timeNs, timeNs - last_time);

        if (file)
            fwrite(rx_buffer, 2 * rx_mtu * sizeof(int16_t), 1, file);
        last_time = timeNs;

        // Время передачи: на 4 мс в будущее от времени приёма
        long long tx_time = timeNs + (4 * 1000 * 1000);

        // Раскладываем tx_time побайтно в tx_buff
        for (size_t i = 0; i < 8; i++)
        {
            uint8_t tx_time_byte = (uint8_t)((tx_time >> (i * 8)) & 0xff);
            tx_buff[2 + i] = (int16_t)(tx_time_byte << 4);
        }

        // Отправка TX-буфера
        void *tx_buffs[] = {tx_buff};
        if (buffers_read == 2)
        {
            printf("buffers_read: %zu\n", buffers_read);
            flags = SOAPY_SDR_HAS_TIME;
            int st = SoapySDRDevice_writeStream(
                sdr, txStream, (const void * const *)tx_buffs,
                tx_mtu, &flags, tx_time, timeoutUs);
            if ((size_t)st != tx_mtu)
                printf("TX Failed: %i\n", st);
        }
    }

    if (file)
        fclose(file);

    // ---- Остановка стримов ----
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);

    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_closeStream(sdr, txStream);

    SoapySDRDevice_unmake(sdr);

    // Process each rx buffer, looking for transmitted timestamp
    // (закомментировано — как в эталоне)
    // for (size_t j = 0; j < channel_count; j++) {
    //     printf("Checking channel %zu\n", j);
    //     check_channel(sample_count, channel_count, j,
    //                   tx_timestamps, rx_timestamps,
    //                   (uint16_t**)rx_buff, rx_mtu);
    // }

    printf("test complete!\n");
    return EXIT_SUCCESS;
}
