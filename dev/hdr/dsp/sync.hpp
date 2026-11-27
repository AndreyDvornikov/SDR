#pragma once
#include <complex>
#include <vector>

namespace sdr::dsp {
using Complex = std::complex<float>;

float gardner_detector(const std::vector<Complex> &samples, size_t k,
                       size_t sps, size_t tau);

std::vector<float> compute_gardner_s_curve(const std::vector<Complex> &samples,
                                           size_t sps = 10);

size_t find_optimal_strobe_phase(const std::vector<float> &s_curve);

std::vector<Complex> strobe_symbols(const std::vector<Complex> &samples,
                                    size_t sps, size_t phase);
} // namespace sdr::dsp
