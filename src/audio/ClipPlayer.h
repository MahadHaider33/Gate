#pragma once
#include "audio/SpscQueue.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
namespace gate {
// One decoding producer and one mixer consumer. Control owns start/stop.
class ClipPlayer {
public:
    ~ClipPlayer();
    void start(const std::wstring& path,bool held=false);
    void release() noexcept {held_=false;}
    void stop();
    void read(float* output,unsigned count) noexcept;
    bool playing() const noexcept;
    std::wstring error() const;
private:
    void decode(std::wstring path) noexcept;
    bool decodePcmWave(const std::wstring& path);
    bool write(const float* data,unsigned count);
    SpscQueue<48000> queue_;
    std::atomic<bool> enabled_{false},cancel_{false},done_{true},held_{false};
    std::atomic<unsigned> readers_{0};
    std::thread worker_;
    mutable std::mutex mutex_;
    std::wstring error_;
};
}
