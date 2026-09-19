#include "audio/Engine.h"
#include "audio/Resampler.h"
#include "audio/SpscQueue.h"
#include "audio/ClipPlayer.h"
#include <audioclient.h>
#include <avrt.h>
#include <ksmedia.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <chrono>

namespace gate {
namespace {
struct Handle {
    HANDLE value = nullptr;
    Handle() = default;
    explicit Handle(bool manual) : value(CreateEventW(nullptr, manual, FALSE, nullptr)) { if (!value) check(HRESULT_FROM_WIN32(GetLastError())); }
    ~Handle() { if (value) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct Apartment {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~Apartment() { if (SUCCEEDED(result)) CoUninitialize(); }
};
struct Priority {
    DWORD index = 0;
    HANDLE handle = AvSetMmThreadCharacteristicsW(L"Audio", &index);
    ~Priority() { if (handle) AvRevertMmThreadCharacteristics(handle); }
};
struct Format {
    std::vector<BYTE> bytes;
    unsigned channels{}, rate{}, bits{}, align{};
    bool floating{};
    WAVEFORMATEX* wave() { return reinterpret_cast<WAVEFORMATEX*>(bytes.data()); }
    void load(IAudioClient* client) {
        WAVEFORMATEX* w{}; check(client->GetMixFormat(&w));
        bytes.assign(reinterpret_cast<BYTE*>(w), reinterpret_cast<BYTE*>(w) + sizeof(WAVEFORMATEX) + w->cbSize);
        CoTaskMemFree(w); w = wave();
        channels = w->nChannels; rate = w->nSamplesPerSec; bits = w->wBitsPerSample; align = w->nBlockAlign;
        auto tag = w->wFormatTag;
        if (tag == WAVE_FORMAT_EXTENSIBLE && w->cbSize >= 22) {
            const auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(w);
            if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) tag = WAVE_FORMAT_IEEE_FLOAT;
            else if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) tag = WAVE_FORMAT_PCM;
        }
        floating = tag == WAVE_FORMAT_IEEE_FLOAT;
        if (rate < 8000 || rate > 192000 || channels < 1 || channels > 8
            || (floating ? bits != 32 : (tag != WAVE_FORMAT_PCM || (bits != 16 && bits != 24 && bits != 32)))
            || align != channels * bits / 8) check(AUDCLNT_E_UNSUPPORTED_FORMAT);
    }
    float read(const BYTE* p) const noexcept {
        if (floating) { float v; memcpy(&v, p, 4); return std::isfinite(v) ? std::clamp(v, -1.f, 1.f) : 0.f; }
        if (bits == 16) { int16_t v; memcpy(&v, p, 2); return v / 32768.f; }
        if (bits == 24) { int32_t v = (int32_t(p[2]) << 24) | (int32_t(p[1]) << 16) | (int32_t(p[0]) << 8); return float(v / 2147483648.0); }
        int32_t v; memcpy(&v, p, 4); return float(v / 2147483648.0);
    }
    void write(BYTE* p, float sample) const noexcept {
        sample = std::clamp(sample, -1.f, 1.f);
        if (floating) { memcpy(p, &sample, 4); return; }
        if (bits == 16) { auto v = int16_t(std::clamp(double(sample)*32768., -32768., 32767.)); memcpy(p, &v, 2); }
        else if (bits == 24) { auto v = int32_t(std::clamp(double(sample)*8388608., -8388608., 8388607.)); p[0]=BYTE(v); p[1]=BYTE(v>>8); p[2]=BYTE(v>>16); }
        else { auto v = int32_t(std::clamp(double(sample)*2147483648., -2147483648., 2147483647.)); memcpy(p, &v, 4); }
    }
};
struct Stream {
    ComPtr<IAudioClient> client;
    ComPtr<IAudioClock> clock;
    Format format;
    Handle ready{false};
    UINT32 bufferSize{};
    void open(const std::wstring& id, bool capture) {
        auto device = openDevice(id);
        check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client));
        format.load(client.Get());
        ComPtr<IAudioClient2> c2;
        if (capture && SUCCEEDED(client.As(&c2))) {
            AudioClientProperties properties{}; properties.cbSize = sizeof(properties);
            properties.eCategory = AudioCategory_Other;
            properties.Options = AUDCLNT_STREAMOPTIONS_RAW;
            if (FAILED(c2->SetClientProperties(&properties))) {
                properties.Options = AUDCLNT_STREAMOPTIONS_NONE;
                check(c2->SetClientProperties(&properties));
            }
        }
        DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        if (!capture) flags |= AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_NOPERSIST;
        HRESULT initialized = E_FAIL;
        ComPtr<IAudioClient3> c3;
        // RATEADJUST render streams deliberately use the conventional shared-mode path.
        if (capture && SUCCEEDED(client.As(&c3))) {
            UINT32 def{}, fundamental{}, min{}, max{};
            if (SUCCEEDED(c3->GetSharedModeEnginePeriod(format.wave(), &def, &fundamental, &min, &max)) && fundamental) {
                auto period = std::clamp((format.rate / 100 + fundamental - 1) / fundamental * fundamental, min, max);
                initialized = c3->InitializeSharedAudioStream(flags, period, format.wave(), nullptr);
            }
        }
        if (FAILED(initialized)) {
            // A failed initialization may leave an IAudioClient unusable. Always reactivate.
            c3.Reset(); c2.Reset(); client.Reset();
            check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client));
            check(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 200000, 0, format.wave(), nullptr));
        }
        check(client->SetEventHandle(ready.value));
        check(client->GetBufferSize(&bufferSize));
        if (!bufferSize || bufferSize > format.rate) check(AUDCLNT_E_BUFFER_SIZE_ERROR);
        check(client->GetService(IID_PPV_ARGS(&clock)));
    }
};

