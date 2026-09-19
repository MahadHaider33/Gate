#include "app/Preferences.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <algorithm>
#include <stdexcept>
namespace gate {
namespace {
constexpr wchar_t keyPath[]=L"Software\\Gate";
std::wstring readString(HKEY key,const wchar_t* name) {
    wchar_t value[2048]{}; DWORD size=sizeof(value);
    if(RegGetValueW(key,nullptr,name,RRF_RT_REG_SZ,nullptr,value,&size)!=ERROR_SUCCESS)return {};
    return value;
}
DWORD readDword(HKEY key,const wchar_t* name,DWORD fallback) {
    DWORD value{},size=sizeof(value);
    return RegGetValueW(key,nullptr,name,RRF_RT_REG_DWORD,nullptr,&value,&size)==ERROR_SUCCESS?value:fallback;
}
}
Preferences Preferences::load() {
    Preferences p; HKEY key{};
    if(RegOpenKeyExW(HKEY_CURRENT_USER,keyPath,0,KEY_READ,&key)!=ERROR_SUCCESS)return p;
    if(readDword(key,L"Version",0)==1) {
        p.processing=unpack(readDword(key,L"Processing",pack({})));
        p.microphone=readString(key,L"Microphone");p.listener=readString(key,L"Listener");
        p.trayExplained=readDword(key,L"TrayExplained",0)!=0;
    }
    if(readDword(key,L"FeaturesVersion",0)==1){
        p.voice=unpackVoice(readDword(key,L"Voice",packVoice({})));
        p.lastVoicePreset=p.voice.preset;
        if(readDword(key,L"VoiceShortcutVersion",0)==1){
            const auto last=readDword(key,L"LastVoicePreset",unsigned(p.lastVoicePreset));
            if(last<voiceCount)p.lastVoicePreset=VoicePreset(last);
            const auto shortcut=readDword(key,L"VoiceShortcut",p.voiceShortcut);
            p.voiceShortcut=GlobalShortcut::valid(shortcut)?shortcut:0;
        }
        if(p.voice.preset!=VoicePreset::Normal)p.lastVoicePreset=p.voice.preset;
        if(readDword(key,L"CustomVoiceVersion",0)==1){
            HKEY custom{};
            if(RegOpenKeyExW(key,L"CustomVoice",0,KEY_READ,&custom)==ERROR_SUCCESS){
                for(unsigned i=0;i<voiceControlCount;++i){const auto& info=voiceControlInfo[i];p.voice.custom.values[i]=std::clamp(int32_t(readDword(custom,info.key,DWORD(info.initial))),info.min,info.max);}
                RegCloseKey(custom);
            }
        }
        p.hearSounds=readDword(key,L"HearSounds",1)!=0;
        p.starterPackVersion=readDword(key,L"StarterPackVersion",0);
        const auto count=std::min(readDword(key,L"ClipCount",0),10000ul);
        const bool clipShortcuts=readDword(key,L"ClipShortcutsVersion",0)==1;
        for(unsigned i=0;i<count;++i){
            HKEY clip{};const auto name=L"Clips\\"+std::to_wstring(i);
            if(RegOpenKeyExW(key,name.c_str(),0,KEY_READ,&clip)!=ERROR_SUCCESS)continue;
            SoundClip c;c.file=readString(clip,L"File");c.name=readString(clip,L"Name");
            const auto emoji=readString(clip,L"Emoji");if(!emoji.empty())c.emoji=emoji.substr(0,32);
            if(clipShortcuts){const auto shortcut=readDword(clip,L"Shortcut",0);if(GlobalShortcut::valid(shortcut))c.shortcut=shortcut;}
            RegCloseKey(clip);if(!clipPath(c.file).empty())p.clips.push_back(std::move(c));
        }
    }
    RegCloseKey(key);return p;
}
void Preferences::save(bool includeClips) const {
    HKEY key{}; if(RegCreateKeyExW(HKEY_CURRENT_USER,keyPath,0,nullptr,0,KEY_WRITE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return;
    auto number=[&](const wchar_t* name,DWORD value){RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));};
    auto string=[&](const wchar_t* name,const std::wstring& value){RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),DWORD((value.size()+1)*sizeof(wchar_t)));};
    number(L"Version",1);number(L"Processing",pack(processing));number(L"TrayExplained",trayExplained);
    string(L"Microphone",microphone);string(L"Listener",listener);
    number(L"FeaturesVersion",1);number(L"Voice",packVoice(voice));number(L"HearSounds",hearSounds);
    number(L"VoiceShortcutVersion",1);number(L"VoiceShortcut",voiceShortcut);number(L"LastVoicePreset",unsigned(lastVoicePreset));
    HKEY custom{};
    if(RegCreateKeyExW(key,L"CustomVoice",0,nullptr,0,KEY_WRITE,nullptr,&custom,nullptr)==ERROR_SUCCESS){
        for(unsigned i=0;i<voiceControlCount;++i){const auto& info=voiceControlInfo[i];const DWORD value=DWORD(std::clamp(voice.custom.values[i],info.min,info.max));RegSetValueExW(custom,info.key,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));}
        RegCloseKey(custom);number(L"CustomVoiceVersion",1);
    }
    number(L"StarterPackVersion",starterPackVersion);
    if(!includeClips){RegCloseKey(key);return;}
    RegDeleteTreeW(key,L"Clips");
    for(size_t i=0;i<clips.size();++i){
        HKEY child{};const auto name=L"Clips\\"+std::to_wstring(i);
        if(RegCreateKeyExW(key,name.c_str(),0,nullptr,0,KEY_WRITE,nullptr,&child,nullptr)!=ERROR_SUCCESS)continue;
        const auto& c=clips[i];
        auto str=[&](const wchar_t* n,const std::wstring& v){RegSetValueExW(child,n,0,REG_SZ,reinterpret_cast<const BYTE*>(v.c_str()),DWORD((v.size()+1)*sizeof(wchar_t)));};
        str(L"File",c.file);str(L"Name",c.name);
        str(L"Emoji",c.emoji);
        const DWORD shortcut=GlobalShortcut::valid(c.shortcut)?c.shortcut:0;
        RegSetValueExW(child,L"Shortcut",0,REG_DWORD,reinterpret_cast<const BYTE*>(&shortcut),sizeof(shortcut));
        RegCloseKey(child);
    }
    number(L"ClipCount",DWORD(clips.size()));number(L"ClipShortcutsVersion",1);RegCloseKey(key);
}
std::wstring Preferences::soundDirectory(){
    PWSTR value=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&value)))return {};
    std::wstring result=std::wstring(value)+L"\\Gate\\Soundboard";CoTaskMemFree(value);return result;
}
std::wstring Preferences::clipPath(const std::wstring& file){
    if(file.empty()||file.find_first_of(L"/\\:")!=std::wstring::npos||file==L"."||file==L"..")return {};
    const auto folder=soundDirectory();return folder.empty()?std::wstring{}:folder+L"\\"+file;
}
bool Preferences::installStarterSounds(const std::wstring& directory){
    if(starterPackVersion>=4)return false;
    struct Starter {unsigned resource;const wchar_t* file;const wchar_t* name;const wchar_t* emoji;};
    constexpr Starter pack[]={
        {201,L"starter-v1-air-horn.wav",L"Air Horn",L"📣"},
        {203,L"starter-v1-drum-roll.wav",L"Drum Roll",L"🥁"},
        {204,L"starter-v1-rimshot.wav",L"Rimshot",L"🥁"},
        {205,L"starter-v1-buzzer.wav",L"Buzzer",L"🚨"},
        {206,L"starter-v1-chime.wav",L"Chime",L"🔔"},
        {207,L"starter-v1-sad-trombone.wav",L"Sad Trombone",L"🎺"}
    };
    // Upgrade generic starter icons once; keep any emoji the user has chosen.
    for(auto& clip:clips){
        if(clip.emoji!=L"🎵"&&!clip.emoji.empty())continue;
        for(const auto& sound:pack)if(clip.file==sound.file){clip.emoji=sound.emoji;break;}
    }
    if(starterPackVersion==3){starterPackVersion=4;return true;}
    const bool updating=starterPackVersion>0;
    if(directory.empty())throw std::runtime_error("Soundboard directory unavailable");
    std::filesystem::create_directories(directory);
    const auto removedFile=std::filesystem::path(directory)/L"starter-v1-applause.wav";
    std::filesystem::remove(removedFile);
    std::erase_if(clips,[](const SoundClip& clip){return clip.file==L"starter-v1-applause.wav";});
    // Existing libraries keep their imports, customizations, and removed tiles.
    if(updating){starterPackVersion=4;return true;}
    const HMODULE module=GetModuleHandleW(nullptr);
    for(const auto& sound:pack){
        const auto resource=FindResourceW(module,MAKEINTRESOURCEW(sound.resource),RT_RCDATA);
        if(!resource)throw std::runtime_error("Starter sound resource missing");
        const DWORD size=SizeofResource(module,resource);
        const auto data=LockResource(LoadResource(module,resource));
        if(!data||size<44)throw std::runtime_error("Starter sound resource invalid");
        const auto path=std::filesystem::path(directory)/sound.file;
        if(!std::filesystem::exists(path)){
            // Publish only a complete file; interrupted extraction is retried.
            const auto temporary=path.wstring()+L".installing";
            HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot write starter sound");
            DWORD written=0;const bool complete=WriteFile(file,data,size,&written,nullptr)&&written==size;
            CloseHandle(file);
            if(!complete||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH|MOVEFILE_REPLACE_EXISTING)){
                DeleteFileW(temporary.c_str());throw std::runtime_error("Cannot install starter sound");
            }
        }
        if(std::none_of(clips.begin(),clips.end(),[&](const SoundClip& clip){return clip.file==sound.file;}))
            clips.push_back({sound.file,sound.name,sound.emoji});
    }
    starterPackVersion=4;
    return true;
}
}
