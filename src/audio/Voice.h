#pragma once
#include <array>
#include <memory>
#include <cstdint>
namespace gate {
enum class VoicePreset : unsigned { Normal, Deep, High, Robot, Radio, Echo };
inline constexpr const wchar_t* voiceNames[]={L"Normal",L"Deep",L"High Pitch",L"Robot",L"Radio",L"Echo"};
struct VoiceParameters { VoicePreset preset=VoicePreset::Normal; unsigned intensity=75; bool enabled=true; };
uint32_t packVoice(VoiceParameters p) noexcept;
VoiceParameters unpackVoice(uint32_t value) noexcept;
class VoiceEffect {
public:
    VoiceEffect();
    ~VoiceEffect();
    void process(const float* input,float* output,VoiceParameters parameters) noexcept;
    unsigned latencySamples() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Monitoring is independent of the cable mix. Always sanitize the final sum.
void mixAudio(const float* voice,const float* clip,float* cable,float* listener,
              unsigned count,float clipGain,bool hearVoice,bool hearSounds) noexcept;
}
