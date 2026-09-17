#include "audio/Processing.h"
#include "audio/Resampler.h"
#include "audio/SpscQueue.h"
#include "audio/Devices.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void parameters(){
    for(unsigned strength=0;strength<=100;++strength)for(int threshold=-70;threshold<=-20;++threshold){
        gate::Parameters p{strength%2==0,strength%3==0,strength,threshold};auto q=gate::unpack(gate::pack(p));
        require(p.suppression==q.suppression&&p.gate==q.gate&&p.strength==q.strength&&p.thresholdDb==q.thresholdDb,"Parameter round trip");
    }
    require(gate::unpack(0xffffffff).thresholdDb==-20,"Corrupt settings are bounded");
}
void queue(){
    gate::SpscQueue<128> q;std::atomic<bool> failed=false;
    std::thread producer([&]{for(unsigned i=0;i<100000;++i){float v=float(i);while(q.push({&v,1})==0)std::this_thread::yield();}});
    for(unsigned i=0;i<100000;++i){float v;while(q.pop({&v,1})==0)std::this_thread::yield();if(v!=float(i))failed=true;}
    producer.join();require(!failed&&q.size()==0,"SPSC ordering and wraparound");
    std::array<float,200> values{};require(q.push(values)==128,"Queue bounded overflow");q.reset();require(q.size()==0,"Reset clears streaming queue");
}
void processor(){
    gate::Processor processor;gate::Parameters p{false,false,100,-50};processor.prepare(p);
    std::array<float,gate::frameSize> in{},out{};
    in[0]=.5f;processor.process(in.data(),out.data(),p);require(out[0]==0,"Dry path is delayed");
    in.fill(0);processor.process(in.data(),out.data(),p);require(out[0]==0,"Two-frame delay holds");
    processor.process(in.data(),out.data(),p);require(std::abs(out[0]-.5f)<.00001f,"Aligned dry signal is preserved");
    p.suppression=true;processor.prepare(p);
    for(int frame=0;frame<100;++frame){processor.process(in.data(),out.data(),p);for(float v:out)require(std::isfinite(v)&&std::abs(v)<.0001f,"Suppression silence");}
    p={false,true,0,-50};processor.prepare(p);in.fill(.0001f);
    for(int frame=0;frame<200;++frame)processor.process(in.data(),out.data(),p);
    require(std::abs(out.back())<.0000001f,"Gate attenuates below threshold");
    in.fill(.1f);for(int frame=0;frame<30;++frame)processor.process(in.data(),out.data(),p);
    require(out.back()>.09f,"Gate opens above threshold");
}
void resampling(){
    for(auto rate:{8000u,44100u,48000u,96000u,192000u}){
        gate::Resampler converter;converter.prepare(rate,48000);
        std::vector<float> in(rate,.1f),out(49000);uint32_t n=rate,m=uint32_t(out.size());
        require(converter.process(in.data(),n,out.data(),m),"SpeexDSP conversion");
        require(n==rate&&m>47000&&m<=48000,"Converted duration");
        for(uint32_t i=0;i<m;++i)require(std::isfinite(out[i]),"Finite resampled audio");
    }
}
void blendAlignment(){
    // Deterministic broadband fixture tests time alignment, not speech quality.
    constexpr int length=24000;
    std::vector<float> in(length),out(length);
    uint32_t random=17;
    for(int i=0;i<length;++i){
        random=random*1664525u+1013904223u;
        in[i]=(float(random>>8)/16777216.f-.5f)*.4f;
    }
    gate::Processor processor;processor.prepare({});
    for(int i=0;i<length;i+=480)processor.process(in.data()+i,out.data()+i,{});
    int bestLag=-1;double best=-1;
    for(int lag=0;lag<=1440;++lag){
        double cross=0,energyA=0,energyB=0;
        for(int i=4800;i<length;++i){const double a=in[i-lag],b=out[i];cross+=a*b;energyA+=a*a;energyB+=b*b;}
        const double score=cross/std::sqrt(std::max(energyA*energyB,1e-30));
        if(score>best){best=score;bestLag=lag;}
    }
    std::cout<<"Wet-path fixture delay="<<bestLag<<" samples, correlation="<<best<<'\n';
    // Suppression deliberately destroys most broadband correlation; only the
    // surviving waveform's unambiguous correlation peak is a timing assertion.
    require(std::abs(bestLag-int(gate::processingDelay))<=1&&best>.03,"RNNoise waveform aligns with dry blend");
}
void routing(){
    require(gate::isCableIdentity(L"ROOT\\VB_AUDIO_CABLE\\0000",L"renamed"),"Cable identity survives renaming");
    require(gate::isCableIdentity(L"SWD\\MMDEVAPI\\endpoint",L"VB-Audio Virtual Cable"),"Cable driver interface identity");
    require(gate::isVirtualIdentity(L"ROOT\\MEDIA",L"Voicemeeter VAIO"),"Routing endpoint rejected");
    require(gate::isVirtualIdentity(L"ROOT\\MEDIA",L"FxSound Audio Enhancer"),"Virtual enhancement output rejected");
    require(!gate::isVirtualIdentity(L"USB\\VID_1234",L"USB Audio Device"),"Physical device eligible");
}
void benchmark(){
    gate::Processor processor;processor.prepare({});std::array<float,480> in{},out{};
    for(size_t i=0;i<in.size();++i)in[i]=.05f*std::sin(float(i)*.13f);
    std::vector<double> elapsed;elapsed.reserve(1000);
    for(int n=0;n<1000;++n){auto begin=std::chrono::steady_clock::now();processor.process(in.data(),out.data(),{});auto end=std::chrono::steady_clock::now();elapsed.push_back(std::chrono::duration<double,std::micro>(end-begin).count());}
    std::sort(elapsed.begin(),elapsed.end());std::cout<<"RNNoise processing p50="<<elapsed[500]<<" us p99="<<elapsed[990]<<" us\n";
}
}
int main(){try{parameters();queue();processor();resampling();blendAlignment();routing();benchmark();std::cout<<"All core checks passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
