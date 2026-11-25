#include "sdr/sdr_soapy.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sdr {

SoapySDRBackend::SoapySDRBackend() = default;

SoapySDRBackend::~SoapySDRBackend() {
    close();
}

bool SoapySDRBackend::open(const SDRConfig& config) {
    close();

    // 1. Попытка создания устройства с драйвером plutosdr и выбранным лупбеком
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    SoapySDRKwargs_set(&args, "loopback", config.loopback ? "1" : "0");
    sdr_ = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    // 2. Fallback с явным указанием URI шины USB
    if (!sdr_) {
        SoapySDRKwargs args_usb = {};
        SoapySDRKwargs_set(&args_usb, "driver", "plutosdr");
        SoapySDRKwargs_set(&args_usb, "uri", "usb:3.5.5");
        SoapySDRKwargs_set(&args_usb, "loopback", config.loopback ? "1" : "0");
        sdr_ = SoapySDRDevice_make(&args_usb);
        SoapySDRKwargs_clear(&args_usb);
    }

    // 3. Fallback с uri=usb:
    if (!sdr_) {
        SoapySDRKwargs args_any = {};
        SoapySDRKwargs_set(&args_any, "driver", "plutosdr");
        SoapySDRKwargs_set(&args_any, "uri", "usb:");
        SoapySDRKwargs_set(&args_any, "loopback", config.loopback ? "1" : "0");
        sdr_ = SoapySDRDevice_make(&args_any);
        SoapySDRKwargs_clear(&args_any);
    }

    if (!sdr_) {
        last_error_ = SoapySDRDevice_lastError();
        return false;
    }

    tx_amplitude_ = config.tx_amplitude;
    update_config(config);
    return true;
}

void SoapySDRBackend::close() {
    stop_stream();
    if (sdr_) {
        SoapySDRDevice_unmake(sdr_);
        sdr_ = nullptr;
    }
}

void SoapySDRBackend::update_config(const SDRConfig& config) {
    if (!sdr_) return;

    tx_amplitude_ = config.tx_amplitude;
    const size_t ch = 0;
    constexpr double SAMPLE_RATE = 1e6; // Фиксировано 1 MSps (Лаб. 8)

    // Настройка TX тракта
    SoapySDRDevice_setSampleRate(sdr_, SOAPY_SDR_TX, ch, SAMPLE_RATE);
    SoapySDRDevice_setFrequency(sdr_, SOAPY_SDR_TX, ch, config.frequency, nullptr);
    SoapySDRDevice_setGain(sdr_, SOAPY_SDR_TX, ch, config.tx_gain);

    // Настройка RX тракта
    SoapySDRDevice_setSampleRate(sdr_, SOAPY_SDR_RX, ch, SAMPLE_RATE);
    SoapySDRDevice_setFrequency(sdr_, SOAPY_SDR_RX, ch, config.frequency, nullptr);
    SoapySDRDevice_setGain(sdr_, SOAPY_SDR_RX, ch, config.rx_gain);
}

bool SoapySDRBackend::start_stream(SDRMode mode) {
    if (!sdr_) return false;
    stop_stream();

    const size_t ch = 0;
    if (mode == SDRMode::TxOnly || mode == SDRMode::FullDuplex) {
        tx_stream_ = SoapySDRDevice_setupStream(sdr_, SOAPY_SDR_TX, SOAPY_SDR_CS16, &ch, 1, nullptr);
        if (!tx_stream_) {
            last_error_ = SoapySDRDevice_lastError();
            return false;
        }
        tx_mtu_ = SoapySDRDevice_getStreamMTU(sdr_, tx_stream_);
        tx_raw_buf_.resize(2 * tx_mtu_);
        if (SoapySDRDevice_activateStream(sdr_, tx_stream_, 0, 0, 0) != 0) {
            last_error_ = SoapySDRDevice_lastError();
            stop_stream();
            return false;
        }
    }

    if (mode == SDRMode::RxOnly || mode == SDRMode::FullDuplex) {
        rx_stream_ = SoapySDRDevice_setupStream(sdr_, SOAPY_SDR_RX, SOAPY_SDR_CS16, &ch, 1, nullptr);
        if (!rx_stream_) {
            last_error_ = SoapySDRDevice_lastError();
            stop_stream();
            return false;
        }
        rx_mtu_ = SoapySDRDevice_getStreamMTU(sdr_, rx_stream_);
        rx_raw_buf_.resize(2 * rx_mtu_);
        if (SoapySDRDevice_activateStream(sdr_, rx_stream_, 0, 0, 0) != 0) {
            last_error_ = SoapySDRDevice_lastError();
            stop_stream();
            return false;
        }
    }

    current_mode_ = mode;
    return true;
}

