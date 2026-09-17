#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <span>
namespace gate {
// Transport queue, not a recording. One producer and one consumer. Reset only when both stopped.
template<size_t Capacity> class SpscQueue {
public:
    size_t push(std::span<const float> input) noexcept {
        const auto w = write_.load(std::memory_order_relaxed);
        const auto r = read_.load(std::memory_order_acquire);
        const auto n = std::min(input.size(), Capacity - (w - r));
        for (size_t i = 0; i < n; ++i) data_[(w + i) % Capacity] = input[i];
        write_.store(w + n, std::memory_order_release);
        return n;
    }
    size_t pop(std::span<float> output) noexcept {
        const auto r = read_.load(std::memory_order_relaxed);
        const auto w = write_.load(std::memory_order_acquire);
        const auto n = std::min(output.size(), w - r);
        for (size_t i = 0; i < n; ++i) {
            output[i] = data_[(r + i) % Capacity];
            data_[(r + i) % Capacity] = 0.f;
        }
        read_.store(r + n, std::memory_order_release);
        return n;
    }
    size_t size() const noexcept {
        const auto r = read_.load(std::memory_order_acquire);
        const auto w = write_.load(std::memory_order_acquire);
        return w >= r ? std::min(w-r, Capacity) : 0;
    }
    void reset() noexcept { data_.fill(0.f); write_ = 0; read_ = 0; }
private:
    std::array<float, Capacity> data_{};
    alignas(64) std::atomic<size_t> write_{0};
    alignas(64) std::atomic<size_t> read_{0};
};
}
