#pragma once

#include <complex>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>

namespace sdr {

class PCMPlayer {
public:
    PCMPlayer();
    ~PCMPlayer() = default;

    // Загрузка файла CS16 (.pcm)
    bool load_file(const std::string& filepath);

    void play();
    void pause();
    void toggle_play();
    void reset();

    // Перемотка
    void seek(size_t sample_index);
    void seek_ratio(float ratio); // 0.0f .. 1.0f

    // Скорость воспроизведения (0.1x .. 10.0x)
    void set_playback_speed(float speed) { playback_speed_.store(std::clamp(speed, 0.05f, 20.0f)); }
    float get_playback_speed() const { return playback_speed_.load(); }

    // Чтение блока отсчетов (продвигает указатель воспроизведения)
    size_t read_samples(std::complex<float>* out_buffer, size_t count, bool loop = true);

    // Подглядывание блока отсчетов на текущей позиции (НЕ сдвигает указатель, для live-анализа на паузе)
    size_t peek_samples(std::complex<float>* out_buffer, size_t count) const;

    // Срез окна вокруг текущего положения (без сдвига)
    void get_window(std::vector<float>& out_i, std::vector<float>& out_q, size_t window_size = 2048);

    // Доступ к состоянию
    bool is_loaded() const { return is_loaded_.load(); }
    bool is_playing() const { return is_playing_.load(); }
    size_t total_samples() const { return total_samples_.load(); }
    size_t current_index() const { return current_index_.load(); }
    float playback_ratio() const;
    const std::string& current_path() const { return current_filepath_; }
    const std::string& current_filename() const { return current_filename_; }

    // Статистика файла (пики, 99.5-й процентиль для нормировки графика)
    float get_peak_amplitude() const { return peak_amplitude_; }
    float get_percentile_99_5() const { return percentile_99_5_; }

private:
    void calculate_stats();

    std::vector<std::complex<float>> samples_;
    std::string current_filepath_;
    std::string current_filename_;

    std::atomic<bool> is_loaded_{false};
    std::atomic<bool> is_playing_{true};
    std::atomic<float> playback_speed_{1.0f};
    std::atomic<size_t> total_samples_{0};
    std::atomic<size_t> current_index_{0};

    float peak_amplitude_{1.0f};
    float percentile_99_5_{1.0f};

    mutable std::mutex mutex_;
};

} // namespace sdr
