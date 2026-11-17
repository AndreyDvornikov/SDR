#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#include <algorithm>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr double SAMPLE_RATE = 1e6;
constexpr double FREQUENCIES_HZ[] = {800e6, 850e6};
constexpr size_t FREQUENCY_COUNT =
    sizeof(FREQUENCIES_HZ) / sizeof(FREQUENCIES_HZ[0]);
constexpr size_t TX_ITERATIONS = 1000000;
constexpr size_t RX_ITERATIONS = 2000;
constexpr long STREAM_TIMEOUT_US = 1000000;

enum class Mode { Tx, Rx };

void print_usage(const char *program) {
  std::printf("Использование: %s <Tx|Rx> --freq <1|2>\n"
              "  Tx / --tx     передача тестового I/Q-сигнала\n"
              "  Rx / --rx     приём I/Q-сигнала в rxdata.pcm\n"
              "  --freq 1      %.0f МГц\n"
              "  --freq 2      %.0f МГц\n",
              program, FREQUENCIES_HZ[0] / 1e6, FREQUENCIES_HZ[1] / 1e6);
}

bool parse_arguments(int argc, char *argv[], Mode *mode, double *frequency) {
  if (argc != 4 || std::strcmp(argv[2], "--freq") != 0)
    return false;

  if (std::strcmp(argv[1], "Tx") == 0 || std::strcmp(argv[1], "--tx") == 0)
    *mode = Mode::Tx;
  else if (std::strcmp(argv[1], "Rx") == 0 || std::strcmp(argv[1], "--rx") == 0)
    *mode = Mode::Rx;
  else
    return false;

  char *end = nullptr;
  const long selected = std::strtol(argv[3], &end, 10);
  if (*argv[3] == '\0' || *end != '\0' || selected < 1 ||
      selected > static_cast<long>(FREQUENCY_COUNT))
    return false;

  *frequency = FREQUENCIES_HZ[selected - 1];
  return true;
}

SoapySDRDevice *make_device() {
  SoapySDRKwargs args = {};
  SoapySDRKwargs_set(&args, "driver", "plutosdr");
  SoapySDRKwargs_set(&args, "uri", "usb:");
  SoapySDRKwargs_set(&args, "loopback", "0");
  SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
  SoapySDRKwargs_clear(&args);
  return sdr;
}

[[maybe_unused]] std::vector<bool> generate_bits(int size) {
  std::vector<bool> bits(size);

  std::mt19937 rng(std::random_device{}());
  std::bernoulli_distribution distribution(0.5);

  for (int i = 0; i < size; ++i) {
    bits[i] = distribution(rng);
  }

  return bits;
}

std::vector<bool> string_to_bits(const std::string &str) {
  std::vector<bool> bits;
  bits.reserve(str.size() * 8);

  for (unsigned char c : str) {
    for (int i = 7; i >= 0; --i) {
      bits.push_back((c >> i) & 1);
    }
  }

  return bits;
}

[[maybe_unused]] std::vector<float> bpsk(const std::vector<bool> &bits) {
  std::vector<float> sym;
  sym.reserve(bits.size());

  for (bool bit : bits) {
    sym.push_back(bit ? 1.0f : -1.0f);
  }

  return sym;
}

std::vector<std::complex<float>> qpsk(const std::vector<bool> &bits) {
  std::vector<std::complex<float>> sym;
  sym.reserve((bits.size() + 1) / 2);

  constexpr float scale = 0.70710678f; // 1/sqrt(2)

  for (std::size_t i = 0; i + 1 < bits.size(); i += 2) {
    const float I = bits[i] ? scale : -scale;
    const float Q = bits[i + 1] ? scale : -scale;

    sym.emplace_back(I, Q);
  }

  return sym;
}

template <typename T>
std::vector<T> upsample(const std::vector<T> &symbols, size_t factor) {
  std::vector<T> samples;
  samples.reserve(symbols.size() * factor);

  for (const auto &symbol : symbols) {
    samples.push_back(symbol);

    for (size_t i = 1; i < factor; ++i) {
      samples.push_back(T{});
    }
  }

  return samples;
}

template <typename T>
std::vector<T> convolution(const std::vector<T> &samples, size_t h_size = 10,
                           float scale = 1.0f) {
  std::vector<float> h(h_size, 1.0f * scale);
  std::vector<T> signal;

  if (samples.empty()) {
    return signal;
  }

  signal.reserve(samples.size() + h.size() - 1);

  for (std::size_t n = 0; n < samples.size() + h.size() - 1; ++n) {
    T sum{};

    for (std::size_t k = 0; k < samples.size(); ++k) {
      if (n >= k && n - k < h.size()) {
        sum += samples[k] * h[n - k];
      }
    }

    signal.push_back(sum);
  }

  return signal;
}

