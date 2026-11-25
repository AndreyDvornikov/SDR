#include "sdr/sdr_controller.hpp"
#include "sdr/sdr_soapy.hpp"
#include "dsp/modulation.hpp"
#include "dsp/filters.hpp"

#include <chrono>
#include <cmath>
#include <thread>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace sdr {

constexpr float PI = 3.14159265358979323846f;

SDRController::SDRController() {
    latest_freq_axis_.resize(1024, 0.0f);
    latest_power_spectrum_.resize(1024, -100.0f);
    latest_tx_freq_axis_.resize(1024, 0.0f);
    latest_tx_power_spectrum_.resize(1024, -100.0f);

    // Автоматическая загрузка базового файла rxdata.pcm
    load_pcm_file("rxdata.pcm");
}

SDRController::~SDRController() {
    stop();
}

std::string SDRController::resolve_pcm_path(const std::string& filepath) {
    if (std::filesystem::exists(filepath)) {
        return filepath;
    }

    std::filesystem::path p(filepath);
    std::string filename = p.filename().string();

    const std::string search_locations[] = {
        filepath,
        "pcms/" + filename,
        "../pcms/" + filename,
        "../../pcms/" + filename,
        "/home/Avadik/Prog/SDR/pcms/" + filename,
        "pcms/backup_clean_20261110/" + filename,
        "../pcms/backup_clean_20261110/" + filename,
        "../../pcms/backup_clean_20261110/" + filename,
        "/home/Avadik/Prog/SDR/pcms/backup_clean_20261110/" + filename
    };

    for (const auto& loc : search_locations) {
        if (std::filesystem::exists(loc)) {
            return loc;
        }
    }
    return filepath;
}

void SDRController::set_source(SignalSource src) {
    if (current_source_ == src && metrics_.is_running.load()) return;
    current_source_ = src;

    if (src == SignalSource::PCMFile) {
        if (device_) {
            device_->stop_stream();
            device_->close();
            device_.reset();
        }
        metrics_.is_connected.store(false);
        std::snprintf(metrics_.device_name, sizeof(metrics_.device_name), "Файл: %s",
                      pcm_player_.is_loaded() ? pcm_player_.current_filename().c_str() : "Не загружен");
        pcm_player_.play();
        if (!metrics_.is_running.load()) {
            start();
        }
    } else if (src == SignalSource::ADALM_Pluto) {
        pcm_player_.pause();
        start();
    }
}

bool SDRController::load_pcm_file(const std::string& filepath) {
    std::string resolved = resolve_pcm_path(filepath);
    if (pcm_player_.load_file(resolved)) {
        current_source_ = SignalSource::PCMFile;
        std::snprintf(metrics_.device_name, sizeof(metrics_.device_name), "Файл: %s",
                      pcm_player_.current_filename().c_str());
        metrics_.is_connected.store(false);
        clear_buffers();

        // Авто-пресет для данных реального эфира СибГУТИ (Лаб. 8)
        if (resolved.find("rxdata.pcm") != std::string::npos) {
            std::lock_guard<std::mutex> lock(config_mutex_);
            config_.frequency = 800e6;
            config_.upsample_factor = 10;
            config_.modulation = dsp::ModulationType::QPSK;
            config_.rx_matched_filter = true;
            config_.rx_cfo_correction = true;
            config_.rx_cfo_hz = -1696.25f; // Точный сдвиг частоты гетеродина в Лаб. 8
        }

        pcm_player_.play();

        if (!metrics_.is_running.load()) {
            start();
        }
        return true;
    }
    return false;
}

SDRConfig SDRController::get_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

void SDRController::set_config(const SDRConfig& config) {
    bool restart_needed = false;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        restart_needed = (config_.loopback != config.loopback) || (config_.sdr_mode != config.sdr_mode);
        config_ = config;
    }

    if (restart_needed && current_source_ == SignalSource::ADALM_Pluto) {
        // Перезапускаем устройство с новым флагом loopback или режимом SDR
        start();
    } else {
        apply_config_changes();
    }
}

void SDRController::set_loopback(bool enabled) {
    SDRConfig cfg = get_config();
    if (cfg.loopback != enabled) {
        cfg.loopback = enabled;
        set_config(cfg);
    }
}

void SDRController::set_sdr_mode(SDRMode mode) {
    SDRConfig cfg = get_config();
    if (cfg.sdr_mode != mode) {
        cfg.sdr_mode = mode;
        set_config(cfg);
    }
}

void SDRController::set_frequency(double freq_hz) {
    SDRConfig cfg = get_config();
    cfg.frequency = freq_hz;
    set_config(cfg);
}

void SDRController::apply_config_changes() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (device_) {
        device_->update_config(config_);
    }
}

