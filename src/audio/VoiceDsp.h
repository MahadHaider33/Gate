#pragma once
// Private, preallocated building blocks for the live voice chain.
#include "audio/Voice.h"
#include "audio/Processing.h"
#include <q/fx/biquad.hpp>
#include <algorithm>
#include <cmath>
#include <xmmintrin.h>
namespace gate::voice_dsp {
struct NoDenormals {
    unsigned previous=_mm_getcsr();
    NoDenormals() noexcept {_mm_setcsr(previous|0x8040u);}
    ~NoDenormals(){_mm_setcsr(previous);}
};
using C=VoiceControl;
constexpr float pi=3.14159265358979323846f;
inline float clean(float x) noexcept {return std::isfinite(x)?std::clamp(x,-1.f,1.f):0.f;}
inline float dbGain(float db) noexcept {return std::pow(10.f,db*.05f);}
struct Recipe {
    std::array<float,voiceControlCount> v{};
    float& operator[](C c) noexcept {return v[unsigned(c)];}
    float operator[](C c) const noexcept {return v[unsigned(c)];}
};
inline Recipe recipe(VoiceParameters p) noexcept {
    CustomVoice settings;
    auto set=[&](C c,int v){settings[c]=v;};
    switch(p.preset){
    case VoicePreset::Deep:set(C::Pitch,-50);set(C::Formant,-22);set(C::Bass,3);set(C::Compression,15);break;
    // Retain High Pitch's original +7-semitone sound at full strength.
    case VoicePreset::High:set(C::Pitch,70);set(C::Formant,70);break;
    case VoicePreset::Masculine:set(C::Pitch,-30);set(C::Formant,-30);set(C::Bass,2);break;
    case VoicePreset::Feminine:set(C::Pitch,35);set(C::Formant,32);set(C::Treble,2);break;
    case VoicePreset::Child:set(C::Pitch,55);set(C::Formant,50);set(C::Bass,-3);set(C::Treble,2);break;
    case VoicePreset::Robot:set(C::Robot,100);set(C::RobotPitch,110);set(C::Compression,40);set(C::Highpass,90);break;
    case VoicePreset::Radio:set(C::Highpass,280);set(C::Lowpass,3800);set(C::Compression,65);set(C::Drive,26);break;
    case VoicePreset::Telephone:set(C::Highpass,480);set(C::Lowpass,2600);set(C::Compression,45);set(C::Drive,12);break;
    case VoicePreset::Echo:set(C::Echo,55);set(C::EchoFeedback,42);break;
    case VoicePreset::Monster:set(C::Pitch,-90);set(C::Formant,-55);set(C::Bass,5);set(C::Drive,35);set(C::Robot,24);set(C::RobotPitch,65);set(C::Reverb,18);break;
    case VoicePreset::Alien:set(C::Pitch,30);set(C::Formant,20);set(C::Modulation,65);set(C::ModRate,35);set(C::Vibrato,45);set(C::VibratoRate,55);set(C::Echo,18);set(C::EchoTime,110);break;
    case VoicePreset::Reverb:set(C::Reverb,65);break;
    case VoicePreset::LoudMic:set(C::Bass,18);set(C::Compression,90);set(C::Drive,100);set(C::Lowpass,8500);set(C::Output,-4);break;
    case VoicePreset::Custom:settings=p.custom;break;
    default:break;
    }
    Recipe r;const float amount=float(std::min(p.intensity,100u))*.01f;
    for(unsigned i=0;i<voiceControlCount;++i){
        const auto& info=voiceControlInfo[i];
        r.v[i]=float(std::clamp(settings.values[i],info.min,info.max));
        if(info.unit==VoiceUnit::Semitones||info.unit==VoiceUnit::TenthsHertz)r.v[i]*=.1f;
        if(info.unit==VoiceUnit::Percent)r.v[i]*=.01f;
    }
    // Strength changes the actual recipe. Custom has explicit per-effect knobs.
    if(p.preset!=VoicePreset::Custom){
        for(auto c:{C::Pitch,C::Formant,C::Bass,C::Treble,C::Compression,C::Drive,C::Robot,C::Modulation,C::Vibrato,C::Echo,C::Reverb,C::Output})r[c]*=amount;
        r[C::Highpass]=20.f*std::pow(r[C::Highpass]/20.f,amount);
        r[C::Lowpass]=20000.f*std::pow(r[C::Lowpass]/20000.f,amount);
    }
    return r;
}
template<unsigned N> struct Delay {
    std::array<float,N> data{};unsigned head=0;
    void clear() noexcept {data.fill(0);head=0;}
    void push(float value) noexcept {data[head]=value;if(++head==N)head=0;}
    float read(float samples) const noexcept {
        samples=std::clamp(samples,1.f,float(N-2));const auto n=unsigned(samples);
        const auto a=(head+N-n)%N,b=(a+N-1)%N;const float fraction=samples-float(n);
        return data[a]+fraction*(data[b]-data[a]);
    }
};
inline void clearFilter(cycfi::q::biquad& f) noexcept {f.x1=f.x2=f.y1=f.y2=0;}
// A band-limited saw carrier: PolyBLEP removes the discontinuity's worst aliases.
inline float saw(float phase,float step) noexcept {
    float correction=0;
    if(phase<step){const float t=phase/step;correction=t+t-t*t-1;}
    else if(phase>1-step){const float t=(phase-1)/step;correction=t*t+t+t+1;}
    return 2*phase-1-correction;
}
struct Vocoder {
    struct Band {
        cycfi::q::bandpass_cpg voice{cycfi::q::frequency(100),float(sampleRate),3.2};
        cycfi::q::bandpass_cpg carrier{voice};float envelope=0;
    };
    std::array<Band,16> bands;
    cycfi::q::highpass sibilants{cycfi::q::frequency(4500),float(sampleRate)};
    float phase=0;
    Vocoder(){for(unsigned i=0;i<bands.size();++i){const float f=120.f*std::pow(6500.f/120.f,float(i)/15.f);bands[i].voice.config(cycfi::q::frequency(f),float(sampleRate),3.2);bands[i].carrier=bands[i].voice;}}
    void clear() noexcept {for(auto& b:bands){clearFilter(b.voice);clearFilter(b.carrier);b.envelope=0;}clearFilter(sibilants);phase=0;}
    float process(float input,float tone) noexcept {
        const float step=tone/sampleRate;phase+=step;if(phase>=1)phase-=1;
        const float carrier=saw(phase,step);float sum=0;
        for(auto& b:bands){const float level=std::abs(b.voice(input));b.envelope+=(level-b.envelope)*(level>b.envelope?.009f:.0005f);sum+=b.carrier(carrier)*b.envelope;}
        // Retain unvoiced consonants for intelligibility.
        return sum*6.f+sibilants(input)*.32f;
    }
};
struct Room {
    std::array<Delay<2300>,4> comb;std::array<float,4> damp{};
    Delay<600> first,second;
    void clear() noexcept {for(auto& d:comb)d.clear();damp.fill(0);first.clear();second.clear();}
    float process(float x) noexcept {
        constexpr float lengths[]={1423,1789,1973,2137};float sum=0;
        for(unsigned i=0;i<4;++i){const float y=comb[i].read(lengths[i]);damp[i]+=.24f*(y-damp[i]);comb[i].push(x*.22f+damp[i]*.78f);sum+=y;}
        const float a=first.read(347),v=sum*.5f+a*.5f;first.push(v);
        const float b=second.read(113),w=a-v*.5f+b*.5f;second.push(w);return b-w*.5f;
    }
};
}
