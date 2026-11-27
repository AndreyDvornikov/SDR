#pragma once

#include "dsp/modulation.hpp"
#include <string>
#include <cstdint>
#include <atomic>

namespace sdr {

enum class SDRMode {
    Idle,
    TxOnly,
    RxOnly,
    FullDuplex
};

enum class SDRDeviceType {
    SoapyPluto,
    FilePlayback
};

struct SDRConfig {
    // Режим радиотракта: FullDuplex (передача и приём), TxOnly (только TX), RxOnly (только RX)
    SDRMode sdr_mode = SDRMode::FullDuplex;

    // Несущая частота: строго 800 МГц или 850 МГц (как в FREQUENCIES_HZ из main.cpp)
    double frequency = 800e6;

    // Аппаратный цифровой loopback (шлейф: 0 = выкл, 1 = вкл)
    bool loopback = false;

    // Усиление и амплитуда (дефолты из main.cpp)
    double tx_gain = -20.0;         // дБ (-20.0 dB в main.cpp)
    double rx_gain = 40.0;          // дБ (+40.0 dB в main.cpp)
    float tx_amplitude = 2000.0f;   // Амплитуда отсчетов ЦАП (TX_AMPLITUDE = 2000.0f)

    // Модуляция и свёртка
    dsp::ModulationType modulation = dsp::ModulationType::QPSK; // QPSK или BPSK
    size_t upsample_factor = 10;    // 10 отсчетов на символ

    // Полезная нагрузка пакета
    std::string tx_payload = "SIBGUTI_IKS431";

    // Приемник и цифровая обработка
    bool rx_matched_filter = true;  // Согласованный фильтр (прямоугольная свёртка)
    bool rx_cfo_correction = true;  // Компенсатор вращения частоты (CFO)
    float rx_cfo_hz = -1696.25f;    // Сдвиг гетеродина Pluto (-1696.25 Гц)
    bool auto_symbol_sync = true;
    size_t strobe_phase = 5;
};

struct SDRMetrics {
    std::atomic<bool> is_running{false};
    std::atomic<bool> is_transmitting{false};
    std::atomic<bool> is_receiving{false};
    std::atomic<bool> is_connected{false}; // true если физическая плата подключена

    // Реальные счётчики пакетов и сэмплов
    std::atomic<uint64_t> tx_packets{0};
    std::atomic<uint64_t> rx_packets{0};
    std::atomic<uint64_t> tx_samples_total{0};
    std::atomic<uint64_t> rx_samples_total{0};

    // Реальные физические измерения уровня сигнала (RMS и Peak)
    std::atomic<float> rx_rms_level{0.0f};
    std::atomic<float> rx_peak_level{0.0f};

    std::atomic<int64_t> last_hw_timestamp{0};
    std::atomic<int> stream_errors{0};

    char device_name[128] = "Офлайн PCM";
    char last_error[256] = "";
};

} // namespace sdr
