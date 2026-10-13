#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr double SAMPLE_RATE = 1e6;
constexpr double FREQUENCIES_HZ[] = {800e6, 900e6};
constexpr size_t FREQUENCY_COUNT = sizeof(FREQUENCIES_HZ) / sizeof(FREQUENCIES_HZ[0]);
constexpr size_t TX_ITERATIONS = 1000000;
constexpr size_t RX_ITERATIONS = 2000;
constexpr long STREAM_TIMEOUT_US = 1000000;

enum class Mode { Tx, Rx };

void print_usage(const char *program)
{
    std::printf(
        "Использование: %s <Tx|Rx> --freq <1|2>\n"
        "  Tx / --tx     передача тестового I/Q-сигнала\n"
        "  Rx / --rx     приём I/Q-сигнала в rxdata.pcm\n"
        "  --freq 1      %.0f МГц\n"
        "  --freq 2      %.0f МГц\n",
        program, FREQUENCIES_HZ[0] / 1e6, FREQUENCIES_HZ[1] / 1e6);
}

bool parse_arguments(int argc, char *argv[], Mode *mode, double *frequency)
{
    if (argc != 4 || std::strcmp(argv[2], "--freq") != 0) return false;

    if (std::strcmp(argv[1], "Tx") == 0 || std::strcmp(argv[1], "--tx") == 0)
        *mode = Mode::Tx;
    else if (std::strcmp(argv[1], "Rx") == 0 || std::strcmp(argv[1], "--rx") == 0)
        *mode = Mode::Rx;
    else
        return false;

    char *end = nullptr;
    const long selected = std::strtol(argv[3], &end, 10);
    if (*argv[3] == '\0' || *end != '\0' || selected < 1 ||
        selected > static_cast<long>(FREQUENCY_COUNT)) return false;

    *frequency = FREQUENCIES_HZ[selected - 1];
    return true;
}

SoapySDRDevice *make_device()
{
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    SoapySDRKwargs_set(&args, "uri", "usb:");
    SoapySDRKwargs_set(&args, "loopback", "0");
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);
    return sdr;
}

int run_tx(SoapySDRDevice *sdr, double frequency)
{
    const size_t channel = 0;
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, channel, SAMPLE_RATE) != 0 ||
        SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, channel, frequency, nullptr) != 0 ||
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
    std::vector<int16_t> buffer(2 * mtu);
    for (size_t i = 0; i < 2 * mtu; i += 2) {
        const int16_t value = i < mtu ? static_cast<int16_t>(i)
                                      : static_cast<int16_t>(2 * mtu - i);
        buffer[i] = value;
        buffer[i + 1] = value;
    }

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
        std::printf("TX: %.0f МГц, MTU: %zu\n", frequency / 1e6, mtu);
        for (size_t iteration = 0; iteration < TX_ITERATIONS; ++iteration) {
            const long long tx_time = base_time + static_cast<long long>(iteration + 1) * STEP_NS;
            buffer[0] = buffer[1] = static_cast<int16_t>(0xffff);
            buffer[10] = buffer[11] = static_cast<int16_t>(0xffff);
            for (size_t i = 0; i < 8; ++i) {
                const uint8_t byte = static_cast<uint8_t>((tx_time >> (8 * i)) & 0xff);
                buffer[2 + i] = static_cast<int16_t>(byte << 4);
            }
            std::fwrite(buffer.data(), sizeof(int16_t), buffer.size(), file);
            const void *buffers[] = {buffer.data()};
            int flags = SOAPY_SDR_HAS_TIME;
            const int written = SoapySDRDevice_writeStream(
                sdr, stream, buffers, mtu,
                &flags, tx_time, STREAM_TIMEOUT_US);
            if (written < 0) {
                std::printf("TX ошибка: %d (%s)\n", written, SoapySDRDevice_lastError());
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

int run_rx(SoapySDRDevice *sdr, double frequency)
{
    const size_t channel = 0;
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, channel, SAMPLE_RATE) != 0 ||
        SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, channel, frequency, nullptr) != 0 ||
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
            if (received > 0)
                std::fwrite(buffer.data(), 2 * sizeof(int16_t), static_cast<size_t>(received), file);
            else if (received < 0)
                std::printf("RX ошибка: %d (%s)\n", received, SoapySDRDevice_lastError());
        }
        SoapySDRDevice_deactivateStream(sdr, stream, 0, 0);
    }
    std::fclose(file);
    SoapySDRDevice_closeStream(sdr, stream);
    return result;
}

} // namespace

int main(int argc, char *argv[])
{
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

    const int result = mode == Mode::Tx ? run_tx(sdr, frequency) : run_rx(sdr, frequency);
    SoapySDRDevice_unmake(sdr);
    return result;
}