void SoapySDRBackend::stop_stream() {
    if (!sdr_) return;

    if (tx_stream_) {
        SoapySDRDevice_deactivateStream(sdr_, tx_stream_, 0, 0);
        SoapySDRDevice_closeStream(sdr_, tx_stream_);
        tx_stream_ = nullptr;
    }
    if (rx_stream_) {
        SoapySDRDevice_deactivateStream(sdr_, rx_stream_, 0, 0);
        SoapySDRDevice_closeStream(sdr_, rx_stream_);
        rx_stream_ = nullptr;
    }
    current_mode_ = SDRMode::Idle;
}

int SoapySDRBackend::write_samples(const std::complex<float>* samples, size_t count, int64_t timestamp_ns) {
    if (!sdr_ || !tx_stream_) return -1;

    size_t to_send = std::min(count, tx_mtu_);
    if (tx_raw_buf_.size() < 2 * to_send) {
        tx_raw_buf_.resize(2 * to_send);
    }

    for (size_t i = 0; i < to_send; ++i) {
        float r = samples[i].real() * tx_amplitude_;
        float im = samples[i].imag() * tx_amplitude_;
        tx_raw_buf_[2 * i] = static_cast<int16_t>(std::clamp(r, -32768.0f, 32767.0f));
        tx_raw_buf_[2 * i + 1] = static_cast<int16_t>(std::clamp(im, -32768.0f, 32767.0f));
    }

    const void* buffs[] = {tx_raw_buf_.data()};
    int flags = (timestamp_ns != 0) ? SOAPY_SDR_HAS_TIME : 0;
    constexpr long timeout_us = 1000000;

    int res = SoapySDRDevice_writeStream(sdr_, tx_stream_, buffs, to_send, &flags, timestamp_ns, timeout_us);
    if (res < 0) {
        last_error_ = SoapySDRDevice_lastError();
    }
    return res;
}

int SoapySDRBackend::read_samples(std::complex<float>* out_samples, size_t count, int64_t* out_timestamp_ns) {
    if (!sdr_ || !rx_stream_) return -1;

    size_t to_read = std::min(count, rx_mtu_);
    if (rx_raw_buf_.size() < 2 * to_read) {
        rx_raw_buf_.resize(2 * to_read);
    }

    void* buffs[] = {rx_raw_buf_.data()};
    int flags = 0;
    long long time_ns = 0;
    constexpr long timeout_us = 1000000;

    int received = SoapySDRDevice_readStream(sdr_, rx_stream_, buffs, to_read, &flags, &time_ns, timeout_us);
    if (received > 0) {
        if (out_timestamp_ns) *out_timestamp_ns = time_ns;
        constexpr float scale = 1.0f / 32768.0f;
        for (int i = 0; i < received; ++i) {
            out_samples[i] = std::complex<float>(
                static_cast<float>(rx_raw_buf_[2 * i]) * scale,
                static_cast<float>(rx_raw_buf_[2 * i + 1]) * scale
            );
        }
    } else if (received < 0) {
        last_error_ = SoapySDRDevice_lastError();
    }
    return received;
}

size_t SoapySDRBackend::get_mtu() const {
    return (current_mode_ == SDRMode::RxOnly) ? rx_mtu_ : tx_mtu_;
}

int64_t SoapySDRBackend::get_hardware_time() {
    if (!sdr_) return 0;
    return SoapySDRDevice_getHardwareTime(sdr_, nullptr);
}

} // namespace sdr
