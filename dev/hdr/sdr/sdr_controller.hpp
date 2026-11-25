#pragma once

#include "sdr/sdr_types.hpp"
#include "sdr/sdr_device.hpp"
#include "sdr/pcm_player.hpp"
#include "dsp/fft.hpp"
#include "dsp/ring_buffer.hpp"

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

namespace sdr {

enum class SignalSource {
    PCMFile = 0,
    ADALM_Pluto = 1
};

class SDRController {
public:
    SDRController();
    ~SDRController();

    // Источник сигнала (Офлайн PCM или ADALM-Pluto)
    SignalSource get_source() const { return current_source_; }
    void set_source(SignalSource src);

    // Управление PCM плеером
    bool load_pcm_file(const std::string& filepath);
    static std::string resolve_pcm_path(const std::string& filepath);
    PCMPlayer& get_pcm_player() { return pcm_player_; }
    const PCMPlayer& get_pcm_player() const { return pcm_player_; }

    // Запуск/остановка радиотракта
    bool start();
    void stop();
    bool is_running() const { return metrics_.is_running.load(); }

    // Конфигурация
    SDRConfig get_config() const;
    void set_config(const SDRConfig& config);
    void apply_config_changes();
    void set_loopback(bool enabled);
    void set_frequency(double freq_hz);
    void set_sdr_mode(SDRMode mode);

    // Метрики
    const SDRMetrics& get_metrics() const { return metrics_; }

    // Получение снапшотов для UI (потокобезопасно)
    void get_tx_time_domain(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_samples = 512);
    void get_rx_time_domain(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_samples = 512);
    void get_rx_filtered_time_domain(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_samples = 512);
    void get_tx_constellation(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_points = 256);
    void get_rx_constellation(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_points = 256);
    size_t get_new_rx_symbols(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_points = 512);
    void get_spectrum(std::vector<float>& out_freq, std::vector<float>& out_power);
    void get_tx_spectrum(std::vector<float>& out_freq, std::vector<float>& out_power);
    void get_waterfall(const float** out_data, size_t* out_w, size_t* out_h);

    // Сброс буферов и истории
    void clear_buffers();

private:
    void tx_loop();
    void rx_loop();

    std::unique_ptr<ISDRDevice> device_;
    PCMPlayer pcm_player_;
    SignalSource current_source_{SignalSource::PCMFile};
    SDRConfig config_;
    mutable std::mutex config_mutex_;

    SDRMetrics metrics_;

    std::atomic<bool> should_stop_{false};
    std::thread tx_thread_;
    std::thread rx_thread_;

    // Потокобезопасные буферы сигналов для UI
    dsp::ThreadSafeRingBuffer<std::complex<float>> tx_ring_buffer_{32768};
    dsp::ThreadSafeRingBuffer<std::complex<float>> rx_ring_buffer_{32768};
    dsp::ThreadSafeRingBuffer<std::complex<float>> rx_filtered_buffer_{32768};
    dsp::ThreadSafeRingBuffer<std::complex<float>> tx_symbols_buffer_{4096};
    dsp::ThreadSafeRingBuffer<std::complex<float>> rx_symbols_buffer_{4096};

    // БПФ и водопад
    dsp::FFTProcessor fft_processor_{1024};
    dsp::FFTProcessor tx_fft_processor_{1024};
    dsp::WaterfallBuffer waterfall_{256, 128};
    std::mutex dsp_mutex_;
    std::vector<float> latest_freq_axis_;
    std::vector<float> latest_power_spectrum_;
    std::vector<float> latest_tx_freq_axis_;
    std::vector<float> latest_tx_power_spectrum_;

    // CFO compensation state for RX
    float rx_cfo_phase_{0.0f};
};

} // namespace sdr
