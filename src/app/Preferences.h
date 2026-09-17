#pragma once
#include "audio/Processing.h"
#include "audio/Voice.h"
#include <string>
#include <vector>
namespace gate {
struct SoundClip {
    std::wstring file,name;
    std::wstring emoji=L"\U0001F3B5";
    bool operator==(const SoundClip&) const = default;
};
struct Preferences {
    Parameters processing;
    std::wstring microphone, listener;
    bool trayExplained = false;
    VoiceParameters voice;
    bool hearSounds=true;
    unsigned starterPackVersion=0;
    std::vector<SoundClip> clips;
    static std::wstring soundDirectory();
    static std::wstring clipPath(const std::wstring& file);
    bool installStarterSounds(const std::wstring& directory=soundDirectory());
    static Preferences load();
    void save(bool includeClips=true) const;
};
}
