#include "audio/Resampler.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace gate {
Resampler::~Resampler() { release(); }
void Resampler::release() noexcept {
    if (state_) { speex_resampler_reset_mem(state_); speex_resampler_destroy(state_); state_ = nullptr; }
}
void Resampler::prepare(unsigned from, unsigned to) {
    release();
    if (from == to) return;
    int error = 0;
    state_ = speex_resampler_init(1, from, to, 3, &error);
    if (!state_ || error) throw std::runtime_error("Cannot initialize SpeexDSP resampler");
    speex_resampler_skip_zeros(state_);
}
bool Resampler::process(const float* in, uint32_t& inCount, float* out, uint32_t& outCount) noexcept {
    if (!state_) {
        inCount = outCount = std::min(inCount, outCount);
        std::memcpy(out, in, inCount * sizeof(float));
        return true;
    }
    return speex_resampler_process_float(state_, 0, in, &inCount, out, &outCount) == RESAMPLER_ERR_SUCCESS;
}
unsigned Resampler::latency() const noexcept { return state_ ? speex_resampler_get_output_latency(state_) : 0; }
}
