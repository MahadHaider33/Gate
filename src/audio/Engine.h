#pragma once
#include "audio/Devices.h"
#include "audio/Voice.h"
#include "audio/Processing.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
namespace gate {
inline constexpr UINT audioChanged = WM_APP + 20;
struct Diagnostics {
    std::atomic<uint64_t> blocks{0}, discontinuities{0}, underruns{0}, overflows{0}, overruns{0}, recoveries{0};
    std::atomic<uint64_t> maxProcessingUs{0};
    std::atomic<float> inputLevel{0.f};
};
struct EngineStatus {
    Devices devices;
    std::wstring playingClip, soundMessage;
    bool soundsActive=false;
    std::wstring microphoneId, listeningId;
    std::wstring routeMessage = L"Looking for audio devices...";
    std::wstring testMessage;
    bool capturing = false, cableActive = false, testActive = false, paused = false;
    bool operator==(const EngineStatus&) const = default;
};
class Engine {
public:
    Engine();
    ~Engine();
    void start(HWND window, std::wstring microphone, std::wstring listener, Parameters parameters);
    void stop();
    void selectMicrophone(std::wstring id);
    void selectListener(std::wstring id);
    void setTest(bool enabled);
    void setPaused(bool paused);
    void setSuspended(bool suspended);
    void setParameters(Parameters parameters) noexcept;
    void setVoice(VoiceParameters parameters) noexcept;
    void setHearSounds(bool hearSounds) noexcept;
    void setSoundboardVisible(bool visible) noexcept;
    void playClip(std::wstring id,std::wstring path);
    void stopClips();
    EngineStatus status() const;
    Diagnostics& diagnostics() noexcept { return diagnostics_; }
private:
    struct Impl;
    Diagnostics diagnostics_;
    std::unique_ptr<Impl> impl_;
};
}
