#include "audio/AppAudio.h"
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <wrl/implements.h>
#include <winternl.h>
#include <avrt.h>
#include <tlhelp32.h>
#include <algorithm>
#include <array>
#include <cstring>

namespace gate {
namespace {
struct Handle {
    HANDLE value{};
    ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
};
struct Apartment {
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~Apartment(){if(SUCCEEDED(result))CoUninitialize();}
};
uint64_t creationTime(HANDLE process) {
    FILETIME created{},exit{},kernel{},user{};
    if(!GetProcessTimes(process,&created,&exit,&kernel,&user))return 0;
    return (uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
}
std::vector<DWORD> ownProcessTree() {
    std::vector<DWORD> ancestors{GetCurrentProcessId()};
    Handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)};
    if(snapshot.value==INVALID_HANDLE_VALUE)check(HRESULT_FROM_WIN32(GetLastError()));
    std::vector<PROCESSENTRY32W> processes;PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot.value,&entry))do{processes.push_back(entry);}while(Process32NextW(snapshot.value,&entry));
    auto created=creationTime(GetCurrentProcess());
    for(;;){
        const auto it=std::find_if(processes.begin(),processes.end(),[&](const auto& p){return p.th32ProcessID==ancestors.back();});
        if(it==processes.end()||!it->th32ParentProcessID||std::find(ancestors.begin(),ancestors.end(),it->th32ParentProcessID)!=ancestors.end())break;
        Handle parent{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,it->th32ParentProcessID)};
        const auto parentCreated=parent.value?creationTime(parent.value):0;
        // A reused parent PID is not part of this tree.
        if(!parentCreated||parentCreated>created)break;
        ancestors.push_back(it->th32ParentProcessID);created=parentCreated;
    }
    return ancestors;
}
// Callback owns its state, including the event, if cancellation wins activation.
class Activation final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IActivateAudioInterfaceCompletionHandler,Microsoft::WRL::FtmBase> {
public:
    Handle done{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
    AUDIOCLIENT_ACTIVATION_PARAMS parameters{};
    HRESULT result=E_PENDING;
    ComPtr<IAudioClient> client;
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override {
        HRESULT activated=E_FAIL;ComPtr<IUnknown> unknown;
        result=operation->GetActivateResult(&activated,&unknown);
        if(SUCCEEDED(result))result=activated;
        if(SUCCEEDED(result))result=unknown.As(&client);
        SetEvent(done.value);return S_OK;
    }
};
}
bool appAudioSupported() noexcept {
    using Version=LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto module=GetModuleHandleW(L"ntdll.dll");
    const auto version=reinterpret_cast<Version>(GetProcAddress(module,"RtlGetVersion"));
    RTL_OSVERSIONINFOW info{};info.dwOSVersionInfoSize=sizeof(info);
    return version&&version(&info)==0&&info.dwBuildNumber>=20348;
}
std::vector<AudioApp> enumerateAudioApps() {
    std::vector<AudioApp> apps;
    const auto ancestors=ownProcessTree();
    struct Context {std::vector<AudioApp>& apps;const std::vector<DWORD>& ancestors;} context{apps,ancestors};
    EnumWindows([](HWND window,LPARAM context)->BOOL {
        if(window==GetShellWindow()||!IsWindowVisible(window)||GetWindowTextLengthW(window)==0)return TRUE;
        DWORD pid{};GetWindowThreadProcessId(window,&pid);
        auto& ctx=*reinterpret_cast<Context*>(context);auto& list=ctx.apps;
        if(!pid||std::find(ctx.ancestors.begin(),ctx.ancestors.end(),pid)!=ctx.ancestors.end()||std::any_of(list.begin(),list.end(),[&](const auto& a){return a.processId==pid;}))return TRUE;
        Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid)};
        if(!process.value)return TRUE;
        const auto created=creationTime(process.value);if(!created)return TRUE;
        wchar_t path[32768]{};DWORD size=DWORD(std::size(path));
        if(!QueryFullProcessImageNameW(process.value,0,path,&size))return TRUE;
        const auto slash=wcsrchr(path,L'\\');
        wchar_t title[512]{};GetWindowTextW(window,title,int(std::size(title)));
        try{list.push_back({pid,created,std::wstring(slash?slash+1:path)+L" — "+title});}catch(...){return FALSE;}
        return TRUE;
    },reinterpret_cast<LPARAM>(&context));
    std::sort(apps.begin(),apps.end(),[](const auto& a,const auto& b){return a.name<b.name;});
    return apps;
}
AppAudio::AppAudio():stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)){if(!stop_)check(HRESULT_FROM_WIN32(GetLastError()));}
AppAudio::~AppAudio(){stop();CloseHandle(stop_);}
void AppAudio::start(AudioApp app){
    stop();{std::lock_guard lock(mutex_);error_.clear();}
    ResetEvent(stop_);finished_=false;
    worker_=std::thread([this,app=std::move(app)]{run(app);});
}
void AppAudio::stop(){SetEvent(stop_);if(worker_.joinable())worker_.join();active_=false;finished_=true;queue_.reset();}
std::wstring AppAudio::error() const {std::lock_guard lock(mutex_);return error_;}
void AppAudio::read(float* samples) noexcept {
    std::span<float> block(samples,frameSize);std::fill(block.begin(),block.end(),0.f);
    if(!active_)return;
    // Discard old transport data after a stall rather than replaying it late.
    while(queue_.size()>frameSize*6)queue_.pop(block);
    std::fill(block.begin(),block.end(),0.f);queue_.pop(block);
}
void AppAudio::run(AudioApp app) noexcept {
    Apartment apartment;
    Handle ready;
    ComPtr<IAudioClient> client;
    DWORD task{};const auto priority=AvSetMmThreadCharacteristicsW(L"Audio",&task);
    try {
        check(apartment.result);
        if(!appAudioSupported())check(HRESULT_FROM_WIN32(ERROR_OLD_WIN_VERSION));
        if(!app.processId||app.processId==GetCurrentProcessId()||!app.created)check(E_INVALIDARG);
        const auto ancestors=ownProcessTree();
        if(std::find(ancestors.begin(),ancestors.end(),app.processId)!=ancestors.end())check(E_INVALIDARG);
        Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,app.processId)};
        if(!process.value)check(HRESULT_FROM_WIN32(GetLastError()));
        if(creationTime(process.value)!=app.created||WaitForSingleObject(process.value,0)!=WAIT_TIMEOUT)check(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
        auto activation=Microsoft::WRL::Make<Activation>();if(!activation)check(E_OUTOFMEMORY);
        if(!activation->done.value)check(HRESULT_FROM_WIN32(GetLastError()));
        activation->parameters.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
        activation->parameters.ProcessLoopbackParams={app.processId,PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE};
        PROPVARIANT params{};params.vt=VT_BLOB;params.blob.cbSize=sizeof(activation->parameters);
        params.blob.pBlobData=reinterpret_cast<BYTE*>(&activation->parameters);
        ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
        check(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),&params,activation.Get(),&operation));
        HANDLE activationEvents[]={stop_,process.value,activation->done.value};
        const auto wait=WaitForMultipleObjects(3,activationEvents,FALSE,5000);
        if(wait==WAIT_OBJECT_0) {finished_=true;if(priority)AvRevertMmThreadCharacteristics(priority);return;}
        if(wait!=WAIT_OBJECT_0+2)check(HRESULT_FROM_WIN32(wait==WAIT_TIMEOUT?ERROR_TIMEOUT:ERROR_PROCESS_ABORTED));
        check(activation->result);client=activation->client;
        WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;
        format.nSamplesPerSec=sampleRate;format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=sampleRate*4;
        check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,0,0,&format,nullptr));
        ready.value=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!ready.value)check(HRESULT_FROM_WIN32(GetLastError()));
        check(client->SetEventHandle(ready.value));
        ComPtr<IAudioCaptureClient> capture;check(client->GetService(IID_PPV_ARGS(&capture)));
        check(client->Start());active_=true;
        HANDLE events[]={stop_,process.value,ready.value};std::array<float,frameSize> mono{};
        for(;;){
            const auto result=WaitForMultipleObjects(3,events,FALSE,1000);
            if(result==WAIT_OBJECT_0)break;
            if(result==WAIT_OBJECT_0+1)check(HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED));
            if(result==WAIT_TIMEOUT)continue; // Silent apps need not emit packets.
            if(result!=WAIT_OBJECT_0+2)check(E_FAIL);
            UINT32 packet{};check(capture->GetNextPacketSize(&packet));
            while(packet&&WaitForSingleObject(stop_,0)==WAIT_TIMEOUT){
                BYTE* data{};UINT32 frames{};DWORD flags{};
                check(capture->GetBuffer(&data,&frames,&flags,nullptr,nullptr));
                for(UINT32 base=0;base<frames;){
                    const auto count=std::min<UINT32>(frameSize,frames-base);
                    for(UINT32 i=0;i<count;++i){
                        int16_t stereo[2]{};
                        if(data&&!(flags&AUDCLNT_BUFFERFLAGS_SILENT))memcpy(stereo,data+(base+i)*4,4);
                        mono[i]=(float(stereo[0])+float(stereo[1]))/65536.f;
                    }
                    queue_.push({mono.data(),count});base+=count;
                }
                check(capture->ReleaseBuffer(frames));check(capture->GetNextPacketSize(&packet));
            }
        }
    }catch(HRESULT hr){std::lock_guard lock(mutex_);error_=hr==HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED)?L"The app closed. Refresh the list to share it again.":L"Could not capture this app. Refresh and try again. "+errorMessage(hr);}
    catch(...){std::lock_guard lock(mutex_);error_=L"Could not capture this app.";}
    active_=false;if(client)client->Stop();client.Reset();finished_=true;
    if(priority)AvRevertMmThreadCharacteristics(priority);
}
}
