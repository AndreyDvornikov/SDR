#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>   // printf
#include <stdlib.h>  // free, EXIT_*
#include <stdint.h>
#include <string.h>  // strcmp
#include <complex.h>

// ---- Общие параметры радиотракта ----
static const double SAMPLE_RATE  = 1e6;   // 1 MSPS
static const double CARRIER_FREQ = 800e6; // 800 МГц
static const double RX_GAIN      = 40.0;  // высокое усиление приёма
static const double TX_GAIN      = -20.0; // умеренное усиление передачи

enum class Mode { Tx, Rx };

void print_usage(const char *prog)
{
    printf("Использование: %s <--tx | --rx>\n"
           "  --tx : режим передачи (треугольник + маркеры + timestamp, пишет txdata.pcm)\n"
           "  --rx : режим приёма (пишет принятые IQ-отсчёты в rxdata.pcm)\n", prog);
}

// ---- Создание устройства ----
SoapySDRDevice *make_device(void)
{
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    SoapySDRKwargs_set(&args, "uri", "usb:");
    // SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1"); // альтернатива по сети
    // SoapySDRKwargs_set(&args, "direct", "1");
    // SoapySDRKwargs_set(&args, "timestamp_every", "1920");
    SoapySDRKwargs_set(&args, "loopback", "0");

    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);
    return sdr;
}

// ================= РЕЖИМ ПЕРЕДАЧИ (бывший tx.cpp) =================
int run_tx(SoapySDRDevice *sdr)
{
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, SAMPLE_RATE) != 0)
        printf("setSampleRate tx fail: %s\n", SoapySDRDevice_lastError());
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, CARRIER_FREQ, NULL) != 0)
        printf("setFrequency tx fail: %s\n", SoapySDRDevice_lastError());
    printf("SoapySDRDevice_getFrequency tx: %lf\n",
           SoapySDRDevice_getFrequency(sdr, SOAPY_SDR_TX, 0));

    size_t channels[] = {0};
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);

    if (SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels[0], TX_GAIN) != 0)
        printf("setGain tx fail: %s\n", SoapySDRDevice_lastError());

    SoapySDRStream *txStream = SoapySDRDevice_setupStream(
        sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    if (txStream == NULL) {
        printf("setupStream tx fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }

    size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);
    printf("MTU TX: %zu\n", tx_mtu);

    int16_t tx_buff[2 * tx_mtu];

    // Заполнение буфера треугольником, I = Q.
    for (size_t i = 0; i < 2 * tx_mtu; i += 2) {
        int16_t v;
        if (i < tx_mtu)
            v = (int16_t)i;
        else
            v = (int16_t)((2 * tx_mtu) - i);
        tx_buff[i]     = v;
        tx_buff[i + 1] = v;
    }

    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0);

    printf("Start test...\n");
    const long timeoutUs = 400000; // взято из srsRAN

    // Базовое время берём из железа — оно монотонно растёт.
    long long hw_time = SoapySDRDevice_getHardwareTime(sdr, NULL);
    printf("Hardware time base: %lld ns\n", hw_time);

    // Интервал между пакетами: 4 мс — безопасный запас в будущее
    // (при 1 MSPS и MTU 32768 один буфер ~ 32 мс данных, но шаг нужен
    //  для ритмичной отправки пакетов с метками времени).
    const long long step_ns = 4LL * 1000 * 1000;

    FILE *file = fopen("txdata.pcm", "w");
    if (!file)
        printf("fopen txdata.pcm failed\n");

    // Практически "бесконечно" — останавливаем Ctrl+C.
    // ВНИМАНИЕ: файл txdata.pcm растёт на ~4*tx_mtu байт за итерацию,
    // не держите передачу включенной часами.
    size_t iteration_count = 1000000;

    for (size_t it = 0; it < iteration_count; it++) {
        // Время отправки: base + (it+1)*step, гарантированно в будущем.
        long long tx_time = hw_time + (long long)(it + 1) * step_ns;

        // Пакуем tx_time в слова после флага FFFF:
        // FFFF FFFF [TS_0]00 ... [TS_7]00 FFFF FFFF
        for (size_t i = 0; i < 2; i++) {
            tx_buff[0 + i]  = (int16_t)0xffff;
            tx_buff[10 + i] = (int16_t)0xffff;
        }
        for (size_t i = 0; i < 8; i++) {
            uint8_t b = (uint8_t)((tx_time >> (i * 8)) & 0xff);
            tx_buff[2 + i] = (int16_t)(b << 4); // DAC left-aligned 12-bit
        }

        if (file)
            fwrite(tx_buff, sizeof(int16_t), 2 * tx_mtu, file);

        void *tx_buffs[] = { tx_buff };
        int flags = SOAPY_SDR_HAS_TIME;

        // printf("iter %zu: tx_time = %lld ns\n", it, tx_time); // можно раскомментировать для отладки

        int st = SoapySDRDevice_writeStream(
            sdr, txStream, (const void * const *)tx_buffs,
            tx_mtu, &flags, tx_time, timeoutUs);

        if (st < 0)
            printf("TX Failed: %i (%s)\n", st, SoapySDRDevice_lastError());
        else if ((size_t)st != tx_mtu)
            printf("TX short write: %i из %zu\n", st, tx_mtu);
    }

    if (file)
        fclose(file);

    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, txStream);
    SoapySDRDevice_unmake(sdr);

    printf("test complete!\n");
    return EXIT_SUCCESS;
}

// ================= РЕЖИМ ПРИЁМА (бывший rx.cpp) =================
int run_rx(SoapySDRDevice *sdr)
{
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, SAMPLE_RATE);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, CARRIER_FREQ, NULL);
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, RX_GAIN);

    size_t channels[] = {0};
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(
        sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, 1, NULL);
    if (rxStream == NULL) {
        printf("setupStream rx fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }
    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);

    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    printf("MTU RX: %zu\n", rx_mtu);

    int16_t rx_buffer[2 * rx_mtu];

    FILE *file = fopen("rxdata.pcm", "wb");
    if (!file) {
        printf("Не удалось открыть файл для записи!\n");
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }

    printf("Начинаем прием... Ждем сигнал от друга.\n");

    // 2000 итераций — это десятки секунд эфира, сигнал друга точно попадёт.
    size_t iteration_count = 2000;

    for (size_t i = 0; i < iteration_count; i++) {
        void *rx_buffs[] = {rx_buffer};
        int flags;
        long long timeNs;

        int sr = SoapySDRDevice_readStream(
            sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, 1000000);

        if (sr > 0) {
            fwrite(rx_buffer, 2 * sr * sizeof(int16_t), 1, file);
        } else if (sr < 0) {
            printf("Ошибка чтения: %s\n", SoapySDRDevice_lastError());
        }
    }

    fclose(file);
    printf("Прием завершен. Файл rxdata.pcm сохранен.\n");

    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_unmake(sdr);

    return EXIT_SUCCESS;
}

// ================= ТОЧКА ВХОДА =================
int main(int argc, char *argv[])
{
    if (argc != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    Mode mode;
    if (strcmp(argv[1], "--tx") == 0)      mode = Mode::Tx;
    else if (strcmp(argv[1], "--rx") == 0) mode = Mode::Rx;
    else {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    SoapySDRDevice *sdr = make_device();
    if (sdr == NULL) {
        printf("SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }

    return (mode == Mode::Tx) ? run_tx(sdr) : run_rx(sdr);
}