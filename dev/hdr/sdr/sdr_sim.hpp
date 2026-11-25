#pragma once

#include "sdr/sdr_device.hpp"
#include <random>
#include <vector>

namespace sdr {

class SimulationSDRBackend : public ISDRDevice {
public:
    SimulationSDRBackend();
    ~SimulationSDRBackend() override = default;

    bool open(const SDRConfig& config) override;
    void close() override;
    bool is_open() const override { return is_open_; }

    bool start_stream(SDRMode mode) override;
    void stop_stream() override;

    int write_samples(const std::complex<float>* samples, size_t count, int64_t timestamp_ns = 0) override;
    int read_samples(std::complex<float>* out_samples, size_t count, int64_t* out_timestamp_ns = nullptr) override;

    size_t get_mtu() const override { return mtu_; }
    int64_t get_hardware_time() override;

    void update_config(const SDRConfig& config) override;
    const char* get_name() const override { return "SDR Channel Simulator (Software Loopback)"; }
    const char* get_last_error() const override { return ""; }

private:
    bool is_open_{false};
    SDRMode current_mode_{SDRMode::Idle};
    size_t mtu_{1024};
    SDRConfig config_;

    std::vector<std::complex<float>> loopback_fifo_;
    std::mt19937 rng_;
    std::normal_distribution<float> noise_dist_{0.0f, 1.0f};

    float current_phase_{0.0f};
    int64_t simulated_time_ns_{0};
};

} // namespace sdr
