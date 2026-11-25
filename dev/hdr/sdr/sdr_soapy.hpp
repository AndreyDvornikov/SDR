#pragma once

#include "sdr/sdr_device.hpp"
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <string>
#include <vector>

namespace sdr {

class SoapySDRBackend : public ISDRDevice {
public:
    SoapySDRBackend();
    ~SoapySDRBackend() override;

    bool open(const SDRConfig& config) override;
    void close() override;
    bool is_open() const override { return sdr_ != nullptr; }

    bool start_stream(SDRMode mode) override;
    void stop_stream() override;

    int write_samples(const std::complex<float>* samples, size_t count, int64_t timestamp_ns = 0) override;
    int read_samples(std::complex<float>* out_samples, size_t count, int64_t* out_timestamp_ns = nullptr) override;

    size_t get_mtu() const override;
    int64_t get_hardware_time() override;

    void update_config(const SDRConfig& config) override;
    const char* get_name() const override { return "ADALM-PLUTO (SoapySDR)"; }
    const char* get_last_error() const override { return last_error_.c_str(); }

private:
    SoapySDRDevice* sdr_{nullptr};
    SoapySDRStream* tx_stream_{nullptr};
    SoapySDRStream* rx_stream_{nullptr};
    SDRMode current_mode_{SDRMode::Idle};
    size_t tx_mtu_{1024};
    size_t rx_mtu_{1024};
    float tx_amplitude_{2000.0f};

    std::vector<int16_t> tx_raw_buf_;
    std::vector<int16_t> rx_raw_buf_;

    mutable std::string last_error_;
};

} // namespace sdr
