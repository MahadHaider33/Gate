#include "audio/Voice.h"
#include "app/Preferences.h"
#include "audio/ClipPlayer.h"
#include "audio/Processing.h"
#include "audio/AppAudio.h"
#include "ui/ScrollMotion.h"
#include <windows.h>
#include <objbase.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <thread>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void voiceShortcut(){
    using namespace gate;
    Preferences p;const auto cleanup=pack(p.processing);
    require(!p.toggleVoice()&&p.voice.preset==VoicePreset::Normal,"No effect is invented before the first selection");
    p.voice.enabled=false;p.selectVoice(VoicePreset::Deep);
    require(!p.voice.enabled,"Selecting a tile still leaves disabled effects off");
    require(p.toggleVoice()&&p.voice.enabled&&p.voice.preset==VoicePreset::Deep,"Shortcut enables the last selected effect from bypass");
    require(p.toggleVoice()&&p.voice.preset==VoicePreset::Normal,"Shortcut returns to Normal");
    require(p.toggleVoice()&&p.voice.preset==VoicePreset::Deep,"Shortcut restores the remembered effect");
    p.selectVoice(VoicePreset::High);p.selectVoice(VoicePreset::Normal);
    require(p.toggleVoice()&&p.voice.preset==VoicePreset::High,"Manual Normal selection preserves the newest effect");
    p.selectVoice(VoicePreset::Custom);p.voice.custom[VoiceControl::Pitch]=37;p.voice.intensity=42;
    p.toggleVoice();p.toggleVoice();
    require(p.voice.preset==VoicePreset::Custom&&p.voice.custom[VoiceControl::Pitch]==37&&p.voice.intensity==42,"Custom controls and intensity survive repeated switches");
    require(pack(p.processing)==cleanup&&p.hearSounds,"Voice shortcut leaves cleanup and sound monitoring untouched");
    require(GlobalShortcut::valid(GlobalShortcut::defaultVoice)&&GlobalShortcut::valid(VK_F8)&&GlobalShortcut::valid(0),"Default, function key and cleared shortcuts accepted");
    for(uint32_t key:{uint32_t('V'),uint32_t('1'),uint32_t(VK_NUMPAD1),uint32_t(VK_INSERT),uint32_t(VK_PAUSE),uint32_t(VK_CAPITAL),uint32_t(VK_RETURN),uint32_t(VK_TAB),uint32_t(VK_ESCAPE),uint32_t(VK_BACK),uint32_t(VK_DELETE),uint32_t(VK_F12)})
        require(GlobalShortcut::valid(key),"Unmodified action keys are not restricted by Gate");
    require(GlobalShortcut::valid('V'|(MOD_SHIFT<<8))&&GlobalShortcut::valid('V'|(MOD_WIN<<8)),"Shift and Windows chords reach native registration");
    require(GlobalShortcut::typingKey('A')&&GlobalShortcut::typingKey('Z')&&GlobalShortcut::typingKey('0')&&GlobalShortcut::typingKey('9')&&GlobalShortcut::typingKey(VK_NUMPAD0)&&GlobalShortcut::typingKey(VK_NUMPAD9),"Only bare letters and numbers need the typing confirmation");
    for(uint32_t key:{0u,uint32_t(VK_SPACE),uint32_t(VK_OEM_PERIOD),uint32_t(VK_INSERT),uint32_t(VK_F8),uint32_t('A'|(MOD_SHIFT<<8)),uint32_t('1'|(MOD_CONTROL<<8))})
        require(!GlobalShortcut::typingKey(key),"Other keys and modified letters/numbers do not get a typing confirmation");
    require(!GlobalShortcut::valid(VK_SHIFT)&&!GlobalShortcut::valid('V'|0x10000u),"Modifier-only and malformed bindings are not action chords");
    struct TestWindow {HWND value=CreateWindowExW(0,L"STATIC",L"Gate shortcut test",0,0,0,0,0,HWND_MESSAGE,nullptr,GetModuleHandleW(nullptr),nullptr);~TestWindow(){if(value)DestroyWindow(value);}} first,second;
    require(first.value&&second.value,"Create hidden shortcut test windows");
    GlobalShortcut owner,conflict;
    const uint32_t chord=VK_F23|((MOD_CONTROL|MOD_ALT|MOD_SHIFT)<<8);
    require(owner.set(first.value,1,chord)&&owner.active(),"Register a global shortcut for a hidden window");
    require(!conflict.set(second.value,1,chord)&&!conflict.active(),"Conflicting global shortcut remains inactive");
    owner.clear();
    require(conflict.set(second.value,1,chord),"Clearing a shortcut releases the chord");
    require(conflict.set(second.value,1,0)&&!conflict.active(),"Clearing registration disables the shortcut");
    require(owner.set(first.value,1,chord),"Released chord can be registered again");
    Preferences sounds;sounds.clips={{L"one.wav",L"One"},{L"two.wav",L"Two"}};
    require(!sounds.clips[0].shortcut&&!sounds.clips[1].shortcut,"Sound shortcuts start unassigned");
    sounds.clips[0].shortcut='1'|((MOD_CONTROL|MOD_ALT)<<8);
    sounds.clips[1].shortcut='2'|((MOD_CONTROL|MOD_ALT)<<8);
    require(sounds.shortcutOwner(sounds.voiceShortcut)==-1,"Sound shortcut conflicts include the voice changer");
    require(sounds.shortcutOwner(sounds.clips[0].shortcut,1)==0,"Another sound cannot use an assigned chord");
    require(sounds.shortcutOwner(sounds.clips[0].shortcut,0)==-2&&sounds.shortcutOwner(0)==-2,"Editing the same binding and clearing are allowed");
    const auto copied=sounds.clips;sounds.clips[0].shortcut=0;
    require(copied!=sounds.clips,"Shortcut edits count as metadata changes for deferred saving");
    const auto remaining=sounds.clips[1].shortcut;sounds.clips.erase(sounds.clips.begin());sounds.clips[0].name=L"Renamed";
    require(sounds.shortcutOwner(remaining)==0&&sounds.clips[0].file==L"two.wav","Shortcuts follow their sound through removal and rename");
}
void scrolling(){
    gate::ScrollMotion slow,fast;slow.retarget(600);fast.retarget(600);
    for(unsigned i=0;i<10;++i)slow.advance(.016);
    for(unsigned i=0;i<20;++i)fast.advance(.008);
    require(std::abs(slow.position-fast.position)<.001,"Scrolling covers the same distance at different frame rates");
    const double position=fast.position,velocity=fast.velocity;fast.retarget(900);
    require(fast.position==position&&fast.velocity==velocity,"More wheel input preserves scrolling motion");
    fast.advance(.008);require(fast.position>position,"Rapid input does not postpone animation");
    fast.retarget(0);const double reversed=fast.position;fast.advance(.008);
    require(fast.position<reversed,"Reversing the wheel responds immediately");
    bool moving=true;for(unsigned i=0;i<100&&moving;++i){moving=fast.advance(.016);require(fast.position>=0,"Scroll never overshoots the edge");}
    require(!moving&&fast.position==0&&fast.velocity==0,"Scroll settles completely without idle animation");
}
void voiceAndMix(){
    gate::VoiceEffect effect;std::array<float,gate::frameSize> input{},out{},clip{},cable{},listener{};
    input.fill(.1f);effect.process(input.data(),out.data(),{});
    for(unsigned i=0;i<input.size();++i)require(std::abs(input[i]-out[i])<.00001f,"Normal voice bypass");
    for(unsigned preset=1;preset<gate::voiceCount;++preset){
        for(unsigned block=0;block<12;++block)effect.process(input.data(),out.data(),{gate::VoicePreset(preset),75,true});
        for(float value:out)require(std::isfinite(value)&&std::abs(value)<=1.f,"Effect output bounded");
    }
    for(unsigned block=0;block<12;++block)effect.process(input.data(),out.data(),{gate::VoicePreset::Robot,75,false});
    require(std::abs(out.back()-.1f)<.0001f,"Effects off restores normal voice");
    clip.fill(.4f);
    gate::mixAudio(input.data(),clip.data(),cable.data(),listener.data(),gate::frameSize,.5f,false,true);
    require(std::abs(cable[0]-.3f)<.00001f&&std::abs(listener[0]-.2f)<.00001f,"Clip monitoring excludes microphone");
    gate::mixAudio(input.data(),clip.data(),cable.data(),listener.data(),gate::frameSize,.5f,true,false);
    require(std::abs(cable[0]-.3f)<.00001f&&std::abs(listener[0]-.1f)<.00001f,"Microphone monitoring excludes clips");
    gate::mixAudio(input.data(),clip.data(),cable.data(),listener.data(),gate::frameSize,.5f,false,false);
    require(listener[0]==0&&cable[0]>.29f,"Monitoring off leaves cable mix active");
    std::array<float,gate::frameSize> media{};media.fill(.6f);
    gate::mixAudio(input.data(),clip.data(),cable.data(),listener.data(),gate::frameSize,.5f,true,true,media.data(),.5f);
    require(std::abs(cable[0]-.6f)<.00001f&&std::abs(listener[0]-.3f)<.00001f,"Media joins cable but never duplicates local audio");
    gate::mixAudio(input.data(),clip.data(),cable.data(),listener.data(),gate::frameSize,.5f,false,false,media.data(),0.f);
    require(std::abs(cable[0]-.3f)<.00001f&&listener[0]==0,"Media mute preserves microphone and clips");
    media.fill(1.f);
    gate::mixAudio(input.data(),clip.data(),cable.data(),listener.data(),gate::frameSize,1.f,false,false,media.data(),1.f);
    require(cable[0]==1.f&&listener[0]==0,"Combined media mix cannot exceed full scale");
    media.fill(.6f);
    gate::mixAudio(input.data(),clip.data(),cable.data(),listener.data(),gate::frameSize,.5f,true,true,media.data(),.5f,0.f);
    require(std::abs(cable[0]-.5f)<.00001f&&std::abs(listener[0]-.2f)<.00001f,"Microphone mute leaves media and soundboard playing");
    gate::mixAudio(input.data(),clip.data(),cable.data(),listener.data(),gate::frameSize,.5f,true,false,media.data(),0.f,.5f);
    require(std::abs(cable[0]-.25f)<.00001f&&std::abs(listener[0]-.05f)<.00001f,"Independent microphone gain affects call and voice monitoring");
    gate::mixAudio(input.data(),clip.data(),cable.data(),listener.data(),gate::frameSize,.5f,true,true,media.data(),0.f,0.f);
    require(std::abs(cable[0]-.2f)<.00001f&&std::abs(listener[0]-.2f)<.00001f,"Both channels muted still preserve soundboard");
}
void expandedVoices(){
    using namespace gate;
    // Check real signal behaviour, not just parameter mappings.
    auto render=[](VoiceParameters p,bool impulse=false){
        VoiceEffect effect;std::array<float,frameSize> in{},out{};std::vector<float> samples;
        samples.reserve(sampleRate);
        for(unsigned block=0;block<100;++block){
            for(unsigned i=0;i<frameSize;++i){const auto n=block*frameSize+i;in[i]=impulse?(n==9600?.5f:0.f):.12f*std::sin(2*3.14159265358979323846f*220.f*float(n)/sampleRate);}
            effect.process(in.data(),out.data(),p);
            for(float v:out){require(std::isfinite(v)&&std::abs(v)<=1,"Expanded preset output stays bounded");samples.push_back(v);}
        }
        return samples;
    };
    auto frequency=[](const std::vector<float>& samples){unsigned crossings=0;for(unsigned i=sampleRate/2+1;i<samples.size();++i)if(samples[i-1]<=0&&samples[i]>0)++crossings;return float(crossings)*2;};
    const auto high=frequency(render({VoicePreset::High,100,true}));
    const auto medium=frequency(render({VoicePreset::High,50,true}));
    const auto deep=frequency(render({VoicePreset::Deep,100,true}));
    require(std::abs(high-220.f*std::pow(2.f,7.f/12.f))<10,"High pitch retains +7 semitones");
    require(std::abs(medium-220.f*std::pow(2.f,3.5f/12.f))<10&&medium<high-35,"Intensity changes pitch rather than blending two voices");
    require(std::abs(deep-220.f*std::pow(2.f,-5.f/12.f))<10,"Deep shifts pitch down");
    VoiceParameters custom{VoicePreset::Custom,75,true};custom.custom[VoiceControl::Pitch]=-120;
    require(std::abs(frequency(render(custom))-110)<10,"Custom negative pitch reaches an octave down");
    for(unsigned i=1;i<voiceCount;++i){const auto result=render({VoicePreset(i),100,true});double energy=0;for(unsigned j=sampleRate/2;j<result.size();++j)energy+=result[j]*result[j];require(energy>.0001,"Every voice produces audible signal");}
    const auto echoed=render({VoicePreset::Echo,100,true},true);
    auto energy=[](const std::vector<float>& values,unsigned begin,unsigned end){double sum=0;for(unsigned i=begin;i<end;++i)sum+=values[i]*values[i];return sum;};
    const double first=energy(echoed,22000,26000),second=energy(echoed,35000,40000);
    require(first>1e-5&&second>1e-7&&second<first*.6,"Echo has multiple decaying repeats");
    const auto room=render({VoicePreset::Reverb,100,true},true);
    require(energy(room,12000,22000)>1e-6,"Reverb creates a decaying room tail");
    VoiceEffect effect;std::array<float,frameSize> in{},out{};
    in.fill(.12f);custom.custom[VoiceControl::Drive]=100;custom.custom[VoiceControl::Bass]=18;custom.custom[VoiceControl::Echo]=100;custom.custom[VoiceControl::Reverb]=100;
    for(unsigned i=0;i<30;++i)effect.process(in.data(),out.data(),custom);
    custom.enabled=false;for(unsigned i=0;i<20;++i)effect.process(in.data(),out.data(),custom);
    for(float v:out)require(v==.12f,"Custom off becomes exact bypass");
    in.fill(0);custom.enabled=true;
    for(unsigned i=0;i<100;++i){effect.process(in.data(),out.data(),custom);for(float v:out)require(std::abs(v)<1e-6,"Re-enable cannot replay old effect tails");}
    in[0]=std::numeric_limits<float>::quiet_NaN();in[1]=std::numeric_limits<float>::infinity();
    effect.process(in.data(),out.data(),custom);for(float v:out)require(std::isfinite(v),"Invalid samples cannot poison effect state");
    // Simultaneous UI writes must not publish torn multi-slider configurations.
    VoiceMailbox mailbox;VoiceParameters a{VoicePreset::Custom,20,false},b{VoicePreset::Deep,90,true};
    a.custom[VoiceControl::Pitch]=-120;b.custom[VoiceControl::Pitch]=120;
    a.custom[VoiceControl::Bass]=-12;b.custom[VoiceControl::Bass]=18;mailbox.store(a);
    std::thread writer([&]{for(unsigned i=0;i<20000;++i)mailbox.store(i%2?a:b);});
    bool coherent=true;VoiceParameters snapshot=a;
    for(unsigned i=0;i<20000;++i){mailbox.read(snapshot);const auto& expected=snapshot.intensity==20?a:b;coherent&=snapshot.intensity==expected.intensity&&snapshot.preset==expected.preset&&snapshot.enabled==expected.enabled&&snapshot.custom==expected.custom;}
    writer.join();require(coherent,"Audio reads coherent custom control snapshots without waiting");
    for(unsigned i=0;i<voiceCount;++i){const auto p=unpackVoice(packVoice({VoicePreset(i),63,false}));require(unsigned(p.preset)==i&&p.intensity==63&&!p.enabled,"Preset IDs persist without enabling effects");}
    std::cout<<"Pitch Hz: deep="<<deep<<", half-high="<<medium<<", high="<<high<<". Expanded voice checks passed.\n";
}
void appCaptureLifecycle(){
    gate::AppAudio capture;std::array<float,gate::frameSize> block{};
    for(unsigned cycle=0;cycle<3;++cycle){
        capture.start({});
        for(unsigned i=0;i<200&&!capture.finished();++i)Sleep(5);
        require(capture.finished()&&!capture.active()&&!capture.error().empty(),"Invalid app fails without capturing system audio");
        capture.stop();block.fill(1.f);capture.read(block.data());
        for(float value:block)require(value==0,"Stopped capture clears buffered media");
    }
    capture.start({});capture.stop();require(!capture.active()&&capture.finished(),"Immediate cancellation completes");
}
void playback(){
    GUID guid{};CoCreateGuid(&guid);wchar_t name[40]{};StringFromGUID2(guid,name,40);
    const auto path=std::filesystem::temp_directory_path()/(std::wstring(L"Gate-test-")+name+L".wav");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}} cleanup{path};
    {
        std::ofstream file(path,std::ios::binary);
        auto word=[&](uint16_t v){file.write(reinterpret_cast<const char*>(&v),2);};
        auto number=[&](uint32_t v){file.write(reinterpret_cast<const char*>(&v),4);};
        file.write("RIFF",4);number(36+9600);file.write("WAVEfmt ",8);number(16);word(1);word(1);number(48000);number(96000);word(2);word(16);file.write("data",4);number(9600);
        for(int i=0;i<4800;++i)word(uint16_t(int16_t(std::sin(i*.0576)*8000)));
    }
    gate::ClipPlayer player;std::array<float,gate::frameSize> output{};
    player.start(path.wstring(),true);Sleep(30);player.read(output.data(),gate::frameSize);
    for(float value:output)require(value==0,"Held playback waits for output routing");
    player.release();float energy=0;
    for(unsigned i=0;i<1000&&player.playing();++i){player.read(output.data(),gate::frameSize);for(float v:output)energy+=std::abs(v);Sleep(2);}
    require(player.error().empty()&&energy>1&&!player.playing(),"Decode WAV and finish playback");
    player.start(path.wstring());player.stop();player.read(output.data(),gate::frameSize);
    for(float value:output)require(value==0,"Stop clears clip audio");
    require(!player.playing(),"Stop clears playing state");
    // Formats outside the direct PCM path still use the general decoder and
    // resampler. Exercise that fallback without opening a physical device.
    {
        std::ofstream file(path,std::ios::binary|std::ios::trunc);
        auto word=[&](uint16_t v){file.write(reinterpret_cast<const char*>(&v),2);};
        auto number=[&](uint32_t v){file.write(reinterpret_cast<const char*>(&v),4);};
        file.write("RIFF",4);number(36+17640);file.write("WAVEfmt ",8);number(16);word(1);word(2);number(44100);number(176400);word(4);word(16);file.write("data",4);number(17640);
        for(int i=0;i<4410;++i){const auto sample=uint16_t(int16_t(std::sin(i*.0627)*8000));word(sample);word(sample);}
    }
    player.start(path.wstring());energy=0;
    for(unsigned i=0;i<1000&&player.playing();++i){player.read(output.data(),gate::frameSize);for(float value:output)energy+=std::abs(value);Sleep(2);}
    require(player.error().empty()&&energy>1&&!player.playing(),"Stereo 44.1 kHz WAV retains decoder fallback and resampling");
}
void starterPack(){
    GUID guid{};CoCreateGuid(&guid);wchar_t name[40]{};StringFromGUID2(guid,name,40);
    const auto folder=std::filesystem::temp_directory_path()/(std::wstring(L"Gate-pack-test-")+name);
    struct Cleanup {std::filesystem::path folder;~Cleanup(){std::error_code error;std::filesystem::remove_all(folder,error);}} cleanup{folder};
    gate::Preferences preferences;preferences.clips.push_back({L"custom.wav",L"My sound"});
    require(preferences.installStarterSounds(folder.wstring()),"Install embedded starter pack");
    require(preferences.clips.size()==7&&preferences.clips.front().name==L"My sound","Preserve existing imports");
    for(size_t i=1;i<preferences.clips.size();++i){
        gate::ClipPlayer player;player.start((folder/preferences.clips[i].file).wstring());
        std::array<float,gate::frameSize> samples{};float energy=0;
        for(unsigned tries=0;tries<1000&&player.playing()&&energy<1;++tries){player.read(samples.data(),gate::frameSize);for(float v:samples)energy+=std::abs(v);Sleep(2);}
        require(player.error().empty()&&energy>1,"Embedded starter WAV decodes");player.stop();
    }
    preferences.clips[1].name=L"My horn";
    const auto removed=folder/preferences.clips.back().file;std::filesystem::remove(removed);preferences.clips.pop_back();
    require(!preferences.installStarterSounds(folder.wstring()),"Starter installation is one-time");
    require(preferences.clips.size()==6&&preferences.clips[1].name==L"My horn"&&!std::filesystem::exists(removed),"Keep starter renames and removals");
    const auto applausePath=folder/L"starter-v1-applause.wav";
    for(unsigned oldVersion:{1u,2u}){
        preferences.clips.push_back({L"starter-v1-applause.wav",L"Renamed applause"});
        preferences.clips.push_back({L"imported-applause.wav",L"Applause"});
        {std::ofstream old(applausePath,std::ios::binary);old<<"old audio";}
        preferences.starterPackVersion=oldVersion;
        require(preferences.installStarterSounds(folder.wstring()),"Migrate previous starter pack");
        require(!std::filesystem::exists(applausePath)&&preferences.clips.size()==7&&preferences.clips.back().file==L"imported-applause.wav"&&preferences.clips[1].name==L"My horn"&&!std::filesystem::exists(removed),"Remove only bundled applause while preserving imports and other tile edits");
        preferences.clips.pop_back();
        require(!preferences.installStarterSounds(folder.wstring()),"Removal migration is one-time");
    }
}

}
int main(){try{voiceShortcut();scrolling();voiceAndMix();expandedVoices();appCaptureLifecycle();playback();starterPack();std::cout<<"Voice shortcut, scroll motion, voice bypass, independent media mixes, capture lifecycle, clip playback/stop, and starter-pack installation passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
