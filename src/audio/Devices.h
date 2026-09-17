#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <string>
#include <vector>
namespace gate {
using Microsoft::WRL::ComPtr;
struct Device {
    std::wstring id, name, instanceId, interfaceName;
    bool capture = false;
    bool virtualRoute = false;
    bool cable = false;
    bool speakers = false;
    bool operator==(const Device&) const = default;
};
struct Devices {
    std::vector<Device> microphones, listeners, cables;
    std::wstring defaultMicrophone, defaultListener;
    bool operator==(const Devices&) const = default;
};
bool isCableIdentity(const std::wstring& instanceId, const std::wstring& interfaceName);
bool isVirtualIdentity(const std::wstring& instanceId, const std::wstring& interfaceName);
Devices enumerateDevices();
ComPtr<IMMDevice> openDevice(const std::wstring& id);
std::wstring errorMessage(HRESULT hr);
void check(HRESULT result);
}