int run_tx(SoapySDRDevice *sdr, double frequency) {
  const size_t channel = 0;
  if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, channel, SAMPLE_RATE) !=
          0 ||
      SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, channel, frequency,
                                  nullptr) != 0 ||
      SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channel, -20.0) != 0) {
    std::printf("Не удалось настроить TX: %s\n", SoapySDRDevice_lastError());
    return EXIT_FAILURE;
  }

  SoapySDRStream *stream = SoapySDRDevice_setupStream(
      sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, &channel, 1, nullptr);
  if (!stream) {
    std::printf("setupStream TX: %s\n", SoapySDRDevice_lastError());
    return EXIT_FAILURE;
  }

  const size_t mtu = SoapySDRDevice_getStreamMTU(sdr, stream);

  auto bits = string_to_bits("SIBGUTI_IKS431");
  auto sym = qpsk(bits);
  constexpr size_t UPSAMPLE_FACTOR = 10;
  auto samples = upsample(sym, UPSAMPLE_FACTOR);
  auto signal = convolution(samples, UPSAMPLE_FACTOR, 1.0f);

  constexpr float TX_AMPLITUDE = 2000.0f;
  constexpr size_t TS_SAMPLES = 6;
  const size_t sig_samples = signal.size();
  const size_t block_samples = TS_SAMPLES + sig_samples;

  std::vector<int16_t> active_buffer(2 * mtu, 0);
  std::vector<int16_t> zero_buffer(2 * mtu, 0);

  FILE *file = std::fopen("txdata.pcm", "wb");
  if (!file) {
    std::printf("Не удалось открыть txdata.pcm\n");
    SoapySDRDevice_closeStream(sdr, stream);
    return EXIT_FAILURE;
  }

  int result = EXIT_SUCCESS;
  if (SoapySDRDevice_activateStream(sdr, stream, 0, 0, 0) != 0) {
    std::printf("activateStream TX: %s\n", SoapySDRDevice_lastError());
    result = EXIT_FAILURE;
  } else {
    const long long base_time = SoapySDRDevice_getHardwareTime(sdr, nullptr);
    constexpr long long STEP_NS = 4LL * 1000 * 1000;
    constexpr long long SILENCE_OFFSET_NS = 1920LL * 1000;
    std::printf("TX: %.0f МГц, MTU: %zu\n", frequency / 1e6, mtu);

    for (size_t iteration = 0; iteration < TX_ITERATIONS; ++iteration) {
      const long long tx_time =
          base_time + static_cast<long long>(iteration + 1) * STEP_NS;

      std::fill(active_buffer.begin(), active_buffer.end(), 0);
      for (size_t b = 0; b < 3; ++b) {
        size_t sample_offset = b * block_samples;
        if (sample_offset + block_samples > mtu) {
          break;
        }

        size_t i16_offset = 2 * sample_offset;

        // Преамбула 0xFFFF
        active_buffer[i16_offset + 0] = static_cast<int16_t>(0xffff);
        active_buffer[i16_offset + 1] = static_cast<int16_t>(0xffff);

        // 8 байт времени tx_time
        for (size_t i = 0; i < 8; ++i) {
          const uint8_t byte =
              static_cast<uint8_t>((tx_time >> (8 * i)) & 0xff);
          active_buffer[i16_offset + 2 + i] = static_cast<int16_t>(byte << 4);
        }

        // Маркер конца таймстемпа 0xFFFF
        active_buffer[i16_offset + 10] = static_cast<int16_t>(0xffff);
        active_buffer[i16_offset + 11] = static_cast<int16_t>(0xffff);

        // Отсчёты сигнала
        size_t sig_start = sample_offset + TS_SAMPLES;
        for (size_t s = 0; s < sig_samples; ++s) {
          active_buffer[2 * (sig_start + s)] =
              static_cast<int16_t>(signal[s].real() * TX_AMPLITUDE);
          active_buffer[2 * (sig_start + s) + 1] =
              static_cast<int16_t>(signal[s].imag() * TX_AMPLITUDE);
        }
      }

      // Передаем активный буфер
      std::fwrite(active_buffer.data(), sizeof(int16_t), active_buffer.size(),
                  file);
      const void *active_buffers[] = {active_buffer.data()};
      int flags = SOAPY_SDR_HAS_TIME;
      int written = SoapySDRDevice_writeStream(
          sdr, stream, active_buffers, mtu, &flags, tx_time, STREAM_TIMEOUT_US);
      if (written < 0) {
        std::printf("TX ошибка (активный): %d (%s)\n", written,
                    SoapySDRDevice_lastError());
        result = EXIT_FAILURE;
        break;
      }

      // 2. Передаем буфер тишины (нули)
      const long long zero_time = tx_time + SILENCE_OFFSET_NS;
      std::fwrite(zero_buffer.data(), sizeof(int16_t), zero_buffer.size(),
                  file);
      const void *zero_buffers[] = {zero_buffer.data()};
      flags = SOAPY_SDR_HAS_TIME;
      written = SoapySDRDevice_writeStream(
          sdr, stream, zero_buffers, mtu, &flags, zero_time, STREAM_TIMEOUT_US);
      if (written < 0) {
        std::printf("TX ошибка (тишина): %d (%s)\n", written,
                    SoapySDRDevice_lastError());
        result = EXIT_FAILURE;
        break;
      }
    }
    SoapySDRDevice_deactivateStream(sdr, stream, 0, 0);
  }
  std::fclose(file);
  SoapySDRDevice_closeStream(sdr, stream);
  return result;
}

