#include "dsp/fft.hpp"

#include <cmath>
#include <algorithm>
#include <cstring>

namespace sdr::dsp {

constexpr float PI = 3.14159265358979323846f;

FFTProcessor::FFTProcessor(size_t fft_size)
    : fft_size_(fft_size), window_type_(WindowType::Hann)
{
    update_window();
}

void FFTProcessor::set_size(size_t fft_size) {
    // Round up to power of 2
    size_t p = 1;
    while (p < fft_size) p <<= 1;
    fft_size_ = p;
    update_window();
}

void FFTProcessor::set_window(WindowType type) {
    window_type_ = type;
    update_window();
}

void FFTProcessor::update_window() {
    window_.resize(fft_size_);
    for (size_t i = 0; i < fft_size_; ++i) {
        float factor = static_cast<float>(i) / static_cast<float>(fft_size_ - 1);
        switch (window_type_) {
            case WindowType::Hann:
                window_[i] = 0.5f * (1.0f - std::cos(2.0f * PI * factor));
                break;
            case WindowType::Hamming:
                window_[i] = 0.54f - 0.46f * std::cos(2.0f * PI * factor);
                break;
            case WindowType::Blackman:
                window_[i] = 0.42f - 0.5f * std::cos(2.0f * PI * factor) +
                             0.08f * std::cos(4.0f * PI * factor);
                break;
            case WindowType::Rectangular:
            default:
                window_[i] = 1.0f;
                break;
        }
    }
}

void FFTProcessor::cooley_tukey_fft(std::vector<std::complex<float>>& a, bool invert) {
    const size_t n = a.size();
    if (n <= 1) return;

    // Bit reversal permutation
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }

    // Cooley-Tukey Radix-2 butterfly
    for (size_t len = 2; len <= n; len <<= 1) {
        float ang = 2.0f * PI / static_cast<float>(len) * (invert ? -1.0f : 1.0f);
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                std::complex<float> u = a[i + j];
                std::complex<float> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (invert) {
        float inv_n = 1.0f / static_cast<float>(n);
        for (auto& x : a) {
            x *= inv_n;
        }
    }
}

std::vector<std::complex<float>> FFTProcessor::compute_fft(
    const std::vector<std::complex<float>>& time_domain)
{
    std::vector<std::complex<float>> input(fft_size_, {0.0f, 0.0f});
    const size_t copy_len = std::min(time_domain.size(), fft_size_);

    for (size_t i = 0; i < copy_len; ++i) {
        input[i] = time_domain[i] * window_[i];
    }

    cooley_tukey_fft(input, false);

    // FFT Shift: перенос отрицательных частот влево, положительных вправо
    std::vector<std::complex<float>> shifted(fft_size_);
    const size_t half = fft_size_ / 2;
    for (size_t i = 0; i < half; ++i) {
        shifted[i] = input[i + half];
        shifted[i + half] = input[i];
    }
    return shifted;
}

void FFTProcessor::compute_power_spectrum(
    const std::vector<std::complex<float>>& time_domain,
    std::vector<float>& out_freq_axis,
    std::vector<float>& out_power_db,
    double sample_rate,
    double center_freq)
{
    auto fft_res = compute_fft(time_domain);

    out_freq_axis.resize(fft_size_);
    out_power_db.resize(fft_size_);

    const double df = sample_rate / static_cast<double>(fft_size_);
    const double f_start = center_freq - sample_rate / 2.0;

    for (size_t i = 0; i < fft_size_; ++i) {
        out_freq_axis[i] = static_cast<float>(f_start + i * df);

        float p = std::norm(fft_res[i]) / static_cast<float>(fft_size_ * fft_size_);
        if (p < 1e-12f) p = 1e-12f;
        out_power_db[i] = 10.0f * std::log10(p);
    }
}

// Waterfall Buffer Implementation
WaterfallBuffer::WaterfallBuffer(size_t width, size_t height)
    : width_(width), height_(height), buffer_(width * height, -100.0f)
{
}

void WaterfallBuffer::resize(size_t width, size_t height) {
    width_ = width;
    height_ = height;
    buffer_.assign(width_ * height_, -100.0f);
}

void WaterfallBuffer::add_slice(const std::vector<float>& power_db, float min_db, float max_db) {
    if (power_db.empty() || width_ == 0 || height_ == 0) return;

    // Shift previous rows downwards (row 0 is latest / top)
    if (height_ > 1) {
        std::memmove(&buffer_[width_], &buffer_[0], (height_ - 1) * width_ * sizeof(float));
    }

    // Resample or copy power_db into row 0
    const float range = (max_db > min_db) ? (max_db - min_db) : 1.0f;
    for (size_t x = 0; x < width_; ++x) {
        size_t src_idx = (x * power_db.size()) / width_;
        if (src_idx >= power_db.size()) src_idx = power_db.size() - 1;

        float val = power_db[src_idx];
        // Normalize between 0.0 and 1.0 or keep raw dB for ImPlot colormap
        float norm_val = std::clamp((val - min_db) / range, 0.0f, 1.0f);
        buffer_[x] = norm_val;
    }
}

void WaterfallBuffer::clear() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
}

} // namespace sdr::dsp
