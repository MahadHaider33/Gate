// Manual integration probe: two quiet synthesized tones, no microphone capture.
// Capture a child process tree and confirm a sibling's tone is excluded.
#include "audio/AppAudio.h"
#include <mmsystem.h>
#include <iostream>
#include <cmath>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Child {
    PROCESS_INFORMATION info{};
    explicit Child(const std::wstring& args){
        wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,DWORD(std::size(exe)));
        auto command=L"\""+std::wstring(exe)+L"\" "+args;
        STARTUPINFOW startup{sizeof(startup)};
        require(CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&info)!=FALSE,"Start tone source");
    }
    ~Child(){if(WaitForSingleObject(info.hProcess,10000)==WAIT_TIMEOUT)TerminateProcess(info.hProcess,1);CloseHandle(info.hThread);CloseHandle(info.hProcess);}
    gate::AudioApp app() const {
        FILETIME c{},e{},k{},u{};GetProcessTimes(info.hProcess,&c,&e,&k,&u);
        return {info.dwProcessId,(uint64_t(c.dwHighDateTime)<<32)|c.dwLowDateTime,L"Gate test source"};
    }
};
int tone(double frequency){
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;format.nSamplesPerSec=48000;
    format.wBitsPerSample=16;format.nBlockAlign=2;format.nAvgBytesPerSec=96000;
    HWAVEOUT output{};
    require(waveOutOpen(&output,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)==MMSYSERR_NOERROR,"Open test render device");
    std::vector<int16_t> samples(48000);
    for(size_t i=0;i<samples.size();++i)samples[i]=int16_t(500*std::sin(6.283185307179586*frequency*double(i)/48000));
    WAVEHDR header{};header.lpData=reinterpret_cast<LPSTR>(samples.data());header.dwBufferLength=DWORD(samples.size()*2);
    header.dwFlags=WHDR_BEGINLOOP|WHDR_ENDLOOP;header.dwLoops=8;
    waveOutPrepareHeader(output,&header,sizeof(header));waveOutWrite(output,&header,sizeof(header));
    Sleep(8000);waveOutReset(output);waveOutUnprepareHeader(output,&header,sizeof(header));waveOutClose(output);return 0;
}
double power(const std::array<float,gate::frameSize>& samples,double frequency){
    double real=0,imag=0;
    for(size_t i=0;i<samples.size();++i){const auto phase=6.283185307179586*frequency*double(i)/48000;real+=samples[i]*std::cos(phase);imag+=samples[i]*std::sin(phase);}
    return real*real+imag*imag;
}
}
int wmain(int argc,wchar_t** argv){
    try{
        if(argc==3&&std::wstring(argv[1])==L"--tone")return tone(_wtof(argv[2]));
        if(argc==2&&std::wstring(argv[1])==L"--tree"){Child source(L"--tone 500");return 0;}
        if(!gate::appAudioSupported()){std::cout<<"SKIP: process loopback unsupported on this Windows build.\n";return 2;}
        Child selected(L"--tree"),other(L"--tone 1000");gate::AppAudio capture;
        capture.start(selected.app());
        for(unsigned i=0;i<500&&!capture.active()&&!capture.finished();++i)Sleep(10);
        if(!capture.active())std::wcerr<<capture.error()<<L'\n';
        require(capture.active(),"Process loopback activates");
        double wanted=0,unwanted=0;std::array<float,gate::frameSize> block{};
        for(unsigned i=0;i<200;++i){capture.read(block.data());wanted+=power(block,500);unwanted+=power(block,1000);Sleep(10);}
        std::cout<<"Selected tree power: "<<wanted<<"; unrelated app power: "<<unwanted<<'\n';
        require(wanted>1,"Selected app child audio captured");require(unwanted<wanted*.01,"Unrelated app excluded");
        capture.stop();block.fill(1);capture.read(block.data());
        for(float v:block)require(v==0,"Stop clears media");
        capture.start(selected.app());
        for(unsigned i=0;i<500&&!capture.active()&&!capture.finished();++i)Sleep(10);
        require(capture.active(),"Capture restarts after Stop");
        for(unsigned i=0;i<1000&&!capture.finished();++i){capture.read(block.data());Sleep(10);}
        require(capture.finished()&&!capture.active()&&!capture.error().empty(),"App exit stops capture");
        capture.stop();auto stale=selected.app();++stale.created;capture.start(stale);
        for(unsigned i=0;i<200&&!capture.finished();++i)Sleep(10);
        require(capture.finished()&&!capture.active()&&!capture.error().empty(),"Stale process identity rejected");capture.stop();
        std::cout<<"Process tree isolation, stop, restart, app exit, and stale identity passed.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