class Output {
public:
    Output(Diagnostics& stats, HANDLE wake) : stats_(stats), wake_(wake) {}
    ~Output() { stop(); }
    void start(const std::wstring& id) {
        stop();
        stream_ = std::make_unique<Stream>(); stream_->open(id, false);
        check(stream_->client->GetService(IID_PPV_ARGS(&render_)));
        check(stream_->client->GetService(IID_PPV_ARGS(&adjust_)));
        resampler_.prepare(sampleRate, stream_->format.rate);
        mono_.resize(stream_->bufferSize);
        UINT64 position{}, stamp{};
        stream_->clock->GetPosition(&position, &stamp);
        lastClock_ = position; lastStamp_ = stamp;
        queue_.reset(); pending_ = offset_ = 0; primed_ = false; failed = false;
        ResetEvent(stop_.value);
        enabled_.store(true, std::memory_order_release);
        worker_ = std::thread([this] { run(); });
    }
    void stop() {
        enabled_.store(false, std::memory_order_release);
        for (auto n = producers_.load(); n; n = producers_.load()) producers_.wait(n);
        SetEvent(stop_.value);
        if (worker_.joinable()) worker_.join();
        if (stream_) stream_->client->Stop();
        render_.Reset(); adjust_.Reset(); stream_.reset(); queue_.reset();
        resampler_.release(); pending_=offset_=0;
        std::fill(mono_.begin(), mono_.end(), 0.f); input_.fill(0.f);
        failed=false;
    }
    void push(const float* samples) noexcept {
        if (!enabled_.load(std::memory_order_acquire)) return;
        producers_.fetch_add(1);
        if (enabled_.load(std::memory_order_acquire)) {
            if (queue_.push({samples, frameSize}) != frameSize) { ++stats_.overflows; failed = true; SetEvent(wake_); }
        }
        producers_.fetch_sub(1); producers_.notify_all();
    }
    bool active() const noexcept { return enabled_.load() && !failed.load(); }
    void adjustClock() {
        if (!active() || !stream_) return;
        UINT64 position{}, stamp{};
        if (FAILED(stream_->clock->GetPosition(&position, &stamp))) { failed=true; SetEvent(wake_); return; }
        // Small queue-fill correction drives the OS's existing resampler; never implement a resampler here.
        if (stamp > lastStamp_ && position >= lastClock_) {
            const double error = double(queue_.size()) - frameSize;
            const double correction = std::clamp(error / sampleRate * 0.02, -0.002, 0.002);
            if (FAILED(adjust_->SetSampleRate(float(stream_->format.rate * (1.0 + correction))))) { failed=true; SetEvent(wake_); }
        }
        lastClock_ = position; lastStamp_ = stamp;
    }
    std::atomic<bool> failed{false};
private:
    void run() noexcept {
        Apartment apartment; Priority priority;
        try {
            // Prefill before Start, as required for glitch-free shared-mode startup.
            BYTE* initial{}; check(render_->GetBuffer(stream_->bufferSize,&initial));
            check(render_->ReleaseBuffer(stream_->bufferSize,AUDCLNT_BUFFERFLAGS_SILENT));
            check(stream_->client->Start());
            HANDLE events[] = {stop_.value, stream_->ready.value};
            while (true) {
                const auto wait = WaitForMultipleObjects(2, events, FALSE, 2000);
                if (wait == WAIT_OBJECT_0) break;
                if (wait != WAIT_OBJECT_0 + 1) check(AUDCLNT_E_DEVICE_INVALIDATED);
                UINT32 padding{}; check(stream_->client->GetCurrentPadding(&padding));
                const UINT32 count = stream_->bufferSize - padding;
                if (!count) continue;
                if (queue_.size() > frameSize * 4) { ++stats_.overflows; check(AUDCLNT_E_BUFFER_ERROR); }
                BYTE* dest{}; check(render_->GetBuffer(count, &dest));
                size_t produced = 0;
                if (!primed_ && queue_.size() >= frameSize*2) primed_ = true;
                if (primed_) {
                    while (produced < count) {
                        if (!pending_) { pending_ = uint32_t(queue_.pop(input_)); offset_ = 0; }
                        if (!pending_) break;
                        uint32_t in = pending_, out = count - uint32_t(produced);
                        if (!resampler_.process(input_.data() + offset_, in, mono_.data() + produced, out)) break;
                        offset_ += in; pending_ -= in; produced += out;
                        if (!in && !out) break;
                    }
                }
                if (produced < count && primed_) ++stats_.underruns;
                std::fill(mono_.begin() + produced, mono_.begin() + count, 0.f);
                for (UINT32 f = 0; f < count; ++f)
                    for (unsigned ch = 0; ch < stream_->format.channels; ++ch)
                        stream_->format.write(dest + f * stream_->format.align + ch * stream_->format.bits/8, mono_[f]);
                check(render_->ReleaseBuffer(count, produced ? 0 : AUDCLNT_BUFFERFLAGS_SILENT));
            }
        } catch (...) { failed = true; SetEvent(wake_); }
        stream_->client->Stop();
    }
    Diagnostics& stats_;
    HANDLE wake_;
    Handle stop_{true};
    std::unique_ptr<Stream> stream_;
    ComPtr<IAudioRenderClient> render_;
    ComPtr<IAudioClockAdjustment> adjust_;
    Resampler resampler_;
    SpscQueue<frameSize * 6> queue_;
    std::array<float, frameSize> input_{};
    std::vector<float> mono_;
    std::thread worker_;
    std::atomic<bool> enabled_{false};
    std::atomic<unsigned> producers_{0};
    uint32_t pending_{}, offset_{};
    bool primed_{};
    UINT64 lastClock_{}, lastStamp_{};
};

