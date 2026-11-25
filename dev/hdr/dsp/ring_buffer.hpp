#pragma once

#include <vector>
#include <mutex>
#include <cstddef>
#include <algorithm>

namespace sdr::dsp {

template <typename T>
class ThreadSafeRingBuffer {
public:
    explicit ThreadSafeRingBuffer(size_t capacity = 65536)
        : buffer_(capacity), capacity_(capacity), head_(0), tail_(0), size_(0) {}

    void set_capacity(size_t new_cap) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.resize(new_cap);
        capacity_ = new_cap;
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

    size_t write(const T* data, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t available = capacity_ - size_;
        size_t to_write = std::min(count, available);

        for (size_t i = 0; i < to_write; ++i) {
            buffer_[head_] = data[i];
            head_ = (head_ + 1) % capacity_;
        }
        size_ += to_write;
        return to_write;
    }

    // Overwriting write: if buffer full, drops oldest
    void write_overwrite(const T* data, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < count; ++i) {
            buffer_[head_] = data[i];
            head_ = (head_ + 1) % capacity_;
            if (size_ < capacity_) {
                size_++;
            } else {
                tail_ = (tail_ + 1) % capacity_; // drop oldest
            }
        }
    }

    size_t read(T* out_data, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t to_read = std::min(count, size_);

        for (size_t i = 0; i < to_read; ++i) {
            out_data[i] = buffer_[tail_];
            tail_ = (tail_ + 1) % capacity_;
        }
        size_ -= to_read;
        return to_read;
    }

    // Peek without advancing tail
    size_t peek_latest(T* out_data, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t available = std::min(count, size_);
        if (available == 0) return 0;

        // latest items are just before head_
        size_t start_idx = (head_ + capacity_ - available) % capacity_;
        for (size_t i = 0; i < available; ++i) {
            out_data[i] = buffer_[(start_idx + i) % capacity_];
        }
        return available;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    size_t head_;
    size_t tail_;
    size_t size_;
    mutable std::mutex mutex_;
};

} // namespace sdr::dsp