bool SDRController::start() {
    stop();
    should_stop_.store(false);

    SDRConfig cfg = get_config();

    if (current_source_ == SignalSource::ADALM_Pluto) {
        auto soapy = std::make_unique<SoapySDRBackend>();
        if (soapy->open(cfg) && soapy->start_stream(cfg.sdr_mode)) {
            device_ = std::move(soapy);
            metrics_.is_connected.store(true);
            const char* mode_tag = (cfg.sdr_mode == SDRMode::TxOnly) ? " TX" :
                                   ((cfg.sdr_mode == SDRMode::RxOnly) ? " RX" :
                                    (cfg.loopback ? " Loopback" : " Дуплекс"));
            std::snprintf(metrics_.device_name, sizeof(metrics_.device_name), "ADALM-PLUTO (USB%s)", mode_tag);
            metrics_.last_error[0] = '\0';
        } else {
            metrics_.is_connected.store(false);
            std::snprintf(metrics_.last_error, sizeof(metrics_.last_error),
                          "Не удалось открыть PlutoSDR. Проверьте подключение USB.");
            std::snprintf(metrics_.device_name, sizeof(metrics_.device_name), "Плата не подключена");
        }
    } else {
        metrics_.is_connected.store(false);
        std::snprintf(metrics_.device_name, sizeof(metrics_.device_name), "Файл: %s",
                      pcm_player_.is_loaded() ? pcm_player_.current_filename().c_str() : "Не загружен");
    }

    bool can_tx = (cfg.sdr_mode == SDRMode::FullDuplex || cfg.sdr_mode == SDRMode::TxOnly);
    bool can_rx = (cfg.sdr_mode == SDRMode::FullDuplex || cfg.sdr_mode == SDRMode::RxOnly);

    metrics_.is_running.store(true);
    metrics_.is_transmitting.store(current_source_ == SignalSource::ADALM_Pluto && device_ != nullptr && can_tx);
    metrics_.is_receiving.store(current_source_ == SignalSource::PCMFile || can_rx);

    tx_thread_ = std::thread(&SDRController::tx_loop, this);
    rx_thread_ = std::thread(&SDRController::rx_loop, this);

    return true;
}

void SDRController::stop() {
    should_stop_.store(true);

    if (tx_thread_.joinable()) tx_thread_.join();
    if (rx_thread_.joinable()) rx_thread_.join();

    if (device_) {
        device_->stop_stream();
        device_->close();
        device_.reset();
    }

    metrics_.is_running.store(false);
    metrics_.is_transmitting.store(false);
    metrics_.is_receiving.store(false);
}

void SDRController::clear_buffers() {
    tx_ring_buffer_.clear();
    rx_ring_buffer_.clear();
    rx_filtered_buffer_.clear();
    tx_symbols_buffer_.clear();
    rx_symbols_buffer_.clear();

    std::lock_guard<std::mutex> lock(dsp_mutex_);
    waterfall_.clear();
    std::fill(latest_power_spectrum_.begin(), latest_power_spectrum_.end(), -100.0f);
}

