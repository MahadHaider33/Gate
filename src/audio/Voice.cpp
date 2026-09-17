#include "audio/Voice.h"
#include "audio/Processing.h"
#include <signalsmith-stretch.h>
#include <q/fx/lowpass.hpp>
#include <q/synth/sin_osc.hpp>
#include <algorithm>
#include <cmath>
namespace gate {
uint32_t packVoice(VoiceParameters p) noexcept {
    return std::min(unsigned(p.preset),5u)|(std::min(p.intensity,100u)<<8)|(uint32_t(p.enabled)<<16);
}
VoiceParameters unpackVoice(uint32_t v) noexcept {
    return {VoicePreset(std::min(v&255u,5u)),std::min((v>>8)&255u,100u),bool(v&(1u<<16))};
}
struct VoiceEffect::Impl {
    signalsmith::stretch::SignalsmithStretch<float> pitch{0};
    cycfi::q::nf_delay aligned{4096},echo{size_t(sampleRate)};
    cycfi::q::leaky_integrator low{cycfi::q::frequency(3200),float(sampleRate)};
    cycfi::q::leaky_integrator bass{cycfi::q::frequency(350),float(sampleRate)};
    cycfi::q::phase_iterator phase{cycfi::q::frequency(70),float(sampleRate)};
    std::array<float,frameSize> shifted{};
    std::array<float,6> weights{1,0,0,0,0,0};
    float amount=.75f,semitones=0;
    unsigned latency=0;
    Impl() { pitch.configure(1,1024,256);latency=unsigned(pitch.inputLatency()+pitch.outputLatency()); }
};
VoiceEffect::VoiceEffect():impl_(std::make_unique<Impl>()) {}
VoiceEffect::~VoiceEffect()=default;
unsigned VoiceEffect::latencySamples() const noexcept {return impl_->latency;}
void VoiceEffect::process(const float* input,float* output,VoiceParameters p) noexcept {
    auto& s=*impl_;const unsigned selected=p.enabled?std::min(unsigned(p.preset),5u):0;
    const float target=float(std::min(p.intensity,100u))/100.f;
    // Keep the spectral history warm; preset changes never allocate or reset it.
    const float pitchTarget=p.preset==VoicePreset::Deep?-7.f:p.preset==VoicePreset::High?7.f:s.semitones;
    s.semitones+=(pitchTarget-s.semitones)*.25f;
    s.pitch.setTransposeSemitones(s.semitones);
    const float* inputs[]={input};float* outputs[]={s.shifted.data()};
    s.pitch.process(inputs,frameSize,outputs,frameSize);
    for(unsigned i=0;i<frameSize;++i){
        const float dry=s.aligned(input[i],s.latency-1);
        s.amount+=(target-s.amount)*.002f;
        const float radio=std::tanh(3.f*s.low(dry-s.bass(dry)))*.65f;
        const float robot=dry*cycfi::q::sin(s.phase++);
        const float echo=s.echo(dry,size_t(sampleRate/4)-1);
        const float values[]={input[i],dry+(s.shifted[i]-dry)*s.amount,dry+(s.shifted[i]-dry)*s.amount,
            dry+(robot-dry)*s.amount,dry+(radio-dry)*s.amount,dry+echo*s.amount*.6f};
        float sum=0;
        for(unsigned j=0;j<6;++j){s.weights[j]+=((j==selected?1.f:0.f)-s.weights[j])*.004f;sum+=values[j]*s.weights[j];}
        output[i]=std::isfinite(sum)?std::clamp(sum,-1.f,1.f):0.f;
    }
}
void mixAudio(const float* voice,const float* clip,float* cable,float* listener,unsigned count,float gain,bool hearVoice,bool hearSounds) noexcept {
    auto protect=[](float v){return std::isfinite(v)?std::clamp(v,-1.f,1.f):0.f;};
    for(unsigned i=0;i<count;++i){const float sound=clip[i]*gain;cable[i]=protect(voice[i]+sound);listener[i]=protect((hearVoice?voice[i]:0.f)+(hearSounds?sound:0.f));}
}
}
