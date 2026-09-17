#include "audio/Processing.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace cycfi::q::literals;
namespace gate {
uint32_t pack(Parameters p) noexcept {
    return uint32_t(p.suppression) | (uint32_t(p.gate) << 1)
        | (std::min(p.strength, 100u) << 8)
        | (uint32_t(std::clamp(p.thresholdDb, -70, -20) + 70) << 16);
}
Parameters unpack(uint32_t v) noexcept {
    return {bool(v & 1), bool(v & 2), std::min((v >> 8) & 255, 100u),
            int(std::min((v >> 16) & 255, 50u)) - 70};
}
Processor::Processor() : mix_(10_ms, 10_ms, float(sampleRate)),
    detector_(30_ms, float(sampleRate)), gain_(5_ms, 150_ms, float(sampleRate)),
    gate_(-50_dB, -56_dB) {
    if (rnnoise_get_frame_size() != frameSize) throw std::runtime_error("Unsupported RNNoise frame size");
    state_ = rnnoise_create(nullptr);
    if (!state_) throw std::bad_alloc();
}
Processor::~Processor() { rnnoise_destroy(state_); }
void Processor::prepare(Parameters p) {
    // Called only before the capture worker starts; model initialisation is not RT safe.
    if (rnnoise_init(state_, nullptr) != 0) throw std::runtime_error("RNNoise initialization failed");
    dryDelay_.clear();
    mix_ = p.suppression ? float(p.strength) / 100.f : 0.f;
    gain_ = 1.f;
    detector_ = 0.f;
    gate_ = cycfi::q::noise_gate(cycfi::q::dB(p.thresholdDb),cycfi::q::dB(p.thresholdDb-6));
    scaled_.fill(0.f); wet_.fill(0.f);
    ran_ = false;
    warmup_ = 0;
}
void Processor::process(const float* input, float* output, Parameters p) noexcept {
    float target = p.suppression ? float(p.strength) / 100.f : 0.f;
    const bool run = target > 0.f || mix_() > 0.0001f;
    if (run) {
        if (!ran_) warmup_ = 3;
        for (unsigned i = 0; i < frameSize; ++i) scaled_[i] = input[i] * 32768.f;
        rnnoise_process_frame(state_, wet_.data(), scaled_.data());
        if (warmup_) { --warmup_; target = 0.f; }
    }
    ran_ = run;
    gate_.onset_threshold(cycfi::q::dB(p.thresholdDb));
    gate_.release_threshold(cycfi::q::dB(p.thresholdDb - 6));
    for (unsigned i = 0; i < frameSize; ++i) {
        const float dry = dryDelay_(input[i], processingDelay - 1);
        const float mix = run ? mix_(target) : (mix_ = 0.f)();
        const float cleaned = dry * (1.f - mix) + (run ? wet_[i] / 32768.f * mix : 0.f);
        const bool open = gate_(detector_(std::abs(cleaned)));
        const float gain = gain_(!p.gate || open ? 1.f : 0.f);
        output[i] = std::isfinite(cleaned) ? std::clamp(cleaned * gain, -1.f, 1.f) : 0.f;
    }
}
}
