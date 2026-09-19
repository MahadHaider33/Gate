#include "audio/Voice.h"
#include "audio/VoiceDsp.h"
#include <signalsmith-stretch.h>
namespace gate {
uint32_t packVoice(VoiceParameters p) noexcept {
    return std::min(unsigned(p.preset),voiceCount-1)|(std::min(p.intensity,100u)<<8)|(uint32_t(p.enabled)<<16);
}
VoiceParameters unpackVoice(uint32_t v) noexcept {
    return {VoicePreset(std::min(v&255u,voiceCount-1)),std::min((v>>8)&255u,100u),bool(v&(1u<<16))};
}
VoiceMailbox::VoiceMailbox() noexcept {store({});}
void VoiceMailbox::store(const VoiceParameters& p) noexcept {
    ++revision_;packed_=packVoice(p);
    for(unsigned i=0;i<voiceControlCount;++i)custom_[i]=std::clamp(p.custom.values[i],voiceControlInfo[i].min,voiceControlInfo[i].max);
    ++revision_;
}
void VoiceMailbox::read(VoiceParameters& previous) const noexcept {
    const auto version=revision_.load();if(version&1)return;
    auto next=unpackVoice(packed_.load());
    for(unsigned i=0;i<voiceControlCount;++i)next.custom.values[i]=custom_[i].load();
    if(revision_.load()==version)previous=next;
}
using namespace voice_dsp;
struct VoiceEffect::Impl {
    signalsmith::stretch::SignalsmithStretch<float> pitch{0};
    Delay<4096> aligned;Delay<sampleRate> echo;Delay<2048> vibrato;
    Vocoder vocoder;Room room;
    cycfi::q::highpass highpass{cycfi::q::frequency(20),float(sampleRate)};
    cycfi::q::lowpass lowpass{cycfi::q::frequency(20000),float(sampleRate)};
    cycfi::q::lowshelf bass{0,cycfi::q::frequency(180),float(sampleRate)};
    cycfi::q::highshelf treble{0,cycfi::q::frequency(3000),float(sampleRate)};
    cycfi::q::highpass dc{cycfi::q::frequency(20),float(sampleRate)};
    std::array<float,frameSize> shifted{},safe{};
    Recipe current=recipe({});
    float wet=0,modPhase=0,vibratoPhase=0,echoDamp=0,envelope=0,compressorGain=1,drivePrevious=0,pitchMix=0;
    float driveGain=1,outputGain=1,makeupGain=1;
    unsigned latency=0,warmup=0,pitchWarmup=0;bool idle=true,pitchRunning=false;
    Impl(){pitch.configure(1,1024,256);pitch.setFormantBase(0);latency=unsigned(pitch.inputLatency()+pitch.outputLatency());}
    void clear() noexcept {
        aligned.clear();echo.clear();vibrato.clear();room.clear();vocoder.clear();
        clearFilter(highpass);clearFilter(lowpass);clearFilter(bass);clearFilter(treble);clearFilter(dc);
        modPhase=vibratoPhase=echoDamp=envelope=drivePrevious=pitchMix=0;compressorGain=driveGain=outputGain=makeupGain=1;pitchRunning=false;warmup=latency;
    }
};
VoiceEffect::VoiceEffect():impl_(std::make_unique<Impl>()) {}
VoiceEffect::~VoiceEffect()=default;
unsigned VoiceEffect::latencySamples() const noexcept {return impl_->latency;}
void VoiceEffect::process(const float* input,float* output,VoiceParameters p) noexcept {
    auto& s=*impl_;
    const bool enabled=p.enabled&&p.preset!=VoicePreset::Normal&&(p.preset==VoicePreset::Custom||p.intensity>0);
    // Exact zero-latency bypass after a short fade, with no spectral processing.
    if(!enabled&&s.wet<.00001f){s.wet=0;s.idle=true;for(unsigned i=0;i<frameSize;++i)output[i]=clean(input[i]);return;}
    NoDenormals noDenormals;
    const auto target=recipe(p);
    if(s.idle){s.clear();s.current=target;s.idle=false;}
    Recipe next=s.current,step;
    for(unsigned i=0;i<voiceControlCount;++i){next.v[i]+=(target.v[i]-next.v[i])*.25f;step.v[i]=(next.v[i]-s.current.v[i])/frameSize;}
    const bool needsPitch=std::abs(next[C::Pitch])>.005f||std::abs(next[C::Formant])>.005f;
    for(unsigned i=0;i<frameSize;++i)s.safe[i]=clean(input[i]);
    if(needsPitch){
        if(!s.pitchRunning){s.pitch.reset();s.pitchWarmup=s.latency;s.pitchMix=0;s.pitchRunning=true;}
        s.pitch.setTransposeSemitones(next[C::Pitch]);s.pitch.setFormantSemitones(next[C::Formant],true);
        const float* inputs[]={s.safe.data()};float* outputs[]={s.shifted.data()};s.pitch.process(inputs,frameSize,outputs,frameSize);
    }else{s.pitchRunning=false;s.pitchMix=0;}
    s.highpass.config(cycfi::q::frequency(next[C::Highpass]),float(sampleRate));
    s.lowpass.config(cycfi::q::frequency(next[C::Lowpass]),float(sampleRate));
    s.bass.config(next[C::Bass],cycfi::q::frequency(180),float(sampleRate));
    s.treble.config(next[C::Treble],cycfi::q::frequency(3000),float(sampleRate));
    const float driveStep=(1+next[C::Drive]*next[C::Drive]*70-s.driveGain)/frameSize;
    const float makeupStep=(dbGain(next[C::Compression]*5)-s.makeupGain)/frameSize;
    const float outputStep=(dbGain(next[C::Output])-s.outputGain)/frameSize;
    for(unsigned i=0;i<frameSize;++i){
        s.driveGain+=driveStep;s.makeupGain+=makeupStep;s.outputGain+=outputStep;
        for(unsigned j=0;j<voiceControlCount;++j)s.current.v[j]+=step.v[j];
        const auto& r=s.current;
        const float dry=s.aligned.read(float(s.latency));s.aligned.push(s.safe[i]);
        if(needsPitch){if(s.pitchWarmup)--s.pitchWarmup;else s.pitchMix+=(1-s.pitchMix)*.004f;}
        float x=needsPitch?dry+(clean(s.shifted[i])-dry)*s.pitchMix:dry;
        if(r[C::Robot]>.0001f)x+=(s.vocoder.process(x,r[C::RobotPitch])-x)*r[C::Robot];
        s.vibratoPhase+=r[C::VibratoRate]/sampleRate;if(s.vibratoPhase>=1)s.vibratoPhase-=1;
        const float vibrated=r[C::Vibrato]>.0001f?s.vibrato.read(240.f+190.f*std::sin(2*pi*s.vibratoPhase)*r[C::Vibrato]):x;s.vibrato.push(x);
        x+=(vibrated-x)*r[C::Vibrato];
        s.modPhase+=r[C::ModRate]/sampleRate;if(s.modPhase>=1)s.modPhase-=1;
        if(r[C::Modulation]>.0001f)x*=1-r[C::Modulation]+r[C::Modulation]*std::sin(2*pi*s.modPhase)*1.4f;
        const float hp=s.highpass(x);if(r[C::Highpass]>20.1f)x=hp;
        const float lp=s.lowpass(x);if(r[C::Lowpass]<19999.f)x=lp;
        x=s.treble(s.bass(x));
        const float magnitude=std::abs(x);s.envelope+=(magnitude-s.envelope)*(magnitude>s.envelope?.01f:.00025f);
        if(i%16==0){const float over=std::max(1.f,s.envelope/.16f);s.compressorGain=std::pow(over,-.8f*r[C::Compression])*s.makeupGain;}
        x*=s.compressorGain;
        // Midpoint oversampling softens the distortion's harshest aliases.
        const float crushed=r[C::Drive]>.0001f?.5f*(std::tanh((s.drivePrevious+x)*.5f*s.driveGain)+std::tanh(x*s.driveGain)):x;s.drivePrevious=x;
        x+=(crushed*.8f-x)*r[C::Drive];
        const float blocked=s.dc(x);if(r[C::Drive]>.0001f)x=blocked;
        const float repeated=s.echo.read(r[C::EchoTime]*sampleRate*.001f);
        s.echoDamp+=.28f*(repeated-s.echoDamp);
        s.echo.push(clean((r[C::Echo]>.0001f?x:0.f)+s.echoDamp*r[C::EchoFeedback]));
        x+=repeated*r[C::Echo]*.75f;
        const float room=s.room.process(r[C::Reverb]>.0001f?x:0.f);
        x+=room*r[C::Reverb]*1.3f;
        x=(dry+(x-dry)*r[C::Mix])*s.outputGain;
        if(s.warmup)--s.warmup;else s.wet+=((enabled?1.f:0.f)-s.wet)*.004f;
        output[i]=clean(s.safe[i]+(clean(x)-s.safe[i])*s.wet);
    }
    s.current=next;
}
void mixAudio(const float* voice,const float* clip,float* cable,float* listener,unsigned count,float gain,bool hearVoice,bool hearSounds,const float* media,float mediaGain,float voiceGain) noexcept {
    for(unsigned i=0;i<count;++i){const float sound=clip[i]*gain,speech=voice[i]*voiceGain;cable[i]=clean(speech+sound+(media?media[i]*mediaGain:0.f));listener[i]=clean((hearVoice?speech:0.f)+(hearSounds?sound:0.f));}
}
}
