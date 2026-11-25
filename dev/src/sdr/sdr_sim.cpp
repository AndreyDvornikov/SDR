#include "sdr/sdr_sim.hpp"
#include <chrono>
#include <cmath>
#include <algorithm>

namespace sdr {

constexpr float PI = 3.14159265358979323846f;

SimulationSDRBackend::SimulationSDRBackend()
    : rng_(std::random_device{}())
{
    simulated_time_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool SimulationSDRBackend::open(const SDRConfig& config) {
    config_ = config;
    is_open_ = true;
    loopback_fifo_.clear();
    return true;
}

void SimulationSDRBackend::close() {
    is_open_ = false;
    current_mode_ = SDRMode::Idle;
    loopback_fifo_.clear();
}

void SimulationSDRBackend::update_config(const SDRConfig& config) {
    config_ = config;
}

bool SimulationSDRBackend::start_stream(SDRMode mode) {
    if (!is_open_) return false;
    current_mode_ = mode;
    return true;
}

void SimulationSDRBackend::stop_stream() {
    current_mode_ = SDRMode::Idle;
}

int SimulationSDRBackend::write_samples(const std::complex<float>* samples, size_t count, int64_t timestamp_ns) {
    if (!is_open_ || count == 0) return 0;

    // Simulation channel model:
    // 1. Channel Attenuation according to gain
    // 2. Carrier Frequency Offset (CFO) e.g. rotating constellation
    // 3. AWGN Noise according to sim_snr_db
    // 4. Phase jitter

    const float snr_linear = std::pow(10.0f, config_.sim_snr_db / 10.0f);
    const float noise_std = (snr_linear > 0.001f) ? (1.0f / std::sqrt(2.0f * snr_linear)) : 0.5f;
    const float phase_step = (2.0f * PI * config_.sim_cfo_hz) / static_cast<float>(config_.sample_rate);

    for (size_t i = 0; i < count; ++i) {
        std::complex<float> s = samples[i];

        // Apply CFO frequency rotation
        current_phase_ += phase_step;
        if (current_phase_ > 2.0f * PI) current_phase_ -= 2.0f * PI;
        else if (current_phase_ < -2.0f * PI) current_phase_ += 2.0f * PI;

        std::complex<float> rot(std::cos(current_phase_), std::sin(current_phase_));
        std::complex<float> tx_rot = s * rot;

        // Apply AWGN Noise
        float noise_i = noise_dist_(rng_) * noise_std;
        float noise_q = noise_dist_(rng_) * noise_std;
        std::complex<float> noisy = tx_rot + std::complex<float>(noise_i, noise_q);

        loopback_fifo_.push_back(noisy);
    }

    // Keep FIFO reasonable size (e.g. max 65536 samples)
    if (loopback_fifo_.size() > 65536) {
        loopback_fifo_.erase(loopback_fifo_.begin(), loopback_fifo_.begin() + (loopback_fifo_.size() - 65536));
    }

    return static_cast<int>(count);
}

int SimulationSDRBackend::read_samples(std::complex<float>* out_samples, size_t count, int64_t* out_timestamp_ns) {
    if (!is_open_ || count == 0) return 0;

    if (out_timestamp_ns) {
        *out_timestamp_ns = get_hardware_time();
    }

    size_t written = 0;
    if (!loopback_fifo_.empty()) {
        size_t available = std::min(count, loopback_fifo_.size());
        std::copy(loopback_fifo_.begin(), loopback_fifo_.begin() + available, out_samples);
        loopback_fifo_.erase(loopback_fifo_.begin(), loopback_fifo_.begin() + available);
        written = available;
    }

    // If FIFO doesn't have enough samples, fill remainder with atmospheric channel noise
    if (written < count) {
        const float noise_std = 0.05f;
        for (size_t i = written; i < count; ++i) {
            out_samples[i] = std::complex<float>(noise_dist_(rng_) * noise_std, noise_dist_(rng_) * noise_std);
        }
        written = count;
    }

    return static_cast<int>(written);
}

int64_t SimulationSDRBackend::get_hardware_time() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

} // namespace sdr