class Notification final : public IMMNotificationClient {
public:
    Notification(HANDLE wake, std::atomic<bool>& changed) : wake_(wake), changed_(changed) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) { *out = static_cast<IMMNotificationClient*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const auto n=--refs_; if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD) override { return signal(); }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return signal(); }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return signal(); }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow,ERole,LPCWSTR) override { return signal(); }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY) override { return signal(); }
private:
    HRESULT signal() { changed_ = true; SetEvent(wake_); return S_OK; }
    std::atomic<ULONG> refs_{1}; HANDLE wake_; std::atomic<bool>& changed_;
};
const Device* find(const std::vector<Device>& list, const std::wstring& id) {
    const auto it = std::find_if(list.begin(), list.end(), [&](const Device& d) { return d.id == id; });
    return it == list.end() ? nullptr : &*it;
}
}

struct Engine::Impl {
    explicit Impl(Diagnostics& stats) : stats(stats), cable(stats,wake.value), listener(stats,wake.value) {}
    Diagnostics& stats;
    HWND window{};
    Handle wake{false}, quit{true}, captureStop{true}, mixerStop{true};
    SpscQueue<frameSize*12> microphoneQueue;
    ClipPlayer clips;
    AppAudio media;
    AudioApp requestedApp;
    uint64_t mediaRevision=0,appliedMediaRevision=0;
    bool mediaRequested=false;
    std::atomic<uint32_t> mixLevels{100u|(80u<<8)};
    std::thread mixerWorker;
    VoiceMailbox voiceParameters;
    std::atomic<bool> hearSounds{true},hearVoice{false};
    std::atomic<bool> soundboardVisible{false};
    std::wstring requestedClip,requestedPath,activeClip;
    uint64_t clipRevision=0,appliedClipRevision=0;
    ULONGLONG soundDrainUntil=0;
    std::atomic<bool> devicesChanged{true}, captureFailed{false};
    std::atomic<uint32_t> parameters{pack({})};
    mutable std::mutex mutex;
    EngineStatus published;
    std::wstring requestedMic, requestedListener;
    bool wantTest=false, paused=false, suspended=false;
    std::thread controller, captureWorker;
    Output cable, listener;
    std::unique_ptr<Stream> capture;
    ComPtr<IAudioCaptureClient> captureClient;
    Resampler inputResampler;
    std::unique_ptr<Processor> processor;
    std::wstring activeMic, activeCable, activeListener;
    bool firstSelection=true;

