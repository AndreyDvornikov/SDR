#pragma once

#include "sdr/sdr_types.hpp"
#include <vector>
#include <complex>

namespace sdr {

class ISDRDevice {
public:
    virtual ~ISDRDevice() = default;

    virtual bool open(const SDRConfig& config) = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;

    virtual bool start_stream(SDRMode mode) = 0;
    virtual void stop_stream() = 0;

    virtual int write_samples(const std::complex<float>* samples, size_t count, int64_t timestamp_ns = 0) = 0;
    virtual int read_samples(std::complex<float>* out_samples, size_t count, int64_t* out_timestamp_ns = nullptr) = 0;

    virtual size_t get_mtu() const = 0;
    virtual int64_t get_hardware_time() = 0;

    virtual void update_config(const SDRConfig& config) = 0;
    virtual const char* get_name() const = 0;
    virtual const char* get_last_error() const = 0;
};

} // namespace sdr
