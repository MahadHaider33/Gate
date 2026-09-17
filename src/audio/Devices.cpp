#include <initguid.h>
#include "audio/Devices.h"
#include <functiondiscoverykeys_devpkey.h>
#include <devpkey.h>
#include <cfgmgr32.h>
#include <propsys.h>
#include <cwctype>
#include <algorithm>
#include <stdexcept>
namespace gate {
namespace {
std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
    return s;
}
std::wstring prop(IPropertyStore* store, REFPROPERTYKEY key) {
    PROPVARIANT v{};
    std::wstring value;
    if (SUCCEEDED(store->GetValue(key, &v)) && v.vt == VT_LPWSTR && v.pwszVal) value = v.pwszVal;
    PropVariantClear(&v);
    return value;
}
std::wstring ancestry(std::wstring instance) {
    DEVINST node{};
    std::wstring result = instance;
    if (CM_Locate_DevNodeW(&node, instance.data(), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) return result;
    for (int depth = 0; depth < 5; ++depth) {
        DEVINST parent{};
        if (CM_Get_Parent(&parent, node, 0) != CR_SUCCESS) break;
        wchar_t id[MAX_DEVICE_ID_LEN]{};
        if (CM_Get_Device_IDW(parent, id, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) break;
        result += L"|"; result += id; node = parent;
    }
    return result;
}
}
void check(HRESULT hr) { if (FAILED(hr)) throw hr; }
std::wstring errorMessage(HRESULT hr) {
    wchar_t* raw{};
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, DWORD(hr), 0, reinterpret_cast<wchar_t*>(&raw), 0, nullptr);
    wchar_t code[24]{}; swprintf_s(code, L" (0x%08X)", unsigned(hr));
    std::wstring result = raw ? raw : L"Audio device could not be opened";
    if (raw) LocalFree(raw);
    while (!result.empty() && iswspace(result.back())) result.pop_back();
    return result + code;
}
bool isCableIdentity(const std::wstring& instance, const std::wstring& interfaceName) {
    const auto id = lower(instance), iface = lower(interfaceName);
    // Interface identity is driver supplied and is independent of endpoint renaming.
    return id.find(L"vb_audio_cable") != std::wstring::npos || id.find(L"vbaudio_cable") != std::wstring::npos
        || iface.find(L"vb-audio virtual cable") != std::wstring::npos;
}
bool isVirtualIdentity(const std::wstring& instance, const std::wstring& interfaceName) {
    const auto id = lower(instance + L" " + interfaceName);
    return isCableIdentity(instance, interfaceName) || id.find(L"voicemeeter") != std::wstring::npos
        || id.find(L"virtual audio") != std::wstring::npos || id.find(L"virtual cable") != std::wstring::npos
        || id.find(L"sonar") != std::wstring::npos || id.find(L"vb-audio") != std::wstring::npos
        || id.find(L"fxsound") != std::wstring::npos || id.find(L"dfx") != std::wstring::npos;
}
ComPtr<IMMDevice> openDevice(const std::wstring& id) {
    ComPtr<IMMDeviceEnumerator> e; check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, IID_PPV_ARGS(&e)));
    ComPtr<IMMDevice> d; check(e->GetDevice(id.c_str(), &d)); return d;
}
Devices enumerateDevices() {
    Devices result;
    ComPtr<IMMDeviceEnumerator> e; check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&e)));
    for (auto flow : {eCapture, eRender}) {
        ComPtr<IMMDevice> def;
        if (SUCCEEDED(e->GetDefaultAudioEndpoint(flow, eCommunications, &def))) {
            LPWSTR id{}; if (SUCCEEDED(def->GetId(&id))) {
                (flow == eCapture ? result.defaultMicrophone : result.defaultListener) = id; CoTaskMemFree(id);
            }
        }
        ComPtr<IMMDeviceCollection> collection;
        check(e->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection));
        UINT count{}; check(collection->GetCount(&count));
        for (UINT i = 0; i < count; ++i) {
            ComPtr<IMMDevice> raw; if (FAILED(collection->Item(i, &raw))) continue;
            ComPtr<IPropertyStore> store; if (FAILED(raw->OpenPropertyStore(STGM_READ, &store))) continue;
            LPWSTR id{}; if (FAILED(raw->GetId(&id))) continue;
            Device d; d.id = id; CoTaskMemFree(id);
            d.name = prop(store.Get(), PKEY_Device_FriendlyName);
            d.interfaceName = prop(store.Get(), PKEY_DeviceInterface_FriendlyName);
            d.instanceId = ancestry(prop(store.Get(), PKEY_Device_InstanceId));
            d.capture = flow == eCapture;
            d.cable = isCableIdentity(d.instanceId, d.interfaceName);
            d.virtualRoute = isVirtualIdentity(d.instanceId, d.interfaceName);
            PROPVARIANT form{};
            if (SUCCEEDED(store->GetValue(PKEY_AudioEndpoint_FormFactor, &form)) && form.vt == VT_UI4)
                d.speakers = form.ulVal == Speakers;
            PropVariantClear(&form);
            if (d.cable && !d.capture) result.cables.push_back(d);
            else if (!d.virtualRoute) (d.capture ? result.microphones : result.listeners).push_back(d);
        }
    }
    return result;
}
}
