#include "audio/Engine.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <psapi.h>
namespace gate { void verifySilentOutput(const std::wstring&,Diagnostics&); }
int wmain(int argc,wchar_t** argv){
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    try{
        auto devices=gate::enumerateDevices();
        std::wcout<<L"Physical microphone endpoints: "<<devices.microphones.size()<<L"\nListening endpoints: "<<devices.listeners.size()<<L"\nVB-CABLE playback endpoints: "<<devices.cables.size()<<L'\n';
        for(auto& d:devices.microphones)std::wcout<<L"Input: "<<d.name<<L'\n';
        for(auto& d:devices.listeners)std::wcout<<L"Listen: "<<d.name<<(d.speakers?L" (speakers)":L"")<<L'\n';
        if(argc==2&&std::wstring(argv[1])==L"--render-silence"){
            if(devices.listeners.empty())return 2;
            gate::Diagnostics stats;
            std::wcout<<L"Rendering only digital silence to: "<<devices.listeners.front().name<<L'\n';
            gate::verifySilentOutput(devices.listeners.front().id,stats);
            std::wcout<<L"Three silent render start/stop cycles passed. Underruns: "<<stats.underruns<<L"; overflows: "<<stats.overflows<<L'\n';
        }
        if(argc==3&&std::wstring(argv[1])==L"--capture-seconds"){
            const int seconds=std::clamp(_wtoi(argv[2]),1,120);
            FILETIME created{},exited{},kernelBefore{},userBefore{},kernelAfter{},userAfter{};
            GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernelBefore,&userBefore);
            gate::Engine engine;engine.start(nullptr,devices.defaultMicrophone,devices.defaultListener,{});
            std::this_thread::sleep_for(std::chrono::seconds(seconds));
            auto state=engine.status();
            GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernelAfter,&userAfter);
            PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
            GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));
            engine.stop();auto& s=engine.diagnostics();
            auto ticks=[](FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
            const double cpu=double(ticks(kernelAfter)+ticks(userAfter)-ticks(kernelBefore)-ticks(userBefore))/10000000./seconds*100.;
            std::wcout<<state.routeMessage<<L"\nBlocks: "<<s.blocks<<L"\nDiscontinuities: "<<s.discontinuities<<L"\nUnderruns: "<<s.underruns<<L"\nOverflows: "<<s.overflows<<L"\nMax processing us: "<<s.maxProcessingUs<<L'\n';
            std::wcout<<L"Deadline overruns: "<<s.overruns<<L"\nCPU (% of one logical core, including startup): "<<cpu<<L"\nPrivate MiB: "<<double(memory.PrivateUsage)/1048576.<<L'\n';
            if(!state.capturing)return 2;
        }
    }catch(HRESULT hr){std::wcerr<<gate::errorMessage(hr);CoUninitialize();return 1;}
    CoUninitialize();return 0;
}