void SDRController::tx_loop() {
    while (!should_stop_.load()) {
        SDRConfig cfg = get_config();

        // 1. Формирование бит полезной нагрузки (Лаб. 8)
        std::string payload = cfg.tx_payload.empty() ? "SIBGUTI_IKS431" : cfg.tx_payload;
        auto bits = dsp::string_to_bits(payload);

        // 2. Модуляция (QPSK или BPSK)
        auto sym = (cfg.modulation == dsp::ModulationType::BPSK) ? dsp::bpsk(bits) : dsp::qpsk(bits);
        tx_symbols_buffer_.write_overwrite(sym.data(), sym.size());

        // 3. Апсемплинг (фактор 10)
        constexpr size_t UPSAMPLE_FACTOR = 10;
        auto upsampled = dsp::upsample(sym, UPSAMPLE_FACTOR);

        // 4. Прямоугольная свёртка (h_size = 10, scale = 1.0f)
        auto signal = dsp::convolution(upsampled, UPSAMPLE_FACTOR, 1.0f);
        tx_ring_buffer_.write_overwrite(signal.data(), signal.size());

        // Расчет БПФ спектра для TX
        {
            std::lock_guard<std::mutex> lock(dsp_mutex_);
            std::vector<std::complex<float>> rep;
            rep.reserve(1024);
            while (rep.size() < 1024 && !signal.empty()) {
                size_t chunk = std::min(size_t(1024) - rep.size(), signal.size());
                rep.insert(rep.end(), signal.begin(), signal.begin() + chunk);
            }
            if (rep.size() == 1024) {
                tx_fft_processor_.compute_power_spectrum(rep, latest_tx_freq_axis_, latest_tx_power_spectrum_, 1e6, cfg.frequency);
            }
        }

        // 5. Непрерывная передача данных в эфир ("данные данные данные данные")
        // Без пауз и пустых буферов тишины!
        bool can_tx = (cfg.sdr_mode == SDRMode::FullDuplex || cfg.sdr_mode == SDRMode::TxOnly);
        if (device_ && current_source_ == SignalSource::ADALM_Pluto && can_tx) {
            size_t mtu = device_->get_mtu();
            if (mtu == 0) mtu = 1024;

            // Заполняем непрерывный буфер отсчетами сигнала
            std::vector<std::complex<float>> continuous_buf;
            continuous_buf.reserve(mtu);

            while (continuous_buf.size() < mtu) {
                size_t needed = mtu - continuous_buf.size();
                size_t chunk = std::min(needed, signal.size());
                continuous_buf.insert(continuous_buf.end(), signal.begin(), signal.begin() + chunk);
            }

            int64_t hw_time = device_->get_hardware_time();
            int sent = device_->write_samples(continuous_buf.data(), continuous_buf.size(), hw_time);
            if (sent > 0) {
                metrics_.tx_packets++;
                metrics_.tx_samples_total += sent;
                metrics_.last_hw_timestamp.store(hw_time);
            }
        } else {
            // Для плавного обновления TX графиков в режиме PCM файла или RX-only
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void SDRController::rx_loop() {
    std::vector<std::complex<float>> rx_chunk(1024);
    std::vector<std::complex<float>> fft_accum;
    fft_accum.reserve(2048);

    while (!should_stop_.load()) {
        SDRConfig cfg = get_config();
        int received = 0;
        int64_t time_ns = 0;

        if (current_source_ == SignalSource::PCMFile) {
            if (pcm_player_.is_loaded()) {
                if (pcm_player_.is_playing()) {
                    received = static_cast<int>(pcm_player_.read_samples(rx_chunk.data(), rx_chunk.size(), true));
                    float speed = pcm_player_.get_playback_speed();
                    if (speed <= 0.01f) speed = 1.0f;
                    int sleep_us = static_cast<int>(1024.0f / speed);
                    if (sleep_us > 0) {
                        std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
                    }
                } else {
                    received = static_cast<int>(pcm_player_.peek_samples(rx_chunk.data(), rx_chunk.size()));
                    std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 кадров/сек при паузе
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }
        } else if (device_) {
            received = device_->read_samples(rx_chunk.data(), rx_chunk.size(), &time_ns);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            continue;
        }

        if (received > 0) {
            metrics_.rx_packets++;
            metrics_.rx_samples_total += received;

            std::vector<std::complex<float>> samples(rx_chunk.begin(), rx_chunk.begin() + received);

            // 1. Нормализация данных при воспроизведении файла
            if (current_source_ == SignalSource::PCMFile) {
                float p99 = pcm_player_.get_percentile_99_5();
                float norm_scale = (p99 > 1e-3f) ? (1.0f / p99) : 1.0f;
                for (auto& s : samples) {
                    s *= norm_scale;
                }
            }

            // 2. Расчет реального уровня сигнала (RMS и Peak в dBFS)
            double pwr_sum = 0.0;
            float max_peak = 0.0f;
            for (const auto& s : samples) {
                float mag = std::abs(s);
                pwr_sum += mag * mag;
                if (mag > max_peak) max_peak = mag;
            }
            float rms = static_cast<float>(std::sqrt(pwr_sum / samples.size()));
            metrics_.rx_rms_level.store(rms);
            metrics_.rx_peak_level.store(max_peak);

            // 3. Компенсация вращения фазы гетеродина CFO: exp(-j * 2*pi * df * t)
            if (cfg.rx_cfo_correction && std::abs(cfg.rx_cfo_hz) > 0.01f) {
                constexpr float SAMPLE_RATE = 1e6f;
                float phase_inc = -2.0f * PI * cfg.rx_cfo_hz / SAMPLE_RATE;
                for (auto& s : samples) {
                    rx_cfo_phase_ += phase_inc;
                    if (rx_cfo_phase_ > 2.0f * PI) rx_cfo_phase_ -= 2.0f * PI;
                    else if (rx_cfo_phase_ < -2.0f * PI) rx_cfo_phase_ += 2.0f * PI;
                    s *= std::complex<float>(std::cos(rx_cfo_phase_), std::sin(rx_cfo_phase_));
                }
            }

            // 4. Запись сырого принимаемого сигнала
            rx_ring_buffer_.write_overwrite(samples.data(), samples.size());

            // 5. Согласованный фильтр (прямоугольная свёртка длины 10 с нормировкой 0.1f из Лаб. 8)
            std::vector<std::complex<float>> filtered;
            if (cfg.rx_matched_filter) {
                filtered = dsp::convolution(samples, 10, 0.1f);
            } else {
                filtered = samples;
            }
            rx_filtered_buffer_.write_overwrite(filtered.data(), filtered.size());

            // 6. Символьная децимация (стробирование каждые 10 отсчетов)
            constexpr size_t step = 10;
            std::vector<std::complex<float>> strobe_symbols;
            strobe_symbols.reserve(filtered.size() / step + 1);
            for (size_t i = step / 2; i < filtered.size(); i += step) {
                float pwr = std::norm(filtered[i]);
                if (pwr > 0.04f) { // отсекаем фоновый шум
                    strobe_symbols.push_back(filtered[i]);
                }
            }

            if (!strobe_symbols.empty()) {
                rx_symbols_buffer_.write_overwrite(strobe_symbols.data(), strobe_symbols.size());
            }

            // 7. Спектр БПФ (1024 точки) и водопад
            fft_accum.insert(fft_accum.end(), samples.begin(), samples.end());
            if (fft_accum.size() >= 1024) {
                std::lock_guard<std::mutex> lock(dsp_mutex_);
                fft_processor_.compute_power_spectrum(
                    fft_accum, latest_freq_axis_, latest_power_spectrum_,
                    1e6, cfg.frequency);

                waterfall_.add_slice(latest_power_spectrum_, -80.0f, 0.0f);
                fft_accum.clear();
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

void SDRController::get_tx_time_domain(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_samples) {
    std::vector<std::complex<float>> tmp(max_samples);
    size_t count = tx_ring_buffer_.peek_latest(tmp.data(), max_samples);
    out_i.resize(count);
    out_q.resize(count);
    for (size_t i = 0; i < count; ++i) {
        out_i[i] = tmp[i].real();
        out_q[i] = tmp[i].imag();
    }
}

void SDRController::get_rx_time_domain(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_samples) {
    std::vector<std::complex<float>> tmp(max_samples);
    size_t count = rx_ring_buffer_.peek_latest(tmp.data(), max_samples);
    out_i.resize(count);
    out_q.resize(count);
    for (size_t i = 0; i < count; ++i) {
        out_i[i] = tmp[i].real();
        out_q[i] = tmp[i].imag();
    }
}

void SDRController::get_rx_filtered_time_domain(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_samples) {
    std::vector<std::complex<float>> tmp(max_samples);
    size_t count = rx_filtered_buffer_.peek_latest(tmp.data(), max_samples);
    out_i.resize(count);
    out_q.resize(count);
    for (size_t i = 0; i < count; ++i) {
        out_i[i] = tmp[i].real();
        out_q[i] = tmp[i].imag();
    }
}

void SDRController::get_tx_constellation(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_points) {
    std::vector<std::complex<float>> tmp(max_points);
    size_t count = tx_symbols_buffer_.peek_latest(tmp.data(), max_points);
    out_i.resize(count);
    out_q.resize(count);
    for (size_t i = 0; i < count; ++i) {
        out_i[i] = tmp[i].real();
        out_q[i] = tmp[i].imag();
    }
}

void SDRController::get_rx_constellation(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_points) {
    std::vector<std::complex<float>> tmp(max_points);
    size_t count = rx_symbols_buffer_.peek_latest(tmp.data(), max_points);
    out_i.resize(count);
    out_q.resize(count);
    for (size_t i = 0; i < count; ++i) {
        out_i[i] = tmp[i].real();
        out_q[i] = tmp[i].imag();
    }
}

size_t SDRController::get_new_rx_symbols(std::vector<float>& out_i, std::vector<float>& out_q, size_t max_points) {
    std::vector<std::complex<float>> tmp(max_points);
    size_t count = rx_symbols_buffer_.read(tmp.data(), max_points);
    out_i.resize(count);
    out_q.resize(count);
    for (size_t i = 0; i < count; ++i) {
        out_i[i] = tmp[i].real();
        out_q[i] = tmp[i].imag();
    }
    return count;
}

void SDRController::get_spectrum(std::vector<float>& out_freq, std::vector<float>& out_power) {
    std::lock_guard<std::mutex> lock(dsp_mutex_);
    out_freq = latest_freq_axis_;
    out_power = latest_power_spectrum_;
}

void SDRController::get_waterfall(const float** out_data, size_t* out_w, size_t* out_h) {
    std::lock_guard<std::mutex> lock(dsp_mutex_);
    if (out_data) *out_data = waterfall_.data();
    if (out_w) *out_w = waterfall_.width();
    if (out_h) *out_h = waterfall_.height();
}

} // namespace sdr
