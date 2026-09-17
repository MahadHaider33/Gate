#include "audio/ClipPlayer.h"
#include "audio/Devices.h"
#include "audio/Resampler.h"
#include "audio/Processing.h"
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstring>
namespace gate {
ClipPlayer::~ClipPlayer(){stop();}
void ClipPlayer::stop(){
    enabled_=false;cancel_=true;
    for(auto n=readers_.load();n;n=readers_.load())readers_.wait(n);
    if(worker_.joinable())worker_.join();
    queue_.reset();done_=true;
}
void ClipPlayer::start(const std::wstring& path,bool held){
    stop();{std::lock_guard lock(mutex_);error_.clear();}
    cancel_=false;done_=false;held_=held;enabled_=true;
    worker_=std::thread([this,path]{decode(path);});
}
bool ClipPlayer::playing() const noexcept {return enabled_&&(!done_||queue_.size()!=0);}
std::wstring ClipPlayer::error() const {std::lock_guard lock(mutex_);return error_;}
void ClipPlayer::read(float* out,unsigned count) noexcept {
    std::fill_n(out,count,0.f);
    if(!enabled_||held_)return;
    readers_.fetch_add(1);
    if(enabled_)queue_.pop({out,count});
    readers_.fetch_sub(1);readers_.notify_all();
}
bool ClipPlayer::write(const float* data,unsigned count){
    unsigned offset=0;
    while(offset<count&&!cancel_){offset+=unsigned(queue_.push({data+offset,count-offset}));if(offset<count)Sleep(2);}
    return !cancel_;
}
bool ClipPlayer::decodePcmWave(const std::wstring& path){
    // Gate's starter assets are already mono PCM16 at the mixer rate. Stream
    // these directly, without constructing a Media Foundation decoder graph.
    std::ifstream file(std::filesystem::path(path),std::ios::binary);
    char header[12]{};if(!file.read(header,sizeof(header))||memcmp(header,"RIFF",4)||memcmp(header+8,"WAVE",4))return false;
    file.seekg(0,std::ios::end);const auto fileEnd=file.tellg();file.seekg(12);
    bool compatible=false;
    while(file&&!cancel_){
        char kind[4]{};uint32_t size=0;
        if(!file.read(kind,4)||!file.read(reinterpret_cast<char*>(&size),4))return false;
        const auto start=file.tellg();
        if(start<0||std::streamoff(size)>fileEnd-start)return false;
        if(!memcmp(kind,"fmt ",4)){
            if(size<16)return false;
            struct Format {uint16_t tag,channels;uint32_t rate,bytes;uint16_t align,bits;} format{};
            static_assert(sizeof(format)==16);
            if(!file.read(reinterpret_cast<char*>(&format),sizeof(format)))return false;
            compatible=format.tag==1&&format.channels==1&&format.rate==sampleRate&&format.align==2&&format.bits==16;
            if(!compatible)return false;
        }else if(!memcmp(kind,"data",4)){
            if(!compatible||!size||size%2)return false;
            std::array<int16_t,2048> pcm{};std::array<float,2048> samples{};
            while(size&&!cancel_){
                const unsigned count=std::min(size/2,unsigned(pcm.size()));
                if(!file.read(reinterpret_cast<char*>(pcm.data()),count*2))check(E_FAIL);
                for(unsigned i=0;i<count;++i)samples[i]=pcm[i]/32768.f;
                if(!write(samples.data(),count))break;
                size-=count*2;
            }
            return true;
        }
        file.seekg(start+std::streamoff(size)+std::streamoff(size&1));
    }
    return cancel_;
}
void ClipPlayer::decode(std::wstring path) noexcept {
    try{if(decodePcmWave(path)){done_=true;return;}}
    catch(...){if(!cancel_){std::lock_guard lock(mutex_);error_=L"Cannot read this sound file.";}done_=true;return;}
    if(cancel_){done_=true;return;}
    const HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    const HRESULT startup=MFStartup(MF_VERSION,MFSTARTUP_LITE);
    try {
        check(startup);
        ComPtr<IMFSourceReader> reader;check(MFCreateSourceReaderFromURL(path.c_str(),nullptr,&reader));
        check(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE));
        check(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM,TRUE));
        ComPtr<IMFMediaType> type;check(MFCreateMediaType(&type));
        check(type->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio));check(type->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_Float));
        check(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,nullptr,type.Get()));
        check(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,&type));
        UINT32 rate=0,channels=0;check(type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,&rate));check(type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,&channels));
        if(rate<8000||rate>192000||channels==0||channels>8)check(E_INVALIDARG);
        Resampler resampler;resampler.prepare(rate,sampleRate);
        std::array<float,1024> mono{};std::array<float,8192> converted{};
        bool received=false;
        while(!cancel_){
            DWORD flags=0;ComPtr<IMFSample> sample;
            check(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,nullptr,&flags,nullptr,&sample));
            if(flags&MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)check(MF_E_INVALIDMEDIATYPE);
            if(sample){
                ComPtr<IMFMediaBuffer> buffer;check(sample->ConvertToContiguousBuffer(&buffer));
                BYTE* bytes=nullptr;DWORD length=0;check(buffer->Lock(&bytes,nullptr,&length));
                // Unlock even when conversion fails or playback is cancelled.
                struct Unlock { IMFMediaBuffer* b;~Unlock(){b->Unlock();} } unlock{buffer.Get()};
                const auto* values=reinterpret_cast<const float*>(bytes);const unsigned frames=length/(sizeof(float)*channels);
                for(unsigned base=0;base<frames&&!cancel_;){
                    const unsigned count=std::min(unsigned(mono.size()),frames-base);
                    for(unsigned i=0;i<count;++i){float sum=0;for(unsigned c=0;c<channels;++c)sum+=values[(base+i)*channels+c];mono[i]=std::isfinite(sum)?std::clamp(sum/channels,-1.f,1.f):0.f;}
                    uint32_t offset=0;
                    while(offset<count&&!cancel_){uint32_t in=count-offset,out=unsigned(converted.size());if(!resampler.process(mono.data()+offset,in,converted.data(),out))check(E_FAIL);offset+=in;if(out){received=true;if(!write(converted.data(),out))break;}if(!in&&!out)check(E_FAIL);}
                    base+=count;
                }
            }
            if(flags&MF_SOURCE_READERF_ENDOFSTREAM)break;
        }
        if(!cancel_&&!received)check(MF_E_INVALID_FILE_FORMAT);
        // Drain the short resampler tail, without retaining audio history.
        if(!cancel_){mono.fill(0);uint32_t in=std::min(unsigned(mono.size()),resampler.latency()+64u),out=unsigned(converted.size());if(resampler.process(mono.data(),in,converted.data(),out))write(converted.data(),out);}
    } catch(...) {if(!cancel_){std::lock_guard lock(mutex_);error_=L"Cannot play this file. Import a supported WAV or MP3 file.";}}
    done_=true;
    if(SUCCEEDED(startup))MFShutdown();if(SUCCEEDED(apartment))CoUninitialize();
}
}
