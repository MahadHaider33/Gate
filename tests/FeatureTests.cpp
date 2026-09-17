#include "audio/Voice.h"
#include "app/Preferences.h"
#include "audio/ClipPlayer.h"
#include "audio/Processing.h"
#include <windows.h>
#include <objbase.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void voiceAndMix(){
    gate::VoiceEffect effect;std::array<float,gate::frameSize> input{},out{},clip{},cable{},listener{};
    input.fill(.1f);effect.process(input.data(),out.data(),{});
    for(unsigned i=0;i<input.size();++i)require(std::abs(input[i]-out[i])<.00001f,"Normal voice bypass");
    for(unsigned preset=1;preset<6;++preset){
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
int main(){try{voiceAndMix();playback();starterPack();std::cout<<"Voice bypass, clip playback/stop, independent mixes, and starter-pack installation passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
