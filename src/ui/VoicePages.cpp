#include "ui/Window.h"
#include <commdlg.h>
#include <uxtheme.h>
#include <filesystem>
#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <oleacc.h>

namespace gate {
namespace {
enum FeatureId { SoundNav=400, EffectToggle, Intensity, HearMyself,
    Import, StopSounds, HearSounds, ClipName, ClipEmoji, RemoveClip, CloseClip, VoiceFirst=500, ClipFirst=1000, SettingsFirst=20000 };
}
bool Window::isSlider(HWND h) const {return h==strength_||h==threshold_||h==intensity_;}
bool Window::isToggle(HWND h) const {return h==suppression_||h==gate_||h==effectToggle_||h==hearSounds_||h==hearMyself_;}
bool Window::isTile(HWND h) const {return std::find(voiceTiles_.begin(),voiceTiles_.end(),h)!=voiceTiles_.end()||std::find(clipTiles_.begin(),clipTiles_.end(),h)!=clipTiles_.end();}
bool Window::tileSelected(HWND h) const {
    for(unsigned i=0;i<6;++i)if(voiceTiles_[i]==h)return unsigned(preferences_.voice.preset)==i;
    for(size_t i=0;i<clipTiles_.size();++i)if(clipTiles_[i]==h)return selectedClip_==int(i);
    return false;
}
bool Window::sideInspector() const {return selectedClip_>=0&&contentWidth_>=700.f;}
float Window::soundGridWidth() const {return sideInspector()?contentWidth_-264.f:contentWidth_;}
unsigned Window::voiceColumns() const {return contentWidth_>=600.f?3u:2u;}
float Window::voiceSettingsTop() const {return 28.f+float((6+voiceColumns()-1)/voiceColumns())*138.f;}
unsigned Window::gridColumns() const {return soundGridWidth()>=510.f?3:2;}
float Window::clipEditorTop() const {
    const float rows=float((clipTiles_.size()+gridColumns()-1)/gridColumns());
    return sideInspector()?80.f:std::max(280.f,80.f+rows*138.f)+60.f;
}
float Window::pageHeight() const {
    if(page_==Page::Microphone)return contentHeight;
    if(page_==Page::Voice)return voiceSettingsTop()+180.f;
    const float bottom=std::max(280.f,80.f+float((clipTiles_.size()+gridColumns()-1)/gridColumns())*138.f);
    return selectedClip_<0?bottom+60.f:sideInspector()?std::max(350.f,bottom+60.f):clipEditorTop()+200.f;
}
void Window::createVoicePages(){
    auto make=[&](const wchar_t* cls,const wchar_t* name,DWORD style,unsigned id,bool sound){
        HWND h=CreateWindowExW(0,cls,name,WS_CHILD|WS_TABSTOP|style,0,0,1,1,content_,reinterpret_cast<HMENU>(UINT_PTR(id)),instance_,nullptr);
        (sound?soundControls_:voiceControls_).push_back(h);
        if(std::wstring(cls)!=WC_EDITW)SetWindowSubclass(h,controlProcedure,1,reinterpret_cast<DWORD_PTR>(this));
        return h;
    };
    auto button=[&](const wchar_t* name,unsigned id,bool sound){return make(WC_BUTTONW,name,BS_OWNERDRAW,id,sound);};
    auto slider=[&](const wchar_t* name,unsigned id,unsigned value,bool sound){auto h=make(TRACKBAR_CLASSW,name,TBS_NOTICKS,id,sound);SendMessageW(h,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(h,TBM_SETPOS,TRUE,value);return h;};
    soundNav_=CreateWindowExW(0,WC_BUTTONW,L"Soundboard",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,1,1,hwnd_,reinterpret_cast<HMENU>(SoundNav),instance_,nullptr);
    SetWindowSubclass(soundNav_,controlProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    effectToggle_=make(WC_BUTTONW,L"Voice effects enabled",BS_AUTOCHECKBOX,EffectToggle,false);
    SendMessageW(effectToggle_,BM_SETCHECK,preferences_.voice.enabled?BST_CHECKED:BST_UNCHECKED,0);
    hearMyself_=make(WC_BUTTONW,L"Hear Myself",BS_AUTOCHECKBOX,HearMyself,false);
    for(unsigned i=0;i<6;++i)voiceTiles_[i]=button(voiceNames[i],VoiceFirst+i,false);
    intensity_=slider(L"Voice effect intensity, percent",Intensity,preferences_.voice.intensity,false);
    import_=button(L"Import Sound",Import,true);stopSounds_=button(L"Stop All",StopSounds,true);
    hearSounds_=make(WC_BUTTONW,L"Hear soundboard clips",BS_AUTOCHECKBOX,HearSounds,true);
    SendMessageW(hearSounds_,BM_SETCHECK,preferences_.hearSounds?BST_CHECKED:BST_UNCHECKED,0);
    clipName_=make(WC_EDITW,L"",ES_AUTOHSCROLL,ClipName,true);SendMessageW(clipName_,EM_SETLIMITTEXT,100,0);
    clipEmoji_=button(L"Change emoji",ClipEmoji,true);closeClip_=button(L"Done",CloseClip,true);removeClip_=button(L"Remove Sound",RemoveClip,true);
    SetWindowSubclass(clipName_,editorProcedure,2,reinterpret_cast<DWORD_PTR>(this));
    ComPtr<IAccPropServices> accessibility;
    if(SUCCEEDED(CoCreateInstance(CLSID_AccPropServices,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&accessibility)))){
        accessibility->SetHwndPropStr(clipName_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_NAME,L"Sound name");
    }
    rebuildClips();
}
void Window::updateFeatureTheme(){
    auto apply=[&](HWND h){SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);};
    apply(soundNav_);for(auto h:voiceControls_)apply(h);for(auto h:soundControls_)apply(h);for(auto h:clipTiles_)apply(h);for(auto h:clipSettings_)apply(h);
    SendMessageW(clipName_,WM_SETFONT,reinterpret_cast<WPARAM>(boldFont_),TRUE);
    SetWindowTheme(clipName_,theme_.colors().dark?L"DarkMode_Explorer":L"Explorer",nullptr);
}
void Window::rebuildClips(){
    for(auto h:clipSettings_)DestroyWindow(h);clipSettings_.clear();
    for(auto h:clipTiles_)DestroyWindow(h);clipTiles_.clear();hoverControl_=nullptr;
    for(size_t i=0;i<preferences_.clips.size();++i){
        auto h=CreateWindowExW(0,WC_BUTTONW,preferences_.clips[i].name.c_str(),WS_CHILD|WS_TABSTOP|WS_CLIPSIBLINGS|BS_OWNERDRAW,0,0,1,1,content_,reinterpret_cast<HMENU>(UINT_PTR(ClipFirst+i)),instance_,nullptr);
        SetWindowSubclass(h,controlProcedure,1,reinterpret_cast<DWORD_PTR>(this));SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);clipTiles_.push_back(h);
        const auto title=L"Settings for "+preferences_.clips[i].name;
        auto settings=CreateWindowExW(0,WC_BUTTONW,title.c_str(),WS_CHILD|WS_TABSTOP|WS_CLIPSIBLINGS|BS_OWNERDRAW,0,0,1,1,content_,reinterpret_cast<HMENU>(UINT_PTR(SettingsFirst+i)),instance_,nullptr);
        SetWindowSubclass(settings,controlProcedure,1,reinterpret_cast<DWORD_PTR>(this));clipSettings_.push_back(settings);
        SetWindowPos(settings,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOREDRAW);
    }
    if(selectedClip_>=int(clipTiles_.size()))selectedClip_=clipTiles_.empty()?-1:0;
    if(selectedClip_>=0)SetWindowTextW(clipName_,preferences_.clips[size_t(selectedClip_)].name.c_str());
}
void Window::selectClip(int index){
    commitClipName();
    selectedClip_=index;
    if(index>=0&&size_t(index)<preferences_.clips.size()){
        const auto& c=preferences_.clips[size_t(index)];SetWindowTextW(clipName_,c.name.c_str());
    }
}
void Window::layoutVoicePages(LayoutBatch& batch){
    auto pos=[&](HWND h,float x,float y,float w,float height){batch.move(h,px(x),px(top(y)),px(w),px(height));};
    const float span=contentWidth_;
    if(page_==Page::Voice){
        const unsigned columns=voiceColumns();const float tile=(span-14.f*float(columns-1))/float(columns);
        for(unsigned i=0;i<6;++i)pos(voiceTiles_[i],float(i%columns)*(tile+14),12+float(i/columns)*138,tile,124);
        const float y=voiceSettingsTop(),half=(span-32)/2;
        pos(effectToggle_,half-48,y,46,28);pos(hearMyself_,span-48,y,46,28);
        pos(intensity_,0,y+132,span,30);
    }else{
        const float grid=soundGridWidth();const unsigned columns=gridColumns();const float tile=(grid-14*float(columns-1))/float(columns);
        pos(import_,0,8,158,38);pos(stopSounds_,170,8,114,38);pos(hearSounds_,span-48,13,46,28);
        for(size_t i=0;i<clipTiles_.size();++i){
            const float x=float(i%columns)*(tile+14),y=80+float(i/columns)*138;
            pos(clipTiles_[i],x,y,tile,124);pos(clipSettings_[i],x+tile-42,y+6,34,34);
        }
        const bool selected=selectedClip_>=0;
        const float x=sideInspector()?grid+32:0,y=clipEditorTop(),w=span-x;
        if(selected){
            pos(clipEmoji_,x,y+6,42,42);pos(clipName_,x+52,y+12,w-52,30);
            pos(removeClip_,x,y+120,w,36);pos(closeClip_,x,y+164,w,32);
        }
    }
}
void Window::paintVoicePages(ID2D1RenderTarget* target){
    const auto& p=theme_.colors();const float span=contentWidth_;
    ComPtr<ID2D1SolidColorBrush> brush;target->CreateSolidColorBrush(p.border,&brush);
    auto line=[&](float x,float y,float width){target->DrawLine({x,top(y)},{x+width,top(y)},brush.Get(),1);};
    auto text=[&](const std::wstring& s,float x,float y,float w,float height=26,float size=14,bool bold=false){label(target,s,{x,top(y),x+w,top(y+height)},size,p.text,bold);};
    auto secondary=[&](const std::wstring& s,float x,float y,float w,float height=42){label(target,s,{x,top(y),x+w,top(y+height)},12.5f,p.secondary,false,DWRITE_TEXT_ALIGNMENT_LEADING,true);};
    if(page_==Page::Voice){
        const float y=voiceSettingsTop(),half=(span-32)/2,right=half+32;
        text(L"Voice Effects",0,y,half-64,28,15,true);
        secondary(L"Apply the selected effect to your microphone.",0,y+38,half,38);
        text(L"Hear Myself",right,y,half-64,28,15,true);
        secondary(state_.testMessage.empty()?L"Listen to your voice with the effect applied.":state_.testMessage,right,y+38,half,38);
        line(0,y+88,span);
        text(L"Intensity",0,y+102,span-65,26,15,true);
        label(target,std::to_wstring(preferences_.voice.intensity)+L"%",{span-60,top(y+102),span,top(y+128)},14,p.text,false,DWRITE_TEXT_ALIGNMENT_TRAILING);
    }else{
        const float grid=soundGridWidth();
        text(L"Hear Sounds",span-164,14,108,26,13);
        line(0,62,span);
        if(clipTiles_.empty()){
            drawIcon(target,6,grid/2-24,top(130),48,p.secondary);
            label(target,L"Your soundboard starts here",{0,top(192),grid,top(222)},18,p.text,true,DWRITE_TEXT_ALIGNMENT_CENTER);
            label(target,L"Import a WAV or MP3 file to add a sound.",{0,top(233),grid,top(255)},12,p.secondary,false,DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        const float x=sideInspector()?grid+32:0,y=clipEditorTop(),w=span-x;
        if(sideInspector())target->DrawLine({grid+16,top(80)},{grid+16,top(340)},brush.Get(),1);
        if(selectedClip_>=0){
            line(x,y+64,w);
            secondary(L"Click the name to rename. Click the emoji to change it.",x,y+76,w,40);
        }
        const float footer=std::max(280.f,80.f+float((clipTiles_.size()+gridColumns()-1)/gridColumns())*138.f)+8.f;
        std::wstring status=state_.soundMessage.empty()?importMessage_:state_.soundMessage;
        if(state_.paused)status=L"Playback paused. Resume Gate from the tray.";
        if(status.empty())status=L"Click a sound to play. Click again to stop.";
        secondary(status,0,footer,grid,42);
    }
}
void Window::importClip(){
    wchar_t file[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=hwnd_;
    dialog.lpstrFilter=L"Audio files (*.wav;*.mp3)\0*.wav;*.mp3\0\0";dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;dialog.lpstrTitle=L"Import a sound";
    if(!GetOpenFileNameW(&dialog))return;
    try{
        const std::filesystem::path source(file);auto ext=source.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),[](wchar_t c){return wchar_t(towlower(c));});
        if(ext!=L".wav"&&ext!=L".mp3")throw std::runtime_error("format");
        const auto folder=Preferences::soundDirectory();if(folder.empty())throw std::runtime_error("folder");std::filesystem::create_directories(folder);
        GUID id{};check(CoCreateGuid(&id));wchar_t guid[40]{};StringFromGUID2(id,guid,40);
        SoundClip clip;clip.file=std::wstring(guid)+ext;clip.name=source.stem().wstring().substr(0,100);
        std::filesystem::copy_file(source,Preferences::clipPath(clip.file));
        commitClipName();preferences_.clips.push_back(std::move(clip));selectedClip_=-1;
        importMessage_=L"Sound imported. Click its tile to play.";rebuildClips();save();layout();
        const float tileY=80.f+float((preferences_.clips.size()-1)/gridColumns())*138.f;
        scrollTo(std::max(0.f,tileY+138.f-viewportHeight_),false);
    }catch(...){importMessage_=L"Could not import this sound. Choose an accessible WAV or MP3 file.";InvalidateRect(content_,nullptr,FALSE);}
}
bool Window::featureSlider(HWND control){
    if(control==intensity_){preferences_.voice.intensity=unsigned(SendMessageW(control,TBM_GETPOS,0,0));engine_.setVoice(preferences_.voice);}
    else return false;
    save();invalidateSliderValue(control);return true;
}
bool Window::featureCommand(unsigned id,unsigned notification){
    if(id==ClipName){if(notification==EN_KILLFOCUS)commitClipName();return true;}
    if(notification!=BN_CLICKED)return false;
    if(id>=VoiceFirst&&id<VoiceFirst+6){
        const auto previous=unsigned(preferences_.voice.preset);
        preferences_.voice.preset=VoicePreset(id-VoiceFirst);
        engine_.setVoice(preferences_.voice);
        InvalidateRect(voiceTiles_[previous],nullptr,FALSE);InvalidateRect(voiceTiles_[id-VoiceFirst],nullptr,FALSE);
        save();return true;
    }
    if(id>=SettingsFirst&&id-SettingsFirst<preferences_.clips.size()){
        selectClip(int(id-SettingsFirst));layout();
        if(!sideInspector())scrollTo(clipEditorTop(),false);
        SetFocus(clipName_);SendMessageW(clipName_,EM_SETSEL,0,-1);return true;
    }
    if(id>=ClipFirst&&id<SettingsFirst&&id-ClipFirst<preferences_.clips.size()){
        const auto& c=preferences_.clips[size_t(id-ClipFirst)];
        if(!importMessage_.empty()){importMessage_.clear();InvalidateRect(content_,nullptr,FALSE);}
        engine_.playClip(c.file,Preferences::clipPath(c.file));
        if(selectedClip_>=0){
            if(emojiPopup_)DestroyWindow(emojiPopup_);
            selectClip(-1);layout();
        }
        return true;
    }
    switch(id){
    case SoundNav:setPage(Page::Soundboard);return true;
    case EffectToggle:preferences_.voice.enabled=SendMessageW(effectToggle_,BM_GETCHECK,0,0)==BST_CHECKED;engine_.setVoice(preferences_.voice);save();return true;
    case HearMyself:testRequested_=!testRequested_;testPending_=true;engine_.setTest(testRequested_);syncTest();return true;
    case Import:importClip();return true;
    case StopSounds:engine_.stopClips();return true;
    case HearSounds:preferences_.hearSounds=SendMessageW(hearSounds_,BM_GETCHECK,0,0)==BST_CHECKED;engine_.setHearSounds(preferences_.hearSounds);save();return true;
    case ClipEmoji:openEmojiPicker();return true;
    case CloseClip:selectClip(-1);break;
    case RemoveClip:if(selectedClip_>=0){
        auto file=preferences_.clips[size_t(selectedClip_)].file;engine_.stopClips();
        preferences_.clips.erase(preferences_.clips.begin()+selectedClip_);selectedClip_=-1;rebuildClips();
        // The decoder can still hold the file until its stop command is applied.
        const auto path=Preferences::clipPath(file);if(!path.empty()&&!DeleteFileW(path.c_str()))pendingDeletes_.push_back(path);

    }break;
    default:return false;
    }
    save();layout();return true;
}
}