    void notify(const EngineStatus& state) {
        { std::lock_guard lock(mutex); published = state; }
        if (window) PostMessageW(window, audioChanged, 0, 0);
    }
    void stopMixer() {
        SetEvent(mixerStop.value);
        if(mixerWorker.joinable())mixerWorker.join();
        stats.voiceOutputLevel=0.f;stats.mediaOutputLevel=0.f;
    }
    void startMixer() {
        if(mixerWorker.joinable())return;
        ResetEvent(mixerStop.value);
        // Allocate and configure DSP before entering the real-time loop.
        auto effect=std::make_unique<VoiceEffect>();
        mixerWorker=std::thread([this,effect=std::move(effect)]() mutable {
            Apartment apartment;Priority priority;
            Handle timer;timer.value=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
            if(!timer.value){timer.value=CreateWaitableTimerW(nullptr,FALSE,nullptr);}
            if(!timer.value)return;
            std::array<float,frameSize> mic{},voice{},sound{},app{},toCable{},toListener{};
            VoiceParameters voiceSettings;
            LARGE_INTEGER frequency{},now{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&now);
            double next=double(now.QuadPart),period=double(frequency.QuadPart)/100.;
            bool primed=false;unsigned blocks=0;
            HANDLE events[]={mixerStop.value,timer.value};
            while(WaitForSingleObject(mixerStop.value,0)!=WAIT_OBJECT_0){
                mic.fill(0);
                if(!primed&&microphoneQueue.size()>=frameSize*2)primed=true;
                if(primed){
                    if(microphoneQueue.size()>frameSize*8){while(microphoneQueue.size()>frameSize*2)microphoneQueue.pop(mic);++stats.overflows;}
                    mic.fill(0);
                    if(microphoneQueue.pop(mic)<frameSize)primed=false;
                }
                voiceParameters.read(voiceSettings);
                effect->process(mic.data(),voice.data(),voiceSettings);
                clips.read(sound.data(),frameSize);
                media.read(app.data());
                const auto levels=mixLevels.load(std::memory_order_relaxed);
                const float voiceGain=(levels&(1u<<16))?0.f:float(levels&255u)/100.f;
                const float appGain=(levels&(1u<<17))?0.f:float((levels>>8)&255u)/100.f;
                mixAudio(voice.data(),sound.data(),toCable.data(),toListener.data(),frameSize,
                    .8f,hearVoice.load(),hearSounds.load(),app.data(),appGain,voiceGain);
                float voicePeak=0.f,appPeak=0.f;
                for(unsigned i=0;i<frameSize;++i){voicePeak=std::max(voicePeak,std::abs(voice[i]*voiceGain));appPeak=std::max(appPeak,std::abs(app[i]*appGain));}
                stats.voiceOutputLevel=std::max(voicePeak,stats.voiceOutputLevel.load(std::memory_order_relaxed)*.88f);
                stats.mediaOutputLevel=std::max(appPeak,stats.mediaOutputLevel.load(std::memory_order_relaxed)*.88f);
                cable.push(toCable.data());listener.push(toListener.data());
                // The mixer follows the input clock slowly. Outputs retain their
                // independent OS clock correction. No samples are resampled here.
                if(++blocks%100==0){
                    const double correction=primed?std::clamp((double(microphoneQueue.size())-frameSize)/sampleRate*.02,-.002,.002):0.;
                    period=double(frequency.QuadPart)/100.*(1.-correction);
                }
                next+=period;QueryPerformanceCounter(&now);
                if(next<now.QuadPart-period*2)next=double(now.QuadPart)+period;
                LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,LONGLONG((next-now.QuadPart)*10000000./frequency.QuadPart));
                if(!SetWaitableTimer(timer.value,&due,0,nullptr,nullptr,FALSE))break;
                if(WaitForMultipleObjects(2,events,FALSE,1000)!=WAIT_OBJECT_0+1)break;
            }
        });
    }
    void stopCapture() {
        SetEvent(captureStop.value);
        if (captureWorker.joinable()) captureWorker.join();
        stats.inputLevel=0.f;
        captureClient.Reset(); capture.reset(); processor.reset(); inputResampler.release(); activeMic.clear();
    }
    void startCapture(const std::wstring& id) {
        stopCapture();
        capture=std::make_unique<Stream>(); capture->open(id,true);
        check(capture->client->GetService(IID_PPV_ARGS(&captureClient)));
        inputResampler.prepare(capture->format.rate,sampleRate);
        processor=std::make_unique<Processor>(); processor->prepare(unpack(parameters.load()));
        activeMic=id; captureFailed=false; ResetEvent(captureStop.value);
        captureWorker=std::thread([this] { captureLoop(); });
    }
    void captureLoop() noexcept {
        Apartment apartment; Priority priority;
        std::array<float,256> mono{};
        std::array<float,2048> converted{};
        std::array<float,frameSize> input{}, output{};
        unsigned assembled=0;
        cycfi::q::peak_envelope_follower meter(cycfi::q::duration(0.15), float(sampleRate));
        LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
        try {
            check(capture->client->Start());
            HANDLE events[] = {captureStop.value,capture->ready.value};
            while (true) {
                const auto wait=WaitForMultipleObjects(2,events,FALSE,2000);
                if (wait==WAIT_OBJECT_0) break;
                if (wait!=WAIT_OBJECT_0+1) check(AUDCLNT_E_DEVICE_INVALIDATED);
                UINT32 next{}; check(captureClient->GetNextPacketSize(&next));
                while (next) {
                    BYTE* data{}; UINT32 frames{}; DWORD flags{}; UINT64 position{}, stamp{};
                    check(captureClient->GetBuffer(&data,&frames,&flags,&position,&stamp));
                    if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) ++stats.discontinuities;
                    for (UINT32 base=0; base<frames;) {
                        const auto n=std::min<UINT32>(256,frames-base);
                        for (UINT32 f=0;f<n;++f) {
                            float value=0;
                            if (!(flags&AUDCLNT_BUFFERFLAGS_SILENT)) {
                                for (unsigned ch=0;ch<capture->format.channels;++ch)
                                    value+=capture->format.read(data+(base+f)*capture->format.align+ch*capture->format.bits/8);
                                value/=capture->format.channels;
                            }
                            mono[f]=value;
                        }
                        uint32_t consumed=0;
                        while(consumed<n) {
                            uint32_t in=n-consumed, out=uint32_t(converted.size());
                            if (!inputResampler.process(mono.data()+consumed,in,converted.data(),out)) check(E_FAIL);
                            consumed+=in;
                            for (uint32_t i=0;i<out;++i) {
                                meter(std::abs(converted[i])); input[assembled++]=converted[i];
                                if (assembled==frameSize) {
                                    LARGE_INTEGER begin{}, end{}; QueryPerformanceCounter(&begin);
                                    processor->process(input.data(),output.data(),unpack(parameters.load(std::memory_order_relaxed)));
                                    QueryPerformanceCounter(&end);
                                    auto elapsed=uint64_t((end.QuadPart-begin.QuadPart)*1000000/frequency.QuadPart);
                                    auto max=stats.maxProcessingUs.load();
                                    while(elapsed>max && !stats.maxProcessingUs.compare_exchange_weak(max,elapsed)) {}
                                    if (elapsed>10000) ++stats.overruns;
                                    ++stats.blocks; stats.inputLevel=meter();
                                    if(microphoneQueue.push(output)!=frameSize)++stats.overflows; assembled=0;
                                }
                            }
                            if (!in && !out) check(E_FAIL);
                        }
                        base+=n;
                    }
                    check(captureClient->ReleaseBuffer(frames));
                    check(captureClient->GetNextPacketSize(&next));
                }
            }
        } catch (...) { captureFailed=true; SetEvent(wake.value); }
        capture->client->Stop();
    }
    void run() noexcept {
        Apartment apartment;
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<Notification> notification;
        EngineStatus state;
        try {
            check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)));
            notification.Attach(new Notification(wake.value, devicesChanged));
            check(enumerator->RegisterEndpointNotificationCallback(notification.Get()));
            HANDLE events[]={quit.value,wake.value};
            while (WaitForSingleObject(quit.value,0)!=WAIT_OBJECT_0) {
                bool deviceEvent=devicesChanged.exchange(false);
                if (deviceEvent) state.devices=enumerateDevices();
                std::wstring mic, listening,clip,path; bool test, stopped;uint64_t revision,appRevision;AudioApp app;
                {
                    std::lock_guard lock(mutex);
                    if (firstSelection) {
                        if (requestedMic.empty() && find(state.devices.microphones,state.devices.defaultMicrophone)) requestedMic=state.devices.defaultMicrophone;
                        if (requestedListener.empty() && find(state.devices.listeners,state.devices.defaultListener)) requestedListener=state.devices.defaultListener;
                        firstSelection=false;
                    }
                    mic=requestedMic; listening=requestedListener; test=wantTest; stopped=paused||suspended;
                    state.paused=stopped;clip=requestedClip;path=requestedPath;revision=clipRevision;
                    app=requestedApp;appRevision=mediaRevision;
                }
                if(stopped||appRevision!=appliedMediaRevision){
                    stopMixer();media.stop();mediaRequested=false;state.mediaName.clear();state.mediaMessage.clear();
                    if(stopped){std::lock_guard lock(mutex);requestedApp={};appliedMediaRevision=mediaRevision;}
                    else {
                        appliedMediaRevision=appRevision;
                        if(app.processId){media.start(app);mediaRequested=true;state.mediaName=app.name;}
                    }
                }
                if(mediaRequested&&media.finished()){
                    state.mediaMessage=media.error();mediaRequested=false;
                    stopMixer();media.stop();
                }
                state.mediaActive=mediaRequested&&media.active();
                state.mediaStarting=mediaRequested&&!state.mediaActive;
                if(stopped){
                    stopMixer();clips.stop();activeClip.clear();cable.stop();listener.stop();activeCable.clear();activeListener.clear();
                    {std::lock_guard lock(mutex);requestedClip.clear();requestedPath.clear();appliedClipRevision=clipRevision;}
                } else if(revision!=appliedClipRevision){
                    clips.stop();activeClip.clear();state.soundMessage.clear();
                    if(!clip.empty()){clips.start(path,true);activeClip=clip;}
                    appliedClipRevision=revision;
                }
                if(!activeClip.empty()&&!clips.playing()){
                    state.soundMessage=clips.error();activeClip.clear();soundDrainUntil=GetTickCount64()+100;
                    std::lock_guard lock(mutex);if(clipRevision==appliedClipRevision){requestedClip.clear();requestedPath.clear();}
                }
                state.playingClip=activeClip;
                state.microphoneId=mic; state.listeningId=listening;
                const bool micExists=find(state.devices.microphones,mic)!=nullptr;
                if (capture && (stopped || activeMic!=mic || !micExists || captureFailed)) {
                    stopMixer();stopCapture();microphoneQueue.reset();
                    { std::lock_guard lock(mutex); wantTest=false; } test=false;
                    if (captureFailed) { ++stats.recoveries; state.routeMessage=L"Microphone interrupted. Re-select it or reconnect to retry."; }
                }
                if (!stopped && micExists && !capture && (!captureFailed || deviceEvent)) {
                    try { startCapture(mic); }
                    catch(HRESULT hr) { stopCapture(); captureFailed=true; state.routeMessage=L"Microphone unavailable: "+errorMessage(hr); }
                    catch(...) { stopCapture(); captureFailed=true; state.routeMessage=L"Could not initialize microphone processing."; }
                }
                if (stopped) state.routeMessage=L"Processing paused";
                else if (!micExists) state.routeMessage=mic.empty()?L"Select a microphone to get started.":L"Selected microphone disconnected. Waiting for that device.";
                if (cable.failed) { cable.stop(); activeCable.clear(); ++stats.recoveries; }
                const auto cableId=state.devices.cables.empty()?std::wstring{}:state.devices.cables.front().id;
                if (!activeCable.empty() && activeCable!=cableId) { cable.stop(); activeCable.clear(); }
                if (!stopped && !cableId.empty() && !cable.active()) {
                    try { cable.start(cableId); activeCable=cableId; }
                    catch(HRESULT hr) { cable.stop(); state.routeMessage=L"Cable unavailable: "+errorMessage(hr); }
                    catch(...) { cable.stop(); state.routeMessage=L"Could not initialize cable output."; }
                }
                if (!stopped && cableId.empty()) state.routeMessage=L"VB-CABLE not found. Install it to use Gate in other apps.";
                else if (cable.active()) state.routeMessage=L"Ready. Select CABLE Output as your microphone in other apps.";
                const auto* outputDevice=find(state.devices.listeners,listening);
                if(!capture||captureFailed){test=false;std::lock_guard lock(mutex);wantTest=false;}
                const bool soundListening=!stopped&&hearSounds.load()&&(clips.playing()||GetTickCount64()<soundDrainUntil);
                // Prepare the route when the soundboard opens, rather than
                // making the first click wait for endpoint initialization.
                const bool warmListener=hearSounds.load()&&soundboardVisible.load();
                const bool needListener=!stopped&&(test||soundListening||warmListener);
                if(listener.failed||(listener.active()&&(!needListener||activeListener!=listening||!outputDevice))){
                    const bool failed=listener.failed.load();listener.stop();activeListener.clear();
                    if(failed||!outputDevice){test=false;std::lock_guard lock(mutex);wantTest=false;state.testMessage=L"Listening device unavailable. Check the device on the Microphone page.";}
                }
                if(needListener&&!listener.active()&&outputDevice&&!outputDevice->virtualRoute&&listening!=cableId){
                    try{listener.start(listening);activeListener=listening;state.testMessage.clear();}
                    catch(...){listener.stop();state.testMessage=L"Could not open the listening device.";test=false;std::lock_guard lock(mutex);wantTest=false;}
                }
                if(test&&!listener.active()){test=false;std::lock_guard lock(mutex);wantTest=false;state.testMessage=L"Select an available listening device on the Microphone page.";}
                if(soundListening&&!listener.active()&&!activeClip.empty())state.soundMessage=state.testMessage.empty()?L"Select a listening device on the Microphone page to hear sounds.":state.testMessage;
                clips.release();
                hearVoice=test&&listener.active();
                state.capturing=capture&&!captureFailed;state.cableActive=cable.active();state.testActive=hearVoice.load();
                state.soundsActive=soundListening&&listener.active();
                if(!stopped&&(state.capturing||cable.active()||listener.active()||clips.playing()))startMixer();
                else stopMixer();
                notify(state);
                DWORD wait;
                do {
                    wait=WaitForMultipleObjects(2,events,FALSE,stopped?INFINITE:250);
                    if(wait==WAIT_TIMEOUT) { cable.adjustClock(); listener.adjustClock(); }
                } while(false);
                if(wait==WAIT_OBJECT_0) break;
            }
        } catch(HRESULT hr) { state.routeMessage=L"Audio initialization failed: "+errorMessage(hr); notify(state); }
          catch(...) { state.routeMessage=L"Audio initialization failed."; notify(state); }
        if(enumerator && notification) enumerator->UnregisterEndpointNotificationCallback(notification.Get());
        stopMixer();media.stop();clips.stop();listener.stop(); cable.stop(); stopCapture();
    }
};
Engine::Engine():impl_(std::make_unique<Impl>(diagnostics_)) {}
Engine::~Engine() { stop(); }
void Engine::start(HWND window,std::wstring mic,std::wstring listener,Parameters p) {
    impl_->window=window; impl_->requestedMic=std::move(mic); impl_->requestedListener=std::move(listener);
    impl_->parameters=pack(p); impl_->controller=std::thread([this]{impl_->run();});
}
void Engine::stop() { if(!impl_)return; SetEvent(impl_->quit.value); if(impl_->controller.joinable())impl_->controller.join(); }
void Engine::selectMicrophone(std::wstring id) {
    { std::lock_guard lock(impl_->mutex); impl_->requestedMic=std::move(id); impl_->wantTest=false; impl_->captureFailed=false; }
    SetEvent(impl_->wake.value);
}
void Engine::selectListener(std::wstring id) {
    { std::lock_guard lock(impl_->mutex); impl_->requestedListener=std::move(id); impl_->wantTest=false; }
    SetEvent(impl_->wake.value);
}
void Engine::setTest(bool enabled) { {std::lock_guard lock(impl_->mutex);impl_->wantTest=enabled;} SetEvent(impl_->wake.value); }
void Engine::setPaused(bool paused) { {std::lock_guard lock(impl_->mutex);impl_->paused=paused;if(paused){impl_->wantTest=false;impl_->requestedApp={};++impl_->mediaRevision;}} SetEvent(impl_->wake.value); }
void Engine::setSuspended(bool suspended) { {std::lock_guard lock(impl_->mutex);impl_->suspended=suspended;if(suspended){impl_->wantTest=false;impl_->requestedApp={};++impl_->mediaRevision;}} SetEvent(impl_->wake.value); }
void Engine::setParameters(Parameters p) noexcept { impl_->parameters=pack(p); }
void Engine::setVoice(VoiceParameters p) noexcept {impl_->voiceParameters.store(p);}
void Engine::setHearSounds(bool hear) noexcept {impl_->hearSounds=hear;SetEvent(impl_->wake.value);}
void Engine::setSoundboardVisible(bool visible) noexcept {impl_->soundboardVisible=visible;SetEvent(impl_->wake.value);}
void Engine::playClip(std::wstring id,std::wstring path){
    {std::lock_guard lock(impl_->mutex);if(impl_->paused||impl_->suspended)return;
        if(impl_->requestedClip==id){impl_->requestedClip.clear();impl_->requestedPath.clear();}
        else{impl_->requestedClip=std::move(id);impl_->requestedPath=std::move(path);}
        ++impl_->clipRevision;}
    SetEvent(impl_->wake.value);
}
void Engine::stopClips(){
    {std::lock_guard lock(impl_->mutex);impl_->requestedClip.clear();impl_->requestedPath.clear();++impl_->clipRevision;}
    SetEvent(impl_->wake.value);
}
EngineStatus Engine::status() const {std::lock_guard lock(impl_->mutex);return impl_->published;}
void Engine::shareApp(AudioApp app){
    {std::lock_guard lock(impl_->mutex);if(impl_->paused||impl_->suspended)return;impl_->requestedApp=std::move(app);++impl_->mediaRevision;}
    SetEvent(impl_->wake.value);
}
void Engine::stopSharing(){
    {std::lock_guard lock(impl_->mutex);impl_->requestedApp={};++impl_->mediaRevision;}
    SetEvent(impl_->wake.value);
}
void Engine::setMediaVolume(unsigned percent) noexcept {
    auto previous=impl_->mixLevels.load();
    while(!impl_->mixLevels.compare_exchange_weak(previous,(previous&~(255u<<8))|(std::min(percent,100u)<<8))){}
}
void Engine::setMixLevels(unsigned microphone,unsigned media,bool muteMicrophone,bool muteMedia) noexcept {
    impl_->mixLevels=std::min(microphone,100u)|(std::min(media,100u)<<8)|(uint32_t(muteMicrophone)<<16)|(uint32_t(muteMedia)<<17);
}
#ifdef GATE_DEVELOPER_PROBES
// Developer verification uses the same output implementation with digital silence.
// No microphone is opened and no user audio is captured by this probe.
void verifySilentOutput(const std::wstring& id, Diagnostics& stats) {
    const auto devices=enumerateDevices();
    if(!find(devices.listeners,id)) check(E_INVALIDARG);
    Handle wake(false); Output output(stats,wake.value); Priority priority;
    Handle timer;
    timer.value=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
    if(!timer.value)check(HRESULT_FROM_WIN32(GetLastError()));
    std::array<float,frameSize> silence{};
    for(int cycle=0;cycle<3;++cycle){
        output.start(id);
        LARGE_INTEGER due{};due.QuadPart=-100000;
        if(!SetWaitableTimer(timer.value,&due,10,nullptr,nullptr,FALSE))check(HRESULT_FROM_WIN32(GetLastError()));
        for(int block=0;block<200;++block){
            output.push(silence.data());
            if(block%100==99) output.adjustClock();
            if(output.failed) {output.stop();check(E_FAIL);}
            if(WaitForSingleObject(timer.value,1000)!=WAIT_OBJECT_0)check(E_FAIL);
        }
        output.stop();
        CancelWaitableTimer(timer.value);
        if(output.active()) check(E_FAIL);
    }
}
#endif
}
