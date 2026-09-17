#pragma once
#include <speex/speex_resampler.h>
#include <cstdint>
namespace gate {
class Resampler {
public:
    Resampler() = default;
    ~Resampler();
    Resampler(const Resampler&) = delete;
    Resampler& operator=(const Resampler&) = delete;
    void prepare(unsigned from, unsigned to);
    void release() noexcept;
    bool process(const float* in, uint32_t& inCount, float* out, uint32_t& outCount) noexcept;
    unsigned latency() const noexcept;
private:
    SpeexResamplerState* state_ = nullptr;
};
}
