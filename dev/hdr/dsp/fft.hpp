#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace sdr::dsp {

enum class WindowType {
    Rectangular,
    Hann,
    Hamming,
    Blackman
};

class FFTProcessor {
public:
    explicit FFTProcessor(size_t fft_size = 1024);
    ~FFTProcessor() = default;

    void set_size(size_t fft_size);
    size_t get_size() const { return fft_size_; }

    void set_window(WindowType type);

    // Вычисление БПФ комплексных отсчетов
    // Возвращает комплексный спектр с центром на нулевой частоте (fftshift)
    std::vector<std::complex<float>> compute_fft(const std::vector<std::complex<float>>& time_domain);

    // Вычисление спектра мощности в dBFS (со сдвигом нуля в центр)
    void compute_power_spectrum(
        const std::vector<std::complex<float>>& time_domain,
        std::vector<float>& out_freq_axis,
        std::vector<float>& out_power_db,
        double sample_rate = 1e6,
        double center_freq = 800e6);

private:
    void update_window();
    void cooley_tukey_fft(std::vector<std::complex<float>>& a, bool invert);

    size_t fft_size_;
    WindowType window_type_;
    std::vector<float> window_;
};

// Кольцевой буфер для водопада (Waterfall)
class WaterfallBuffer {
public:
    WaterfallBuffer(size_t width = 256, size_t height = 128);

    void resize(size_t width, size_t height);
    void add_slice(const std::vector<float>& power_db, float min_db = -80.0f, float max_db = 0.0f);

    size_t width() const { return width_; }
    size_t height() const { return height_; }
    const float* data() const { return buffer_.data(); }

    void clear();

private:
    size_t width_;
    size_t height_;
    std::vector<float> buffer_; // row-major flat 2D grid [height * width]
};

} // namespace sdr::dsp
