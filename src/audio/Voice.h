#pragma once
#include <array>
#include <memory>
#include <cstdint>
#include <atomic>
namespace gate {
// Append presets so saved IDs from earlier releases retain their meaning.
enum class VoicePreset : unsigned { Normal, Deep, High, Robot, Radio, Echo, Masculine, Feminine, Child, Monster, Alien, Telephone, Reverb, LoudMic, Custom, Count };
inline constexpr unsigned voiceCount=unsigned(VoicePreset::Count);
inline constexpr const wchar_t* voiceNames[]={L"Normal",L"Deep",L"High Pitch",L"Robot",L"Radio",L"Echo",L"Masculine",L"Feminine",L"Baby / Child",L"Monster",L"Alien",L"Telephone",L"Reverb",L"Loud Mic",L"Custom"};
inline constexpr const wchar_t* voiceDescriptions[]={L"Natural voice",L"Low and warm",L"Helium / chipmunk",L"Vocoder voice",L"Over the air",L"Fading repeats",L"Deeper resonance",L"Lighter resonance",L"Small and bright",L"Dark and growling",L"Otherworldly",L"On the phone",L"Room ambience",L"Bass and blown-out grit",L"Build your own voice"};
enum class VoiceControl : unsigned { Pitch, Formant, Bass, Treble, Highpass, Lowpass, Compression, Drive, Robot, RobotPitch, Modulation, ModRate, Vibrato, VibratoRate, Echo, EchoTime, EchoFeedback, Reverb, Mix, Output, Count };
inline constexpr unsigned voiceControlCount=unsigned(VoiceControl::Count);
enum class VoiceUnit { Percent, Semitones, Decibels, Hertz, Milliseconds, TenthsHertz };
struct VoiceControlInfo {const wchar_t* name;const wchar_t* key;int min,max,initial;VoiceUnit unit;};
inline constexpr VoiceControlInfo voiceControlInfo[]={
    {L"Pitch",L"Pitch",-120,120,0,VoiceUnit::Semitones},
    {L"Resonance (formant)",L"Formant",-80,80,0,VoiceUnit::Semitones},
    {L"Bass",L"Bass",-12,18,0,VoiceUnit::Decibels},
    {L"Treble",L"Treble",-12,12,0,VoiceUnit::Decibels},
    {L"Low cut",L"Highpass",20,1200,20,VoiceUnit::Hertz},
    {L"High cut",L"Lowpass",1500,20000,20000,VoiceUnit::Hertz},
    {L"Compression",L"Compression",0,100,0,VoiceUnit::Percent},
    {L"Distortion",L"Drive",0,100,0,VoiceUnit::Percent},
    {L"Robot",L"Robot",0,100,0,VoiceUnit::Percent},
    {L"Robot tone",L"RobotPitch",60,300,110,VoiceUnit::Hertz},
    {L"Alien modulation",L"Modulation",0,100,0,VoiceUnit::Percent},
    {L"Modulation speed",L"ModRate",1,120,35,VoiceUnit::Hertz},
    {L"Vibrato",L"Vibrato",0,100,0,VoiceUnit::Percent},
    {L"Vibrato speed",L"VibratoRate",5,120,40,VoiceUnit::TenthsHertz},
    {L"Echo",L"Echo",0,100,0,VoiceUnit::Percent},
    {L"Echo delay",L"EchoTime",60,800,280,VoiceUnit::Milliseconds},
    {L"Echo feedback",L"EchoFeedback",0,75,35,VoiceUnit::Percent},
    {L"Reverb",L"Reverb",0,100,0,VoiceUnit::Percent},
    {L"Effect mix",L"Mix",0,100,100,VoiceUnit::Percent},
    {L"Output level",L"Output",-18,6,0,VoiceUnit::Decibels}
};
struct CustomVoice {
    std::array<int,voiceControlCount> values=[] {std::array<int,voiceControlCount> v{};for(unsigned i=0;i<voiceControlCount;++i)v[i]=voiceControlInfo[i].initial;return v;}();
    int& operator[](VoiceControl c) noexcept {return values[unsigned(c)];}
    int operator[](VoiceControl c) const noexcept {return values[unsigned(c)];}
    bool operator==(const CustomVoice&) const = default;
};
struct VoiceParameters { VoicePreset preset=VoicePreset::Normal; unsigned intensity=75; bool enabled=true; CustomVoice custom; };
uint32_t packVoice(VoiceParameters p) noexcept;
VoiceParameters unpackVoice(uint32_t value) noexcept;
// One UI writer, one audio reader. Readers keep their previous coherent snapshot
// if a write overlaps; they never wait or acquire a mutex on the audio thread.
class VoiceMailbox {
public:
    VoiceMailbox() noexcept;
    void store(const VoiceParameters& parameters) noexcept;
    void read(VoiceParameters& previous) const noexcept;
private:
    std::atomic<uint32_t> revision_{0},packed_{0};
    std::array<std::atomic<int>,voiceControlCount> custom_{};
};
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
              unsigned count,float clipGain,bool hearVoice,bool hearSounds,
              const float* media=nullptr,float mediaGain=0.f,float voiceGain=1.f) noexcept;
}
