#pragma once

#include <complex>
#include <cstdint>
#include <string>
#include <vector>

namespace sdr::dsp {

enum class ModulationType {
    BPSK,
    QPSK
};

const char* modulation_to_string(ModulationType type);

// Генерация случайных бит
std::vector<bool> generate_bits(size_t count);

// Преобразование строки в биты и обратно
std::vector<bool> string_to_bits(const std::string& str);
std::string bits_to_string(const std::vector<bool>& bits);

// Модуляция (точно как в Лаб. 8)
std::vector<std::complex<float>> bpsk(const std::vector<bool>& bits);
std::vector<std::complex<float>> qpsk(const std::vector<bool>& bits);
std::vector<std::complex<float>> modulate(ModulationType type, const std::vector<bool>& bits);

// Демодуляция (жесткие решения)
std::vector<bool> demod_qpsk(const std::vector<std::complex<float>>& symbols);
std::vector<bool> demod_bpsk(const std::vector<std::complex<float>>& symbols);

// Опорные созвездия (для отрисовки эталонных точек)
std::vector<std::complex<float>> get_reference_constellation(ModulationType type);

} // namespace sdr::dsp
