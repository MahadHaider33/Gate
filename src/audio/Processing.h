#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <rnnoise.h>
#include <q/fx/delay.hpp>
#include <q/fx/envelope.hpp>
#include <q/fx/noise_gate.hpp>

namespace gate {
constexpr unsigned sampleRate = 48000;
constexpr unsigned frameSize = 480;
// Pinned RNNoise: one analysis-overlap frame plus one delayed spectrum frame.
constexpr unsigned processingDelay = 2 * frameSize;
struct Parameters {
    bool suppression = true;
    bool gate = false;
    unsigned strength = 100;
    int thresholdDb = -50;
};
uint32_t pack(Parameters p) noexcept;
Parameters unpack(uint32_t value) noexcept;

class Processor {
public:
    Processor();
    ~Processor();
    Processor(const Processor&) = delete;
    Processor& operator=(const Processor&) = delete;
    void prepare(Parameters parameters);
    void process(const float* input, float* output, Parameters parameters) noexcept;
    static constexpr unsigned latencySamples() { return processingDelay; }
private:
    DenoiseState* state_ = nullptr;
    std::array<float, frameSize> scaled_{}, wet_{};
    cycfi::q::nf_delay dryDelay_{size_t(processingDelay)};
    cycfi::q::ar_envelope_follower mix_;
    cycfi::q::peak_envelope_follower detector_;
    cycfi::q::ar_envelope_follower gain_;
    cycfi::q::noise_gate gate_;
    unsigned warmup_ = 0;
    bool ran_ = false;
};
}
