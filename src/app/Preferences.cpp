#include "app/Preferences.h"
#include <windows.h>
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
    RegCloseKey(key);return p;
}
void Preferences::save() const {
    HKEY key{}; if(RegCreateKeyExW(HKEY_CURRENT_USER,keyPath,0,nullptr,0,KEY_WRITE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return;
    auto number=[&](const wchar_t* name,DWORD value){RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));};
    auto string=[&](const wchar_t* name,const std::wstring& value){RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),DWORD((value.size()+1)*sizeof(wchar_t)));};
    number(L"Version",1);number(L"Processing",pack(processing));number(L"TrayExplained",trayExplained);
    string(L"Microphone",microphone);string(L"Listener",listener);RegCloseKey(key);
}
}
