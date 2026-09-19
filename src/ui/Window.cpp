#include <initguid.h>
#include "ui/Window.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <oleacc.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace gate {
namespace {
enum Id { Input=100, Listener, Suppression, GateToggle, Strength, Threshold, Test, MicNav, VoiceNav, Setup };
constexpr UINT trayMessage=WM_APP+22;
constexpr UINT meterTimer=1,saveTimer=2;
constexpr UINT openTray=301,pauseTray=302,exitTray=303;

}
Window::Window(HINSTANCE instance):instance_(instance),preferences_(Preferences::load()) {
    try {if(preferences_.installStarterSounds())preferences_.save();}
    catch(...) {importMessage_=L"Starter sounds could not be added. Check access to your soundboard folder and restart Gate.";}
    savedClips_=preferences_.clips;
}
int Window::run(int show) {
    check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory_.GetAddressOf()));
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(textFactory_.GetAddressOf())));
    WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=procedure;cls.hInstance=instance_;cls.hCursor=LoadCursor(nullptr,IDC_ARROW);
    cls.lpszClassName=L"GateDesktopWindow";cls.hIcon=LoadIconW(instance_,MAKEINTRESOURCEW(101));cls.hIconSm=cls.hIcon;
    RegisterClassExW(&cls);
    WNDCLASSEXW body{sizeof(body)};body.lpfnWndProc=contentProcedure;body.hInstance=instance_;body.hCursor=cls.hCursor;body.lpszClassName=L"GateContentWindow";RegisterClassExW(&body);
    dpi_=GetDpiForSystem();
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    const int windowWidth=std::min(px(1100),int(work.right-work.left));
    const int windowHeight=std::min(px(790),int(work.bottom-work.top));
    hwnd_=CreateWindowExW(0,cls.lpszClassName,L"Gate",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        work.left+(work.right-work.left-windowWidth)/2,work.top+(work.bottom-work.top-windowHeight)/2,
        windowWidth,windowHeight,nullptr,nullptr,instance_,this);
    if(!hwnd_)check(HRESULT_FROM_WIN32(GetLastError()));
    ShowWindow(hwnd_,show);UpdateWindow(hwnd_);
    engine_.setVoice(preferences_.voice);engine_.setHearSounds(preferences_.hearSounds);
    engine_.start(hwnd_,preferences_.microphone,preferences_.listener,preferences_.processing);
    MSG msg{};
    for(;;){
        if(scrolling_&&scrollClock_){
            const DWORD event=MsgWaitForMultipleObjectsEx(1,&scrollClock_,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(event==WAIT_OBJECT_0)animateScroll();
            else if(event==WAIT_FAILED){const float destination=scrollTarget_;stopScroll();moveScroll(destination);}
            if(!PeekMessageW(&msg,nullptr,0,0,PM_REMOVE))continue;
            if(msg.message==WM_QUIT)break;
        }else if(GetMessageW(&msg,nullptr,0,0)<=0)break;
        if(captureShortcut(msg))continue;
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_TAB)
            SendMessageW(hwnd_,WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);
        if(!(emojiPopup_&&IsDialogMessageW(emojiPopup_,&msg))&&!IsDialogMessageW(hwnd_,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        // Native tab navigation must also bring off-screen controls into view.
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_TAB){
            HWND focus=GetFocus();
            if(focus && focus!=microphoneNav_ && focus!=voiceNav_ && focus!=soundNav_ && focus!=mediaNav_ && IsChild(hwnd_,focus)){
                RECT r{};GetWindowRect(focus,&r);MapWindowPoints(nullptr,content_,reinterpret_cast<POINT*>(&r),2);
                if(r.bottom>px(viewportHeight_-12))scrollTo(scroll_+r.bottom/scale()-viewportHeight_+42,false);
                else if(r.top<px(12))scrollTo(scroll_+r.top/scale()-12,false);
            }
        }
    }
    return int(msg.wParam);
}
LRESULT CALLBACK Window::procedure(HWND h,UINT msg,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->hwnd_=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(h,msg,w,l);
    try{return self->message(msg,w,l);}catch(...){return DefWindowProcW(h,msg,w,l);}
}
void Window::createControls() {
    content_=CreateWindowExW(WS_EX_CONTROLPARENT,L"GateContentWindow",nullptr,WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,1,1,hwnd_,nullptr,instance_,this);
    auto create=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id){return CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,0,0,1,1,(id==MicNav||id==VoiceNav)?hwnd_:content_,reinterpret_cast<HMENU>(INT_PTR(id)),instance_,nullptr);};
    input_=create(WC_COMBOBOXW,L"Input Device",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL,Input);
    listener_=create(WC_COMBOBOXW,L"Listening Device",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL,Listener);
    suppression_=create(WC_BUTTONW,L"Background Noise Reduction",BS_AUTOCHECKBOX,Suppression);
    gate_=create(WC_BUTTONW,L"Noise Gate",BS_AUTOCHECKBOX,GateToggle);
    strength_=create(TRACKBAR_CLASSW,L"Strength: blend of original and cleaned audio",TBS_NOTICKS,Strength);
    threshold_=create(TRACKBAR_CLASSW,L"Noise gate cutoff, percent",TBS_NOTICKS,Threshold);
    test_=create(WC_BUTTONW,L"Test Microphone",BS_OWNERDRAW,Test);
    microphoneNav_=create(WC_BUTTONW,L"Microphone",BS_OWNERDRAW,MicNav);
    voiceNav_=create(WC_BUTTONW,L"Voice Changer",BS_OWNERDRAW,VoiceNav);
    setup_=create(WC_BUTTONW,L"Get VB-CABLE",BS_OWNERDRAW,Setup);
    SendMessageW(strength_,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(strength_,TBM_SETPOS,TRUE,preferences_.processing.strength);
    SendMessageW(threshold_,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(threshold_,TBM_SETLINESIZE,0,1);SendMessageW(threshold_,TBM_SETPOS,TRUE,gateThresholdPercent(preferences_.processing.thresholdDb));
    SendMessageW(suppression_,BM_SETCHECK,preferences_.processing.suppression?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(gate_,BM_SETCHECK,preferences_.processing.gate?BST_CHECKED:BST_UNCHECKED,0);
    EnableWindow(strength_,preferences_.processing.suppression);EnableWindow(threshold_,preferences_.processing.gate);
    ComPtr<IAccPropServices> accessibility;
    if(SUCCEEDED(CoCreateInstance(CLSID_AccPropServices,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&accessibility)))){
        accessibility->SetHwndPropStr(input_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_NAME,L"Input Device");
        accessibility->SetHwndPropStr(listener_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_NAME,L"Listening Device");
        accessibility->SetHwndPropStr(listener_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_DESCRIPTION,L"Select the device used by Test Microphone.");
        accessibility->SetHwndPropStr(strength_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_NAME,L"Strength, blend of original and cleaned audio, percent");
        accessibility->SetHwndPropStr(threshold_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_NAME,L"Noise gate cutoff, percent");
        accessibility->SetHwndPropStr(threshold_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_DESCRIPTION,L"Higher settings block more quiet sounds and need a louder voice to open. Lower it if quiet speech is cut off.");
    }
    tooltip_=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP,0,0,0,0,hwnd_,nullptr,instance_,nullptr);
    TOOLINFOW tip{sizeof(tip)};tip.uFlags=TTF_IDISHWND|TTF_SUBCLASS;tip.hwnd=hwnd_;tip.uId=reinterpret_cast<UINT_PTR>(strength_);
    tip.lpszText=const_cast<wchar_t*>(L"Blends original and cleaned audio. 100% uses the fully processed signal; this does not change the model's intensity.");
    SendMessageW(tooltip_,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tip));SendMessageW(tooltip_,TTM_SETMAXTIPWIDTH,0,360);
    tip.uId=reinterpret_cast<UINT_PTR>(threshold_);
    tip.lpszText=const_cast<wchar_t*>(L"Higher settings block more quiet sounds. Lower this if quiet speech is cut off. 0% is the lowest cutoff, not off; use the Noise Gate switch to disable it.");
    SendMessageW(tooltip_,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tip));
    for(auto control:{input_,listener_,suppression_,gate_,strength_,threshold_,test_,microphoneNav_,voiceNav_,setup_})
        SetWindowSubclass(control,controlProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    createVoicePages();
    createMediaPage();
    updateTheme();syncTest();
}
void Window::updateTheme() {
    theme_.refresh();const auto& p=theme_.colors();
    BOOL dark=p.dark;DwmSetWindowAttribute(hwnd_,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    const COLORREF caption=rgb(p.background),captionText=rgb(p.text);
    DwmSetWindowAttribute(hwnd_,DWMWA_CAPTION_COLOR,&caption,sizeof(caption));
    DwmSetWindowAttribute(hwnd_,DWMWA_TEXT_COLOR,&captionText,sizeof(captionText));
    if(font_)DeleteObject(font_);if(boldFont_)DeleteObject(boldFont_);if(fieldBrush_)DeleteObject(fieldBrush_);
    font_=CreateFontW(-px(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    boldFont_=CreateFontW(-px(16),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    fieldBrush_=CreateSolidBrush(rgb(p.field));
    for(auto h:{input_,listener_,suppression_,gate_,strength_,threshold_,test_,microphoneNav_,voiceNav_,setup_})if(h)SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
    for(auto h:{input_,listener_,mediaApp_}){SendMessageW(h,CB_SETITEMHEIGHT,WPARAM(-1),px(38));SendMessageW(h,CB_SETITEMHEIGHT,0,px(32));}
    SendMessageW(mediaApp_,CB_SETITEMHEIGHT,WPARAM(-1),px(51));
    SendMessageW(mediaNav_,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
    for(auto h:mediaControls_)SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
    updateFeatureTheme();
    target_.Reset();contentTarget_.Reset();controlTarget_.Reset();RedrawWindow(hwnd_,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
void Window::populate(HWND combo,const std::vector<Device>& devices,const std::wstring& id) {
    // Do not disturb an open device picker on periodic audio status updates.
    bool same=SendMessageW(combo,CB_GETCOUNT,0,0)==LRESULT(devices.size());
    if(same)for(size_t i=0;i<devices.size();++i){wchar_t name[2048]{};SendMessageW(combo,CB_GETLBTEXT,i,reinterpret_cast<LPARAM>(name));if(devices[i].name!=name){same=false;break;}}
    if(!same){SendMessageW(combo,CB_RESETCONTENT,0,0);for(const auto& d:devices)SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(d.name.c_str()));}
    int selected=-1;for(size_t i=0;i<devices.size();++i)if(devices[i].id==id)selected=int(i);
    if(SendMessageW(combo,CB_GETCURSEL,0,0)!=selected)SendMessageW(combo,CB_SETCURSEL,selected,0);
}
void Window::refreshStatus() {
    for(auto it=pendingDeletes_.begin();it!=pendingDeletes_.end();){
        if(DeleteFileW(it->c_str())||GetLastError()==ERROR_FILE_NOT_FOUND)it=pendingDeletes_.erase(it);
        else ++it;
    }
    auto next=engine_.status();
    if(next==state_&&!testPending_)return;
    const auto previous=std::move(state_);state_=std::move(next);
    syncMedia();
    if(page_==Page::Media)InvalidateRect(content_,nullptr,FALSE);
    if(previous.devices.microphones!=state_.devices.microphones||previous.microphoneId!=state_.microphoneId)
        populate(input_,state_.devices.microphones,state_.microphoneId);
    if(previous.devices.listeners!=state_.devices.listeners||previous.listeningId!=state_.listeningId)
        populate(listener_,state_.devices.listeners,state_.listeningId);
    preferences_.microphone=state_.microphoneId;preferences_.listener=state_.listeningId;
    const bool testChanged=testPending_||previous.testActive!=state_.testActive||previous.capturing!=state_.capturing;
    testRequested_=state_.testActive;testPending_=false;if(testChanged)syncTest();
    const bool routeChanged=previous.routeMessage!=state_.routeMessage;
    const bool messageChanged=previous.testMessage!=state_.testMessage;
    if(routeChanged||messageChanged){
        ComPtr<IAccPropServices> accessibility;
        if(SUCCEEDED(CoCreateInstance(CLSID_AccPropServices,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&accessibility)))){
            if(routeChanged)accessibility->SetHwndPropStr(input_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_DESCRIPTION,state_.routeMessage.c_str());
            if(messageChanged)accessibility->SetHwndPropStr(test_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_DESCRIPTION,state_.testMessage.c_str());
        }
    }
    {LayoutBatch batch(1);batch.show(setup_,page_==Page::Microphone&&state_.devices.cables.empty());}
    if(previous.cableActive!=state_.cableActive||previous.paused!=state_.paused){
        RECT footer{0,px(height_-48),px(sidebar_),px(height_)};InvalidateRect(hwnd_,&footer,FALSE);
    }
    if(page_==Page::Microphone&&(testChanged||routeChanged||messageChanged||previous.cableActive!=state_.cableActive||previous.devices.cables!=state_.devices.cables)){
        RECT status{0,px(top(116)),px(contentWidth_),px(top(226))};InvalidateRect(content_,&status,FALSE);
        InvalidateRect(setup_,nullptr,FALSE);
    }
    if(page_==Page::Voice&&(testChanged||messageChanged)){
        RECT status{0,px(top(voiceSettingsTop())),px(contentWidth_),px(top(voiceSettingsTop()+80))};InvalidateRect(content_,&status,FALSE);
    }
    if(page_==Page::Soundboard){
        if(previous.playingClip!=state_.playingClip){
            for(size_t i=0;i<preferences_.clips.size();++i){
                const auto& file=preferences_.clips[i].file;
                if(file==previous.playingClip||file==state_.playingClip){
                    InvalidateRect(clipTiles_[i],nullptr,FALSE);InvalidateRect(clipSettings_[i],nullptr,FALSE);
                }
            }
        }
        if(previous.soundMessage!=state_.soundMessage||previous.paused!=state_.paused){
            const float y=std::max(280.f,80.f+float((clipTiles_.size()+gridColumns()-1)/gridColumns())*138.f)+8.f;
            RECT footer{0,px(top(y)),px(soundGridWidth()),px(top(y+42))};InvalidateRect(content_,&footer,FALSE);
        }
    }
    updateTimer();
}

void Window::syncTest(){const bool active=testPending_?testRequested_:state_.testActive;SetWindowTextW(test_,active?L"Stop Test":L"Test Microphone");EnableWindow(test_,!testPending_&&state_.capturing);
    if(hearMyself_){SendMessageW(hearMyself_,BM_SETCHECK,active?BST_CHECKED:BST_UNCHECKED,0);EnableWindow(hearMyself_,!testPending_&&state_.capturing);InvalidateRect(hearMyself_,nullptr,FALSE);}}
void Window::updateTimer(){
    const bool prepare=IsWindowVisible(hwnd_)&&!IsIconic(hwnd_)&&page_==Page::Soundboard;
    if(prepare!=soundboardPrepared_){soundboardPrepared_=prepare;engine_.setSoundboardVisible(prepare);}
    bool need=IsWindowVisible(hwnd_)&&!IsIconic(hwnd_)&&((page_==Page::Microphone&&state_.capturing&&state_.testActive)||(page_==Page::Media&&(state_.capturing||state_.mediaActive)));if(need!=timerActive_){if(need)SetTimer(hwnd_,meterTimer,50,nullptr);else KillTimer(hwnd_,meterTimer);timerActive_=need;}}
void Window::save(){SetTimer(hwnd_,saveTimer,400,nullptr);}
void Window::flushSave(){
    KillTimer(hwnd_,saveTimer);
    const bool clipsChanged=savedClips_!=preferences_.clips;
    preferences_.save(clipsChanged);if(clipsChanged)savedClips_=preferences_.clips;
}
void Window::setPage(Page page){if(page_==page)return;endShortcutCapture();stopScroll();commitClipName();if(emojiPopup_)DestroyWindow(emojiPopup_);page_=page;if(page==Page::Media)refreshApps();scroll_=scrollTarget_=0;layout();InvalidateRect(microphoneNav_,nullptr,FALSE);InvalidateRect(voiceNav_,nullptr,FALSE);InvalidateRect(soundNav_,nullptr,FALSE);InvalidateRect(mediaNav_,nullptr,FALSE);updateTimer();}
void Window::hide(){endShortcutCapture();commitClipName();if(emojiPopup_)DestroyWindow(emojiPopup_);engine_.setTest(false);testRequested_=false;ShowWindow(hwnd_,SW_HIDE);updateTimer();if(!preferences_.trayExplained){
    wcscpy_s(tray_.szInfoTitle,L"Gate is still running");wcscpy_s(tray_.szInfo,L"Your microphone keeps working. Open Gate, pause processing, or exit from the tray icon.");tray_.uFlags=NIF_INFO;tray_.dwInfoFlags=NIIF_INFO;Shell_NotifyIconW(NIM_MODIFY,&tray_);preferences_.trayExplained=true;save();}}
void Window::trayMenu(){POINT point{};GetCursorPos(&point);HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,openTray,L"Open Gate");AppendMenuW(menu,MF_STRING,pauseTray,paused_?L"Resume processing":L"Pause processing");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,exitTray,L"Exit");SetForegroundWindow(hwnd_);auto command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,hwnd_,nullptr);DestroyMenu(menu);if(command)SendMessageW(hwnd_,WM_COMMAND,command,0);PostMessageW(hwnd_,WM_NULL,0,0);}
LRESULT Window::message(UINT msg,WPARAM w,LPARAM l) {
    if(msg==RegisterWindowMessageW(L"TaskbarCreated")&&tray_.hWnd){tray_.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;Shell_NotifyIconW(NIM_ADD,&tray_);return 0;}
    switch(msg){
    case WM_CREATE:
        dpi_=GetDpiForWindow(hwnd_);theme_.connect(hwnd_);createControls();
        tray_.cbSize=sizeof(tray_);tray_.hWnd=hwnd_;tray_.uID=1;tray_.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray_.uCallbackMessage=trayMessage;tray_.hIcon=LoadIconW(instance_,MAKEINTRESOURCEW(101));wcscpy_s(tray_.szTip,L"Gate");Shell_NotifyIconW(NIM_ADD,&tray_);layout();return 0;
    case WM_SIZE:if(w!=SIZE_MINIMIZED)layout();else stopScroll();updateTimer();return 0;
    case WM_SHOWWINDOW:if(!w)stopScroll();updateTimer();break;
    case WM_ACTIVATEAPP:if(!w)endShortcutCapture();break;
    case WM_HOTKEY:{
        if(capturingVoiceShortcut_||capturingClipShortcut_>=0)return 0;
        const uint32_t chord=uint32_t(HIWORD(l)|(LOWORD(l)<<8));
        if(w==voiceHotkeyId&&chord==preferences_.voiceShortcut)toggleVoiceShortcut();
        else if(w>=clipHotkeyFirst&&w-clipHotkeyFirst<clipHotkeys_.size()){
            const size_t index=size_t(w-clipHotkeyFirst);
            if(clipHotkeys_[index]->key.active()&&chord==preferences_.clips[index].shortcut)triggerClip(index);
        }
        return 0;
    }
    case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={px(800),px(530)};return 0;
    case WM_DPICHANGED:{dpi_=HIWORD(w);auto* r=reinterpret_cast<RECT*>(l);SetWindowPos(hwnd_,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);updateTheme();layout();return 0;}
    case WM_SETTINGCHANGE:case WM_THEMECHANGED:case themeChanged:updateTheme();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:paint();return 0;
    case WM_DRAWITEM:drawItem(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;
    case WM_MEASUREITEM:reinterpret_cast<MEASUREITEMSTRUCT*>(l)->itemHeight=px(32);return TRUE;
    case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:SetTextColor(reinterpret_cast<HDC>(w),rgb(theme_.colors().text));SetBkColor(reinterpret_cast<HDC>(w),rgb(theme_.colors().field));return reinterpret_cast<LRESULT>(fieldBrush_);
    case WM_MOUSEWHEEL:{
        UINT lines=3;SystemParametersInfoW(SPI_GETWHEELSCROLLLINES,0,&lines,0);
        const float distance=lines==WHEEL_PAGESCROLL?viewportHeight_-40:float(lines)*16;
        scrollTo(scrollTarget_-float(GET_WHEEL_DELTA_WPARAM(w))/WHEEL_DELTA*distance);return 0;
    }
    case WM_LBUTTONDOWN:
        if(maxScroll()>0&&GET_X_LPARAM(l)>=px(width_-22)&&GET_Y_LPARAM(l)>=px(headerHeight)){
            const float thumb=std::max(32.f,viewportHeight_*viewportHeight_/pageHeight());
            const float y=headerHeight+(viewportHeight_-thumb)*scroll_/maxScroll();
            const float mouse=GET_Y_LPARAM(l)/scale();
            scrollGrab_=mouse>=y&&mouse<=y+thumb?mouse-y:thumb/2;
            draggingScroll_=true;SetCapture(hwnd_);
            scrollTo((mouse-headerHeight-scrollGrab_)/(viewportHeight_-thumb)*maxScroll(),false);return 0;
        }break;
    case WM_MOUSEMOVE:
        if(draggingScroll_){const float thumb=std::max(32.f,viewportHeight_*viewportHeight_/pageHeight());
            scrollTo((GET_Y_LPARAM(l)/scale()-headerHeight-scrollGrab_)/(viewportHeight_-thumb)*maxScroll(),false);return 0;}break;
    case WM_LBUTTONUP:if(draggingScroll_){draggingScroll_=false;ReleaseCapture();InvalidateRect(hwnd_,nullptr,FALSE);return 0;}break;
    case WM_CAPTURECHANGED:draggingScroll_=false;break;
    case WM_VSCROLL:
        if(isMediaFader(reinterpret_cast<HWND>(l))){
            const auto value=unsigned(SendMessageW(reinterpret_cast<HWND>(l),TBM_GETPOS,0,0));
            if(reinterpret_cast<HWND>(l)==mediaVolume_)mediaVolumePercent_=value;else mediaMicPercent_=value;
            applyMixLevels();return 0;
        }break;
    case WM_HSCROLL:
        if(featureSlider(reinterpret_cast<HWND>(l)))return 0;
        if(reinterpret_cast<HWND>(l)==strength_)preferences_.processing.strength=unsigned(SendMessageW(strength_,TBM_GETPOS,0,0));
        if(reinterpret_cast<HWND>(l)==threshold_){
            preferences_.processing.thresholdDb=gateThresholdFromPercent(unsigned(SendMessageW(threshold_,TBM_GETPOS,0,0)));
        }
        engine_.setParameters(preferences_.processing);save();
        invalidateSliderValue(reinterpret_cast<HWND>(l));return 0;
    case WM_TIMER:
        if(w==saveTimer)flushSave();
        else if(w==3)animateScroll();
        else if(w==meterTimer){if(page_==Page::Media)invalidateMediaMeters();else {RECT r{px(198),px(top(120)),px(contentWidth_),px(top(154))};InvalidateRect(content_,&r,FALSE);}}return 0;
    case audioChanged:refreshStatus();return 0;
    case WM_COMMAND:
        if(mediaCommand(LOWORD(w),HIWORD(w))||featureCommand(LOWORD(w),HIWORD(w)))return 0;
        switch(LOWORD(w)){
        case MicNav:setPage(Page::Microphone);break;
        case VoiceNav:setPage(Page::Voice);break;
        case Input:if(HIWORD(w)==CBN_SELCHANGE){int index=int(SendMessageW(input_,CB_GETCURSEL,0,0));if(index>=0&&size_t(index)<state_.devices.microphones.size()){preferences_.microphone=state_.devices.microphones[index].id;engine_.selectMicrophone(preferences_.microphone);save();}}break;
        case Listener:if(HIWORD(w)==CBN_SELCHANGE){int index=int(SendMessageW(listener_,CB_GETCURSEL,0,0));if(index>=0&&size_t(index)<state_.devices.listeners.size()){preferences_.listener=state_.devices.listeners[index].id;engine_.selectListener(preferences_.listener);save();}}break;
        case Suppression:case GateToggle:
            preferences_.processing.suppression=SendMessageW(suppression_,BM_GETCHECK,0,0)==BST_CHECKED;preferences_.processing.gate=SendMessageW(gate_,BM_GETCHECK,0,0)==BST_CHECKED;
            engine_.setParameters(preferences_.processing);EnableWindow(strength_,preferences_.processing.suppression);EnableWindow(threshold_,preferences_.processing.gate);save();InvalidateRect(content_,nullptr,FALSE);break;
        case Test:testRequested_=!testRequested_;testPending_=true;engine_.setTest(testRequested_);syncTest();break;
        case Setup:ShellExecuteW(hwnd_,L"open",L"https://vb-audio.com/Cable/",nullptr,nullptr,SW_SHOWNORMAL);break;
        case openTray:ShowWindow(hwnd_,SW_RESTORE);SetForegroundWindow(hwnd_);updateTimer();break;
        case pauseTray:paused_=!paused_;engine_.setPaused(paused_);break;
        case exitTray:DestroyWindow(hwnd_);break;
        }return 0;
    case trayMessage:if(l==WM_LBUTTONUP||l==WM_LBUTTONDBLCLK){ShowWindow(hwnd_,SW_RESTORE);SetForegroundWindow(hwnd_);updateTimer();}else if(l==WM_RBUTTONUP)trayMenu();return 0;
    case WM_POWERBROADCAST:if(w==PBT_APMSUSPEND)engine_.setSuspended(true);else if(w==PBT_APMRESUMEAUTOMATIC)engine_.setSuspended(false);return TRUE;
    case WM_CLOSE:hide();return 0;
    case WM_QUERYENDSESSION:return TRUE;
    case WM_ENDSESSION:if(w)DestroyWindow(hwnd_);return 0;
    case WM_DESTROY:capturingVoiceShortcut_=false;capturingClipShortcut_=-1;voiceHotkey_.clear();clipHotkeys_.clear();stopScroll();if(scrollClock_){CloseHandle(scrollClock_);scrollClock_=nullptr;}commitClipName();engine_.stop();for(const auto& path:pendingDeletes_)DeleteFileW(path.c_str());flushSave();Shell_NotifyIconW(NIM_DELETE,&tray_);if(font_)DeleteObject(font_);if(boldFont_)DeleteObject(boldFont_);if(fieldBrush_)DeleteObject(fieldBrush_);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd_,msg,w,l);
}
}
