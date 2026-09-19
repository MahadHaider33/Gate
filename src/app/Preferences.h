#pragma once
#include "audio/Processing.h"
#include "audio/Voice.h"
#include "app/GlobalShortcut.h"
#include <string>
#include <vector>
namespace gate {
struct SoundClip {
    std::wstring file,name;
    std::wstring emoji=L"\U0001F3B5";
    uint32_t shortcut=0;
    bool operator==(const SoundClip&) const = default;
};
struct Preferences {
    Parameters processing;
    std::wstring microphone, listener;
    bool trayExplained = false;
    VoiceParameters voice;
    VoicePreset lastVoicePreset=VoicePreset::Normal;
    uint32_t voiceShortcut=GlobalShortcut::defaultVoice;
    void selectVoice(VoicePreset preset) noexcept {
        voice.preset=preset;
        if(preset!=VoicePreset::Normal)lastVoicePreset=preset;
    }
    bool toggleVoice() noexcept {
        if(voice.preset!=VoicePreset::Normal){
            lastVoicePreset=voice.preset;
            if(voice.enabled){voice.preset=VoicePreset::Normal;return true;}
        }
        if(lastVoicePreset==VoicePreset::Normal)return false;
        voice.preset=lastVoicePreset;voice.enabled=true;return true;
    }
    bool hearSounds=true;
    unsigned starterPackVersion=0;
    std::vector<SoundClip> clips;
    // -1 identifies the voice shortcut; -2 means no matching owner.
    int shortcutOwner(uint32_t value,int except=-2) const noexcept {
        if(!value)return -2;
        if(except!=-1&&voiceShortcut==value)return -1;
        for(size_t i=0;i<clips.size();++i)if(int(i)!=except&&clips[i].shortcut==value)return int(i);
        return -2;
    }
    static std::wstring soundDirectory();
    static std::wstring clipPath(const std::wstring& file);
    bool installStarterSounds(const std::wstring& directory=soundDirectory());
    static Preferences load();
    void save(bool includeClips=true) const;
};
}
