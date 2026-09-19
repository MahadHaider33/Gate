#include "ui/Window.h"
#include <oleacc.h>
#include <algorithm>
#include <cmath>
#include <cwctype>

namespace gate {
namespace {
enum MediaId {MediaNav=420,MediaApp,MediaRefresh,MediaStart,MediaChange,MediaVolume,MediaMicVolume,MediaMicMute,MediaMute,MediaRemove};
constexpr float faderTop=174,faderHeight=182;
}
bool Window::isMediaFader(HWND h) const {return h==mediaVolume_||h==mediaMicVolume_;}
void Window::createMediaPage(){
    mediaNav_=CreateWindowExW(0,WC_BUTTONW,L"Media",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,1,1,hwnd_,reinterpret_cast<HMENU>(MediaNav),instance_,nullptr);
    SetWindowSubclass(mediaNav_,controlProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    auto make=[&](const wchar_t* cls,const wchar_t* title,DWORD style,unsigned id){
        auto h=CreateWindowExW(0,cls,title,WS_CHILD|WS_TABSTOP|style,0,0,1,1,content_,reinterpret_cast<HMENU>(UINT_PTR(id)),instance_,nullptr);
        SetWindowSubclass(h,controlProcedure,1,reinterpret_cast<DWORD_PTR>(this));mediaControls_.push_back(h);return h;
    };
    mediaApp_=make(WC_COMBOBOXW,L"App to share",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL,MediaApp);
    mediaRemove_=make(WC_BUTTONW,L"Remove selected app",BS_OWNERDRAW,MediaRemove);
    mediaRefresh_=make(WC_BUTTONW,L"Refresh apps",BS_OWNERDRAW,MediaRefresh);
    mediaChange_=make(WC_BUTTONW,L"Change app",BS_OWNERDRAW,MediaChange);
    mediaStart_=make(WC_BUTTONW,L"Start sharing",BS_OWNERDRAW,MediaStart);
    mediaMicVolume_=make(TRACKBAR_CLASSW,L"Microphone volume, percent",TBS_NOTICKS|TBS_VERT,MediaMicVolume);
    mediaVolume_=make(TRACKBAR_CLASSW,L"App audio volume, percent",TBS_NOTICKS|TBS_VERT,MediaVolume);
    mediaMicMute_=make(WC_BUTTONW,L"Mute microphone",BS_OWNERDRAW,MediaMicMute);
    mediaMute_=make(WC_BUTTONW,L"Mute app audio",BS_OWNERDRAW,MediaMute);
    for(auto h:{mediaMicVolume_,mediaVolume_}){SendMessageW(h,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(h,TBM_SETPOS,TRUE,h==mediaVolume_?mediaVolumePercent_:mediaMicPercent_);}
    ComPtr<IAccPropServices> accessibility;
    if(SUCCEEDED(CoCreateInstance(CLSID_AccPropServices,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&accessibility)))){
        accessibility->SetHwndPropStr(mediaApp_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_NAME,L"App to share");
        accessibility->SetHwndPropStr(mediaMicVolume_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_NAME,L"Microphone volume, percent");
        accessibility->SetHwndPropStr(mediaVolume_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_NAME,L"App audio volume, percent");
    }
    syncMedia();
}
std::wstring Window::mediaAppName() const {
    const auto index=SendMessageW(mediaApp_,CB_GETCURSEL,0,0);
    std::wstring name=state_.mediaActive||state_.mediaStarting?state_.mediaName:
        index>=0&&size_t(index)<mediaApps_.size()?mediaApps_[size_t(index)].name:L"";
    if(name.empty())return L"Choose an app";
    if(name.find(L"Opera GX")!=std::wstring::npos)return L"Opera GX";
    name=name.substr(0,name.find(L" — "));
    if(name.size()>4&&name.substr(name.size()-4)==L".exe")name.resize(name.size()-4);
    if(!name.empty())name[0]=wchar_t(towupper(name[0]));return name;
}
void Window::refreshApps(){
    AudioApp selected;const auto index=SendMessageW(mediaApp_,CB_GETCURSEL,0,0);
    if(index>=0&&size_t(index)<mediaApps_.size())selected=mediaApps_[size_t(index)];
    mediaApps_=enumerateAudioApps();SendMessageW(mediaApp_,CB_RESETCONTENT,0,0);
    for(size_t i=0;i<mediaApps_.size();++i){
        const auto& app=mediaApps_[i];SendMessageW(mediaApp_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(app.name.c_str()));
        if(app.processId==selected.processId&&app.created==selected.created)SendMessageW(mediaApp_,CB_SETCURSEL,i,0);
    }
    syncMedia();InvalidateRect(mediaApp_,nullptr,FALSE);InvalidateRect(content_,nullptr,FALSE);
}
void Window::syncMedia(){
    const bool supported=appAudioSupported(),busy=state_.mediaActive||state_.mediaStarting;
    EnableWindow(mediaRemove_,busy||SendMessageW(mediaApp_,CB_GETCURSEL,0,0)!=CB_ERR);
    EnableWindow(mediaApp_,supported&&!busy);EnableWindow(mediaRefresh_,supported&&!busy);EnableWindow(mediaChange_,supported&&!busy);
    EnableWindow(mediaStart_,busy||(supported&&!state_.paused&&state_.cableActive&&SendMessageW(mediaApp_,CB_GETCURSEL,0,0)!=CB_ERR));
    const auto title=busy?L"Stop sharing":L"Start sharing";wchar_t current[64]{};GetWindowTextW(mediaStart_,current,64);
    if(wcscmp(current,title))SetWindowTextW(mediaStart_,title);
    EnableWindow(mediaVolume_,supported);EnableWindow(mediaMute_,supported);
}
void Window::applyMixLevels(){
    engine_.setMixLevels(mediaMicPercent_,mediaVolumePercent_,mediaMicMuted_,mediaMuted_);
    SetWindowTextW(mediaMicMute_,mediaMicMuted_?L"Unmute microphone":L"Mute microphone");
    SetWindowTextW(mediaMute_,mediaMuted_?L"Unmute app audio":L"Mute app audio");
    InvalidateRect(content_,nullptr,FALSE);
    for(auto h:{mediaMicVolume_,mediaVolume_,mediaMicMute_,mediaMute_})InvalidateRect(h,nullptr,FALSE);
}
void Window::layoutMediaPage(LayoutBatch& batch){
    const float span=contentWidth_,card=(span-16)/2;
    auto pos=[&](HWND h,float x,float y,float width,float height){batch.move(h,px(x),px(top(y)),px(width),px(height));};
    pos(mediaApp_,12,17,span-214,276);pos(mediaRemove_,span-197,26,34,36);pos(mediaRefresh_,span-157,26,34,36);pos(mediaChange_,span-117,26,105,36);
    pos(mediaMicVolume_,card/2+10,faderTop,48,faderHeight);pos(mediaVolume_,card+16+card/2+10,faderTop,48,faderHeight);
    pos(mediaMicMute_,card/2-31,386,62,32);pos(mediaMute_,card+16+card/2-31,386,62,32);
    const float button=std::min(222.f,span*.39f);pos(mediaStart_,span-button-18,461,button,42);
}
void Window::invalidateMediaMeters(){
    const float card=(contentWidth_-16)/2;
    for(float x:{0.f,card+16}){RECT r{px(x+card/2-51),px(top(faderTop)),px(x+card/2-19),px(top(faderTop+faderHeight))};InvalidateRect(content_,&r,FALSE);}
}
void Window::paintMediaPage(ID2D1RenderTarget* target){
    const auto& p=theme_.colors();const float span=contentWidth_,card=(span-16)/2;
    ComPtr<ID2D1SolidColorBrush> brush;target->CreateSolidColorBrush(p.card,&brush);
    auto text=[&](const std::wstring& s,float x,float y,float w,float h,float size,D2D1_COLOR_F color,bool bold=false,bool wrap=false){label(target,s,{x,top(y),x+w,top(y+h)},size,color,bold,DWRITE_TEXT_ALIGNMENT_LEADING,wrap);};
    auto panel=[&](float x,float y,float w,float h){auto r=D2D1::RoundedRect({x+.5f,top(y)+.5f,x+w-.5f,top(y+h)-.5f},9,9);brush->SetColor(p.card);target->FillRoundedRectangle(r,brush.Get());brush->SetColor(p.border);target->DrawRoundedRectangle(r,brush.Get());};
    panel(0,8,span,72);panel(0,96,card,332);panel(card+16,96,card,332);panel(0,444,span,76);
    brush->SetColor(p.border);target->DrawLine({span-206,top(28)},{span-206,top(60)},brush.Get());
    const bool mutedMic=mediaMicMuted_||mediaMicPercent_==0,mutedApp=mediaMuted_||mediaVolumePercent_==0;
    for(unsigned i=0;i<2;++i){
        const float x=i?card+16:0;
        if(i&&mediaAppName().find(L"Opera")!=std::wstring::npos)label(target,L"O",{x+18,top(109),x+46,top(141)},27,p.text,true,DWRITE_TEXT_ALIGNMENT_CENTER);
        else drawIcon(target,i?6:0,x+18,top(112),28,p.text);
        text(i?L"App audio":L"Microphone",x+56,110,card-70,26,16,p.text,true);
        const std::wstring voice=preferences_.voice.enabled?voiceNames[unsigned(preferences_.voice.preset)]:L"Normal";
        text(i?mediaAppName():voice==L"Normal"?L"Natural voice":voice,x+56,138,card-145,22,12,p.secondary);
        const bool muted=i?mutedApp:mutedMic,active=i?state_.mediaActive:state_.capturing;
        const wchar_t* status=muted?L"Muted":state_.paused?L"Paused":active?(i?L"Live":L"Active"):i?(state_.mediaStarting?L"Starting":L"Ready"):L"Offline";
        const bool lit=active&&!muted&&!state_.paused;
        const auto pill=D2D1::RoundedRect({x+card-85,top(138),x+card-15,top(160)},11,11);
        brush->SetColor(lit?p.selected:p.field);target->FillRoundedRectangle(pill,brush.Get());
        brush->SetColor(lit?p.accent:p.secondary);target->FillEllipse(D2D1::Ellipse({x+card-74,top(149)},2.5f,2.5f),brush.Get());
        text(status,x+card-67,138,49,22,10.5f,lit?p.accent:p.secondary);
        const float peak=(!active||muted||state_.paused)?0.f:(i?engine_.diagnostics().mediaOutputLevel:engine_.diagnostics().voiceOutputLevel).load(std::memory_order_relaxed);
        const float level=std::clamp((20.f*std::log10(std::max(peak,.001f))+60.f)/60.f,0.f,1.f);
        for(int bar=0;bar<20;++bar){
            const float y=faderTop+12+(19-bar)*8.2f;brush->SetColor(float(bar)/20<level?p.accent:p.track);
            target->FillRoundedRectangle(D2D1::RoundedRect({x+card/2-49,top(y),x+card/2-21,top(y+5.6f)},2,2),brush.Get());
        }
        label(target,std::to_wstring(i?mediaVolumePercent_:mediaMicPercent_)+L"%",{x+card/2-3,top(358),x+card/2+72,top(382)},13,muted?p.secondary:p.text,false,DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    brush->SetColor(p.secondary);target->DrawEllipse(D2D1::Ellipse({29,top(482)},9,5),brush.Get(),1.7f);target->DrawEllipse(D2D1::Ellipse({41,top(482)},9,5),brush.Get(),1.7f);
    const float available=span-std::min(222.f,span*.39f)-94;
    text(L"CABLE Output",64,456,available,25,16,p.text,true);
    text(state_.cableActive?L"Select in your call app":L"Not connected",64,486,available,22,12,p.secondary);
    text(L"ⓘ",0,534,20,22,14,p.secondary);
    text(L"Browser sharing includes all tabs. Keep your call in a separate app.",27,534,span-27,36,12,p.secondary,false,true);
    std::wstring status;
    if(!appAudioSupported())status=L"App sharing requires Windows 11 or Windows build 20348+.";
    else if(state_.paused)status=L"Gate is paused. Resume from the tray, then start sharing again.";
    else if(!state_.mediaMessage.empty())status=state_.mediaMessage;
    else if(state_.mediaStarting)status=L"Connecting to the app…";
    else if(!state_.cableActive)status=L"Connect VB-CABLE on the Microphone page to share audio in calls.";
    else if(mediaApps_.empty()&&!state_.mediaActive)status=L"Open an app, then use Refresh apps above.";
    text(status,0,575,span,40,12,p.secondary,false,true);
}
bool Window::paintMediaControl(HWND control,ID2D1RenderTarget* target,float w,float h){
    if(std::find(mediaControls_.begin(),mediaControls_.end(),control)==mediaControls_.end())return false;
    const auto& p=theme_.colors();target->Clear(p.card);
    const bool enabled=IsWindowEnabled(control)!=FALSE,hover=hoverControl_==control&&enabled;
    ComPtr<ID2D1SolidColorBrush> brush;target->CreateSolidColorBrush(p.border,&brush);
    auto round=[&](D2D1_RECT_F r,float radius,D2D1_COLOR_F color){brush->SetColor(color);target->FillRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush.Get());};
    auto line=[&](float a,float b,float c,float d,D2D1_COLOR_F color,float stroke=1.6f){brush->SetColor(color);target->DrawLine({a,b},{c,d},brush.Get(),stroke);};
    if(control==mediaApp_){
        const auto name=mediaAppName();
        if(name.find(L"Opera")!=std::wstring::npos)label(target,L"O",{5,3,39,h-3},30,p.text,true,DWRITE_TEXT_ALIGNMENT_CENTER);
        else drawIcon(target,6,9,h/2-14,28,p.text);
        label(target,name,{51,2,w-8,h/2+2},16,p.text,true);
        label(target,name==L"Choose an app"?L"Select a running application":L"Application audio",{51,h/2,w-8,h-1},12,p.secondary);
    }else if(isMediaFader(control)){
        const bool muted=control==mediaVolume_?mediaMuted_:mediaMicMuted_;
        const float fraction=float(SendMessageW(control,TBM_GETPOS,0,0))/100.f;
        const float x=w/2,y=12+(h-24)*(1-fraction);
        round({x-2,12,x+2,h-12},2,p.track);round({x-2,y,x+2,h-12},2,enabled&&!muted?p.accent:p.secondary);
        brush->SetColor(p.text);target->FillEllipse(D2D1::Ellipse({x,y},8.5f,8.5f),brush.Get());
        brush->SetColor(enabled&&!muted?p.accent:p.secondary);target->DrawEllipse(D2D1::Ellipse({x,y},8.5f,8.5f),brush.Get(),1.5f);
    }else if(control==mediaStart_){
        const bool active=state_.mediaActive||state_.mediaStarting;
        round({0,0,w,h},7,enabled?p.accent:p.field);
        const auto ink=enabled?p.buttonText:p.secondary;const float start=std::max(13.f,(w-137)/2);
        if(active)round({start,h/2-5,start+10,h/2+5},1,ink);
        else {line(start,h/2-5,start+9,h/2,ink);line(start+9,h/2,start,h/2+5,ink);line(start,h/2-5,start,h/2+5,ink);}
        label(target,active?L"Stop sharing":L"Start sharing",{start+19,0,w-10,h},14,ink,true,DWRITE_TEXT_ALIGNMENT_CENTER);
    }else if(control==mediaMicMute_||control==mediaMute_){
        const bool mic=control==mediaMicMute_,muted=mic?mediaMicMuted_:mediaMuted_;const auto ink=muted?p.accent:p.text;
        round({.5f,.5f,w-.5f,h-.5f},6,muted||hover?p.selected:p.field);
        brush->SetColor(muted?p.accent:p.border);target->DrawRoundedRectangle(D2D1::RoundedRect({.5f,.5f,w-.5f,h-.5f},6,6),brush.Get());
        if(mic)drawIcon(target,0,w/2-10,h/2-11,21,ink);
        else {const float x=w/2,y=h/2;line(x-9,y-4,x-4,y-4,ink);line(x-4,y-4,x+2,y-9,ink);line(x+2,y-9,x+2,y+9,ink);line(x+2,y+9,x-4,y+4,ink);line(x-4,y+4,x-9,y+4,ink);line(x-9,y+4,x-9,y-4,ink);if(!muted){line(x+6,y-5,x+9,y,ink);line(x+9,y,x+6,y+5,ink);}}
        if(muted)line(w/2-12,h/2-12,w/2+12,h/2+12,ink,2.f);
    }else{
        if(hover)round({0,0,w,h},5,p.selected);
        if(control==mediaRemove_){
            const auto ink=enabled?(hover?p.text:p.secondary):p.track;
            line(w/2-5,h/2-5,w/2+5,h/2+5,ink);line(w/2+5,h/2-5,w/2-5,h/2+5,ink);
        }
        else if(control==mediaRefresh_)label(target,L"↻",{0,0,w,h},23,enabled?p.secondary:p.track,false,DWRITE_TEXT_ALIGNMENT_CENTER);
        else label(target,L"Change app",{0,0,w,h},13,enabled?p.accent:p.secondary,true,DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    if(GetFocus()==control&&!(SendMessageW(control,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){
        brush->SetColor(p.secondary);target->DrawRoundedRectangle(D2D1::RoundedRect({1.5f,1.5f,w-1.5f,h-1.5f},5,5),brush.Get());
    }
    return true;
}
bool Window::mediaCommand(unsigned id,unsigned notification){
    if(id==MediaApp&&notification==CBN_SELCHANGE){syncMedia();InvalidateRect(mediaApp_,nullptr,FALSE);InvalidateRect(content_,nullptr,FALSE);return true;}
    if(notification!=BN_CLICKED)return false;
    switch(id){
    case MediaNav:setPage(Page::Media);return true;
    case MediaRemove:
        engine_.stopSharing();SendMessageW(mediaApp_,CB_SETCURSEL,WPARAM(-1),0);
        state_.mediaMessage.clear();syncMedia();
        InvalidateRect(mediaApp_,nullptr,FALSE);InvalidateRect(content_,nullptr,FALSE);return true;
    case MediaRefresh:refreshApps();return true;
    case MediaChange:refreshApps();SetFocus(mediaApp_);SendMessageW(mediaApp_,CB_SHOWDROPDOWN,TRUE,0);return true;
    case MediaMicMute:mediaMicMuted_=!mediaMicMuted_;applyMixLevels();return true;
    case MediaMute:mediaMuted_=!mediaMuted_;applyMixLevels();return true;
    case MediaStart:{
        if(state_.mediaActive||state_.mediaStarting){engine_.stopSharing();return true;}
        const auto index=SendMessageW(mediaApp_,CB_GETCURSEL,0,0);
        if(index>=0&&size_t(index)<mediaApps_.size()){
            state_.mediaName=mediaApps_[size_t(index)].name;engine_.shareApp(mediaApps_[size_t(index)]);state_.mediaStarting=true;state_.mediaMessage.clear();syncMedia();updateTimer();InvalidateRect(content_,nullptr,FALSE);
        }return true;
    }
    default:return false;
    }
}
}
