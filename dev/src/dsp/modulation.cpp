#include "dsp/modulation.hpp"

#include <cmath>
#include <random>
#include <algorithm>

namespace sdr::dsp {

const char* modulation_to_string(ModulationType type) {
    switch (type) {
        case ModulationType::BPSK: return "BPSK";
        case ModulationType::QPSK: return "QPSK";
        default: return "Unknown";
    }
}

std::vector<bool> generate_bits(size_t count) {
    std::vector<bool> bits(count);
    std::mt19937 rng(std::random_device{}());
    std::bernoulli_distribution dist(0.5);
    for (size_t i = 0; i < count; ++i) {
        bits[i] = dist(rng);
    }
    return bits;
}

std::vector<bool> string_to_bits(const std::string& str) {
    std::vector<bool> bits;
    bits.reserve(str.size() * 8);
    for (unsigned char c : str) {
        for (int i = 7; i >= 0; --i) {
            bits.push_back((c >> i) & 1);
        }
    }
    return bits;
}

std::string bits_to_string(const std::vector<bool>& bits) {
    std::string str;
    str.reserve(bits.size() / 8);
    for (size_t i = 0; i + 7 < bits.size(); i += 8) {
        unsigned char c = 0;
        for (int b = 0; b < 8; ++b) {
            c = (c << 1) | (bits[i + b] ? 1 : 0);
        }
        str.push_back(static_cast<char>(c));
    }
    return str;
}

std::vector<std::complex<float>> bpsk(const std::vector<bool>& bits) {
    std::vector<std::complex<float>> sym;
    sym.reserve(bits.size());
    for (bool bit : bits) {
        sym.emplace_back(bit ? 1.0f : -1.0f, 0.0f);
    }
    return sym;
}

std::vector<std::complex<float>> qpsk(const std::vector<bool>& bits) {
    std::vector<std::complex<float>> sym;
    sym.reserve((bits.size() + 1) / 2);
    constexpr float scale = 0.70710678f; // 1/sqrt(2)

    for (size_t i = 0; i + 1 < bits.size(); i += 2) {
        const float I = bits[i] ? scale : -scale;
        const float Q = bits[i + 1] ? scale : -scale;
        sym.emplace_back(I, Q);
    }
    return sym;
}

std::vector<std::complex<float>> modulate(ModulationType type, const std::vector<bool>& bits) {
    switch (type) {
        case ModulationType::BPSK: return bpsk(bits);
        case ModulationType::QPSK: return qpsk(bits);
        default: return qpsk(bits);
    }
}

std::vector<bool> demod_qpsk(const std::vector<std::complex<float>>& symbols) {
    std::vector<bool> bits;
    bits.reserve(symbols.size() * 2);
    for (const auto& s : symbols) {
        bits.push_back(s.real() >= 0.0f);
        bits.push_back(s.imag() >= 0.0f);
    }
    return bits;
}

std::vector<bool> demod_bpsk(const std::vector<std::complex<float>>& symbols) {
    std::vector<bool> bits;
    bits.reserve(symbols.size());
    for (const auto& s : symbols) {
        bits.push_back(s.real() >= 0.0f);
    }
    return bits;
}

std::vector<std::complex<float>> get_reference_constellation(ModulationType type) {
    std::vector<std::complex<float>> ref;
    switch (type) {
        case ModulationType::BPSK:
            ref = {{-1.0f, 0.0f}, {1.0f, 0.0f}};
            break;
        case ModulationType::QPSK: {
            constexpr float scale = 0.70710678f;
            ref = {
                {-scale, -scale},
                {-scale, scale},
                {scale, -scale},
                {scale, scale}
            };
            break;
        }
    }
    return ref;
}

} // namespace sdr::dsp
