#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(void)
{
    SoapySDRKwargs args = {};
    SoapySDRDevice *sdr;

    // Если у вас тоже usb:
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    SoapySDRKwargs_set(&args, "uri", "usb:"); 
    
    sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL) {
        printf("SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }

    // Настройка RX
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, 1e6);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, 800e6, NULL);
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, 40.0); // Высокое усиление для приема

    size_t channels[] = {0};
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, 1, NULL);
    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);

    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    printf("MTU RX: %lu\n", rx_mtu);

    int16_t rx_buffer[2 * rx_mtu];
    
    // Открываем файл для записи
    FILE *file = fopen("rxdata.pcm", "wb");
    if (!file) {
        printf("Не удалось открыть файл для записи!\n");
        return EXIT_FAILURE;
    }

    printf("Начинаем прием... Ждем сигнал от друга.\n");
    printf("Нажмите Ctrl+C для остановки, или ждите завершения цикла.\n");

    // Записываем большой буфер (например, 2000 итераций)
    // 2000 * tx_mtu сэмплов - это несколько секунд эфира. Сигнал друга точно туда попадет.
    size_t iteration_count = 2000; 
    
    for (size_t i = 0; i < iteration_count; i++) {
        void *rx_buffs[] = {rx_buffer};
        int flags;
        long long timeNs;
        
        // Читаем с таймаутом 1 секунда
        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, 1000000);
        
        if (sr > 0) {
            // Записываем в файл
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