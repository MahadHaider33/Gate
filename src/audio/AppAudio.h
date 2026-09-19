#pragma once
#include "audio/Devices.h"
#include "audio/Processing.h"
#include "audio/SpscQueue.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace gate {
struct AudioApp {
    DWORD processId=0;
    uint64_t created=0;
    std::wstring name;
};
bool appAudioSupported() noexcept;
std::vector<AudioApp> enumerateAudioApps();

// Controller owns start/stop. Stop the mixer before resetting this transport.
class AppAudio {
public:
    AppAudio();
    ~AppAudio();
    void start(AudioApp app);
    void stop();
    void read(float* samples) noexcept;
    bool active() const noexcept {return active_.load();}
    bool finished() const noexcept {return finished_.load();}
    std::wstring error() const;
private:
    void run(AudioApp app) noexcept;
    HANDLE stop_{};
    std::thread worker_;
    SpscQueue<frameSize*12> queue_;
    std::atomic<bool> active_{false},finished_{true};
    mutable std::mutex mutex_;
    std::wstring error_;
};
}