int run_rx(SoapySDRDevice *sdr, double frequency) {
  const size_t channel = 0;
  if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, channel, SAMPLE_RATE) !=
          0 ||
      SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, channel, frequency,
                                  nullptr) != 0 ||
      SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channel, 40.0) != 0) {
    std::printf("Не удалось настроить RX: %s\n", SoapySDRDevice_lastError());
    return EXIT_FAILURE;
  }

  SoapySDRStream *stream = SoapySDRDevice_setupStream(
      sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, &channel, 1, nullptr);
  if (!stream) {
    std::printf("setupStream RX: %s\n", SoapySDRDevice_lastError());
    return EXIT_FAILURE;
  }

  const size_t mtu = SoapySDRDevice_getStreamMTU(sdr, stream);
  std::vector<int16_t> buffer(2 * mtu);
  FILE *file = std::fopen("rxdata.pcm", "wb");
  if (!file) {
    std::printf("Не удалось открыть rxdata.pcm\n");
    SoapySDRDevice_closeStream(sdr, stream);
    return EXIT_FAILURE;
  }

  int result = EXIT_SUCCESS;
  if (SoapySDRDevice_activateStream(sdr, stream, 0, 0, 0) != 0) {
    std::printf("activateStream RX: %s\n", SoapySDRDevice_lastError());
    result = EXIT_FAILURE;
  } else {
    std::printf("RX: %.0f МГц, MTU: %zu\n", frequency / 1e6, mtu);
    for (size_t iteration = 0; iteration < RX_ITERATIONS; ++iteration) {
      void *buffers[] = {buffer.data()};
      int flags = 0;
      long long time_ns = 0;
      const int received = SoapySDRDevice_readStream(
          sdr, stream, buffers, mtu, &flags, &time_ns, STREAM_TIMEOUT_US);
      if (received > 0) {
        std::vector<std::complex<float>> rx_samples;
        rx_samples.reserve(received);
        for (int i = 0; i < received; ++i) {
          rx_samples.emplace_back(static_cast<float>(buffer[2 * i]),
                                  static_cast<float>(buffer[2 * i + 1]));
        }

        // Согласованный фильтр с нормировкой на длину (10)
        auto filtered = convolution(rx_samples, 10, 0.1f);

        std::vector<int16_t> conv_buffer(2 * filtered.size());
        for (size_t i = 0; i < filtered.size(); ++i) {
          const float r = filtered[i].real();
          const float im = filtered[i].imag();
          conv_buffer[2 * i] =
              static_cast<int16_t>(std::clamp(r, -32768.0f, 32767.0f));
          conv_buffer[2 * i + 1] =
              static_cast<int16_t>(std::clamp(im, -32768.0f, 32767.0f));
        }
        std::fwrite(conv_buffer.data(), sizeof(int16_t), conv_buffer.size(),
                    file);
      } else if (received < 0) {
        std::printf("RX ошибка: %d (%s)\n", received,
                    SoapySDRDevice_lastError());
      }
    }
    SoapySDRDevice_deactivateStream(sdr, stream, 0, 0);
  }
  std::fclose(file);
  SoapySDRDevice_closeStream(sdr, stream);
  return result;
}

} // namespace

int main(int argc, char *argv[]) {
  Mode mode;
  double frequency = 0;
  if (!parse_arguments(argc, argv, &mode, &frequency)) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  SoapySDRDevice *sdr = make_device();
  if (!sdr) {
    std::printf("SoapySDRDevice_make: %s\n", SoapySDRDevice_lastError());
    return EXIT_FAILURE;
  }

  const int result =
      mode == Mode::Tx ? run_tx(sdr, frequency) : run_rx(sdr, frequency);
  SoapySDRDevice_unmake(sdr);
  return result;
}
