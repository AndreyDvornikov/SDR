#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(void)
{
    // ---- Создание устройства ----
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    if (1) {
        SoapySDRKwargs_set(&args, "uri", "usb:");
    } else {
        SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1");
    }
//    SoapySDRKwargs_set(&args, "direct", "1");
//    SoapySDRKwargs_set(&args, "timestamp_every", "1920");
    SoapySDRKwargs_set(&args, "loopback", "0");

    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL)
    {
        printf("SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }

    // ---- Настройка TX ----
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, 1e6) != 0)
        printf("setSampleRate tx fail: %s\n", SoapySDRDevice_lastError());
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, 800e6, NULL) != 0)
        printf("setFrequency tx fail: %s\n", SoapySDRDevice_lastError());

    printf("SoapySDRDevice_getFrequency tx: %lf\n",
           SoapySDRDevice_getFrequency(sdr, SOAPY_SDR_TX, 0));

    // ---- Каналы ----
    size_t channels[] = {0};
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);

    if (SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels[0], -20.0) != 0)
        printf("setGain tx fail: %s\n", SoapySDRDevice_lastError());

    SoapySDRStream *txStream = SoapySDRDevice_setupStream(
        sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    if (txStream == NULL)
    {
        printf("setupStream tx fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }

    // ---- MTU ----
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);
    printf("MTU TX: %zu\n", tx_mtu);

    int16_t tx_buff[2 * tx_mtu];

    // Заполнение буфера треугольником, I = Q.
    for (size_t i = 0; i < 2 * tx_mtu; i += 2)
    {
        int16_t v;
//        v = (int16_t)2000;
        if (i < tx_mtu)
            v = (int16_t)i;
        else
            v = (int16_t)((2 * tx_mtu) - i);

        tx_buff[i]     = v;
        tx_buff[i + 1] = v;
    }

    // ---- Активация стрима ----
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0);

    printf("Start test...\n");
    const long timeoutUs = 400000;

    // Базовое время берём из железа — оно монотонно растёт.
    long long hw_time = SoapySDRDevice_getHardwareTime(sdr, NULL);
    printf("Hardware time base: %lld ns\n", hw_time);

    // Интервал между пакетами. 1 мс при 1 MSPS и MTU 1920 отсчётов
    // ~ 1.92 мс данных, поэтому 4 мс — безопасный запас в будущее.
    const long long step_ns = 4LL * 1000 * 1000; // 4 мс

    FILE *file = fopen("txdata.pcm", "w");
    if (!file)
        printf("fopen txdata.pcm failed\n");

    size_t iteration_count = 1000000;

    for (size_t it = 0; it < iteration_count; it++)
    {
        // Время отправки: base + (it+1)*step, чтобы гарантированно
        // быть в будущем относительно текущего hw времени.
        long long tx_time = hw_time + (long long)(it + 1) * step_ns;

        // Пакуем tx_time в первые 8 "слов" после флага FFFF.
        // Формат из комментария: FFFF FFFF [TS_0]00 ... [TS_7]00 FFFF FFFF
        for (size_t i = 0; i < 2; i++)
        {
            tx_buff[0 + i]  = (int16_t)0xffff;
            tx_buff[10 + i] = (int16_t)0xffff;
        }
        for (size_t i = 0; i < 8; i++)
        {
            uint8_t b = (uint8_t)((tx_time >> (i * 8)) & 0xff);
            tx_buff[2 + i] = (int16_t)(b << 4); // DAC left-aligned 12-bit
        }

        // Пишем в файл, если открыт
        if (file)
            fwrite(tx_buff, sizeof(int16_t), 2 * tx_mtu, file);

        // Отправка
        void *tx_buffs[] = { tx_buff };
        int flags = SOAPY_SDR_HAS_TIME;

        printf("iter %zu: tx_time = %lld ns\n", it, tx_time);

        int st = SoapySDRDevice_writeStream(
            sdr, txStream,
            (const void * const *)tx_buffs,
            tx_mtu, &flags, tx_time, timeoutUs);

        if (st < 0)
            printf("TX Failed: %i (%s)\n", st, SoapySDRDevice_lastError());
        else if ((size_t)st != tx_mtu)
            printf("TX short write: %i из %zu\n", st, tx_mtu);
    }

    if (file)
        fclose(file);

    // ---- Остановка ----
    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, txStream);
    SoapySDRDevice_unmake(sdr);

    printf("test complete!\n");
    return EXIT_SUCCESS;
}
