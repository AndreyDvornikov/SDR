#include <iostream>
#include <random>
#include <vector>
#include <complex>

std::vector<bool> generate_bits(int size) {
    std::vector<bool> bits(size);

    std::mt19937 rng(std::random_device{}());
    std::bernoulli_distribution distribution(0.5);

    for (int i = 0; i < size; ++i) {
        bits[i] = distribution(rng);
    }

    return bits;
}

std::vector<float> bpsk(const std::vector<bool>& bits) {
    std::vector<float> sym;
    sym.reserve(bits.size());

    for (bool bit : bits) {
        sym.push_back(bit ? 1.0f : -1.0f);
    }

    return sym;
}

std::vector<std::complex<float>> qpsk(const std::vector<bool>& bits) {
    std::vector<std::complex<float>> sym;
    sym.reserve((bits.size() + 1) /2 );

    constexpr float scale = 0.70710678f; // 1/sqrt(2)

    for (std::size_t i = 0; i +1 < bits.size(); i += 2) {
        const float I = bits[i] ? scale : -scale;
        const float Q = bits[i+1] ? scale : -scale;

        sym.emplace_back(I, Q);
    }

    return sym;
}

template <typename T>
std::vector<T> upsample(const std::vector<T>& symbols, size_t factor) {
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

template <typename T>
std::vector<T> convolution(const std::vector<T>& samples) {
    std::vector<float> h(4, 1.0f);
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

int main() {
    auto bits = generate_bits(16);
    auto sym = qpsk(bits);
    auto samples = upsample(sym, 4);
    auto signal = convolution(samples);
    for (auto s : signal) {
        std::cout << s << " ";
    }
    std::cout << "\n";
}
