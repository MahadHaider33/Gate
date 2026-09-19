#pragma once
#include <windows.h>
#include <cstdint>

namespace gate {
// RegisterHotKey delivers on the existing UI thread, including while hidden.
// No keyboard hook, polling timer, or audio-thread work is needed.
class GlobalShortcut {
public:
    static constexpr uint32_t defaultVoice='V'|((MOD_CONTROL|MOD_ALT)<<8);
    static bool valid(uint32_t value) noexcept {
        if(!value)return true;
        const unsigned key=value&255,modifiers=value>>8;
        if(modifiers&~unsigned(MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_WIN))return false;
        // Modifiers form a chord with an action key. Let Windows decide which
        // actual keys/chords are available rather than imposing a Gate policy.
        return key>=VK_BACK&&key<255&&key!=VK_SHIFT&&key!=VK_CONTROL&&key!=VK_MENU&&
            key!=VK_LWIN&&key!=VK_RWIN&&!(key>=VK_LSHIFT&&key<=VK_RMENU);
    }
    static bool typingKey(uint32_t value) noexcept {
        if(value>>8)return false;
        return (value>='A'&&value<='Z')||(value>='0'&&value<='9')||(value>=VK_NUMPAD0&&value<=VK_NUMPAD9);
    }
    GlobalShortcut()=default;
    GlobalShortcut(const GlobalShortcut&)=delete;
    GlobalShortcut& operator=(const GlobalShortcut&)=delete;
    ~GlobalShortcut(){clear();}
    void clear() noexcept {if(active_)UnregisterHotKey(owner_,id_);active_=false;}
    bool set(HWND owner,int id,uint32_t value) noexcept {
        clear();owner_=owner;id_=id;
        if(!valid(value))return false;
        if(!value)return true;
        active_=RegisterHotKey(owner,id,(value>>8)|MOD_NOREPEAT,value&255)!=FALSE;
        return active_;
    }
    bool active() const noexcept {return active_;}
private:
    HWND owner_{};
    int id_{};
    bool active_=false;
};
}
