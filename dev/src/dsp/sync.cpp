#include "dsp/sync.hpp"
#include <algorithm>
#include <cmath>

namespace sdr::dsp {
using Complex = std::complex<float>;
float gardner_detector(const std::vector<Complex> &samples, size_t k,
                       size_t sps, size_t tau) {
  size_t curr = k * sps + tau;
  size_t prev = (k - 1) * sps + tau;
  size_t mid = curr - (sps / 2);

  if (curr >= samples.size() || k == 0) {
    return 0.0f;
  }

  const Complex &s_curr = samples[curr];
  const Complex &s_prev = samples[prev];
  const Complex &s_mid = samples[mid];

  float err_i = s_mid.real() * (s_prev.real() - s_curr.real());
  float err_q = s_mid.imag() * (s_prev.imag() - s_curr.imag());
  return err_i + err_q;
}

std::vector<float> compute_gardner_s_curve(const std::vector<Complex> &samples,
                                           size_t sps) {
  std::vector<float> s_curve(sps, 0.0f);
  size_t num_symbols = samples.size() / sps;
  if (num_symbols < 3)
    return s_curve;
  for (size_t tau = 0; tau < sps; ++tau) {
    float sum_err = 0.0f;
    size_t cnt = 0;
    for (size_t k = 1; k < num_symbols; ++k) {
      sum_err += gardner_detector(samples, k, sps, tau);
      cnt++;
    }

    s_curve[tau] = (cnt > 0) ? (sum_err / cnt) : 0.0f;
  }
  return s_curve;
}

size_t find_optimal_strobe_phase(const std::vector<float> &s_curve) {
  if (s_curve.empty())
    return 0;

  size_t n = s_curve.size();
  for (size_t p = 0; p < n; ++p) {
    size_t p_next = (p + 1) % n;
    if (s_curve[p] >= 0.0f && s_curve[p_next] <= 0.0f) {
      return (std::abs(s_curve[p]) < std::abs(s_curve[p_next])) ? p : p_next;
    }
  }

  size_t best_tau = 0;
  float min_val = std::abs(s_curve[0]);

  for (size_t tau = 1; tau < n; ++tau) {
    float val = std::abs(s_curve[tau]);
    if (val < min_val) {
      min_val = val;
      best_tau = tau;
    }
  }

  return best_tau;
}

std::vector<Complex> strobe_symbols(const std::vector<Complex> &samples,
                                    size_t sps, size_t phase) {
  std::vector<Complex> symbols;
  if (sps == 0 || phase >= samples.size())
    return symbols;

  symbols.reserve((samples.size() - phase + sps - 1) / sps);
  for (size_t i = phase; i < samples.size(); i += sps) {
    symbols.push_back(samples[i]);
  }

  return symbols;
}
} // namespace sdr::dsp
