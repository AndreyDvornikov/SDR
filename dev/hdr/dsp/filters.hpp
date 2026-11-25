#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace sdr::dsp {

// Вставка factor - 1 нулей между отсчетами (Upsampling из Лаб. 8)
template <typename T>
std::vector<T> upsample(const std::vector<T>& symbols, size_t factor = 10) {
    if (factor <= 1 || symbols.empty()) {
        return symbols;
    }
    std::vector<T> samples;
    samples.reserve(symbols.size() * factor);

    for (const auto& symbol : symbols) {
        samples.push_back(symbol);
        for (size_t i = 1; i < factor; ++i) {
            samples.push_back(T{});
        }
    }
    return samples;
}

// Прямоугольная дискретная свёртка из Лаб. 8 (main.cpp):
// h_size = 10 (длина окна), scale = 1.0f (TX) или 0.1f (RX согласованный фильтр)
template <typename T>
std::vector<T> convolution(const std::vector<T>& samples, size_t h_size = 10, float scale = 1.0f) {
    std::vector<float> h(h_size, 1.0f * scale);
    std::vector<T> signal;

    if (samples.empty()) {
        return signal;
    }

    const size_t n_samples = samples.size();
    signal.reserve(n_samples + h.size() - 1);

    for (std::size_t n = 0; n < n_samples + h.size() - 1; ++n) {
        T sum{};
        size_t k_start = (n >= h.size()) ? (n - h.size() + 1) : 0;
        size_t k_end = (n + 1 < n_samples) ? (n + 1) : n_samples;

        for (std::size_t k = k_start; k < k_end; ++k) {
            sum += samples[k] * h[n - k];
        }
        signal.push_back(sum);
    }

    return signal;
}

} // namespace sdr::dsp
