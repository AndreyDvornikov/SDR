#include "sdr/pcm_player.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace sdr {

PCMPlayer::PCMPlayer() = default;

bool PCMPlayer::load_file(const std::string& filepath) {
    FILE* file = std::fopen(filepath.c_str(), "rb");
    if (!file) {
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    long file_size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (file_size < 4) {
        std::fclose(file);
        return false;
    }

    // Limit maximum load size to 64M complex samples (256 MB) to keep memory smooth
    constexpr long max_bytes = 256 * 1024 * 1024;
    long bytes_to_read = std::min(file_size, max_bytes);
    size_t int16_count = bytes_to_read / sizeof(int16_t);
    int16_count = (int16_count / 2) * 2; // ensure even for I/Q pairs

    std::vector<int16_t> raw(int16_count);
    size_t read_bytes = std::fread(raw.data(), sizeof(int16_t), int16_count, file);
    std::fclose(file);

    if (read_bytes < 2) {
        return false;
    }

    size_t num_complex = read_bytes / 2;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.resize(num_complex);

        for (size_t i = 0; i < num_complex; ++i) {
            float I = static_cast<float>(raw[2 * i]);
            float Q = static_cast<float>(raw[2 * i + 1]);
            samples_[i] = std::complex<float>(I, Q);
        }

        current_filepath_ = filepath;
        current_filename_ = std::filesystem::path(filepath).filename().string();
        total_samples_.store(num_complex);
        current_index_.store(0);
        is_loaded_.store(true);
        is_playing_.store(true);

        calculate_stats();
    }

    return true;
}

void PCMPlayer::calculate_stats() {
    if (samples_.empty()) {
        peak_amplitude_ = 1.0f;
        percentile_99_5_ = 1.0f;
        return;
    }

    // Subsample to calculate percentile quickly
    size_t step = std::max<size_t>(1, samples_.size() / 50000);
    std::vector<float> magnitudes;
    magnitudes.reserve(samples_.size() / step + 1);

    float max_val = 0.0f;
    for (size_t i = 0; i < samples_.size(); i += step) {
        float abs_i = std::abs(samples_[i].real());
        float abs_q = std::abs(samples_[i].imag());
        float m = std::max(abs_i, abs_q);
        magnitudes.push_back(m);
        if (m > max_val) max_val = m;
    }

    peak_amplitude_ = max_val > 1.0f ? max_val : 1.0f;

    if (!magnitudes.empty()) {
        std::sort(magnitudes.begin(), magnitudes.end());
        size_t idx = static_cast<size_t>(magnitudes.size() * 0.995f);
        if (idx >= magnitudes.size()) idx = magnitudes.size() - 1;
        percentile_99_5_ = magnitudes[idx] > 1.0f ? magnitudes[idx] : 1.0f;
    } else {
        percentile_99_5_ = peak_amplitude_;
    }
}

void PCMPlayer::play() {
    is_playing_.store(true);
}

void PCMPlayer::pause() {
    is_playing_.store(false);
}

void PCMPlayer::toggle_play() {
    is_playing_.store(!is_playing_.load());
}

void PCMPlayer::reset() {
    current_index_.store(0);
}

void PCMPlayer::seek(size_t sample_index) {
    size_t tot = total_samples_.load();
    if (tot == 0) return;
    current_index_.store(std::min(sample_index, tot - 1));
}

void PCMPlayer::seek_ratio(float ratio) {
    size_t tot = total_samples_.load();
    if (tot == 0) return;
    float clamped = std::clamp(ratio, 0.0f, 1.0f);
    size_t idx = static_cast<size_t>(clamped * (tot - 1));
    current_index_.store(idx);
}

float PCMPlayer::playback_ratio() const {
    size_t tot = total_samples_.load();
    if (tot == 0) return 0.0f;
    return static_cast<float>(current_index_.load()) / static_cast<float>(tot);
}

size_t PCMPlayer::read_samples(std::complex<float>* out_buffer, size_t count, bool loop) {
    if (!is_loaded_.load() || count == 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    size_t tot = samples_.size();
    if (tot == 0) return 0;

    size_t cur = current_index_.load();
    size_t written = 0;

    while (written < count) {
        size_t avail = tot - cur;
        size_t chunk = std::min(count - written, avail);

        std::copy(samples_.begin() + cur, samples_.begin() + cur + chunk, out_buffer + written);
        written += chunk;
        cur += chunk;

        if (cur >= tot) {
            if (loop) {
                cur = 0;
            } else {
                is_playing_.store(false);
                break;
            }
        }
    }

    if (is_playing_.load()) {
        current_index_.store(cur);
    }
    return written;
}

size_t PCMPlayer::peek_samples(std::complex<float>* out_buffer, size_t count) const {
    if (!is_loaded_.load() || count == 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    size_t tot = samples_.size();
    if (tot == 0) return 0;

    size_t cur = current_index_.load();
    for (size_t i = 0; i < count; ++i) {
        out_buffer[i] = samples_[(cur + i) % tot];
    }
    return count;
}

void PCMPlayer::get_window(std::vector<float>& out_i, std::vector<float>& out_q, size_t window_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t tot = samples_.size();
    if (tot == 0) {
        out_i.clear();
        out_q.clear();
        return;
    }

    size_t cur = current_index_.load();
    size_t count = std::min(window_size, tot);
    size_t start = (cur + count <= tot) ? cur : (tot - count);

    out_i.resize(count);
    out_q.resize(count);

    for (size_t i = 0; i < count; ++i) {
        out_i[i] = samples_[start + i].real();
        out_q[i] = samples_[start + i].imag();
    }
}

} // namespace sdr
