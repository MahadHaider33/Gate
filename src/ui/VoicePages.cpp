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
    Import, StopSounds, HearSounds, ClipName, ClipEmoji, RemoveClip, CloseClip, CustomReset, CustomBack, VoiceShortcut, ClearVoiceShortcut, ClipShortcut, ClearClipShortcut, VoiceFirst=500, CustomFirst=600,
    ConfirmVoiceShortcut=700,CancelVoiceShortcut,ConfirmClipShortcut,CancelClipShortcut,ClipFirst=1000,SettingsFirst=20000 };
constexpr float shortcutHeight=88.f;
std::wstring shortcutName(uint32_t value){
    if(!value)return L"Set shortcut";
    std::wstring text;const unsigned modifiers=value>>8,key=value&255;
    if(modifiers&MOD_CONTROL)text+=L"Ctrl + ";
    if(modifiers&MOD_ALT)text+=L"Alt + ";
    if(modifiers&MOD_SHIFT)text+=L"Shift + ";
    if(modifiers&MOD_WIN)text+=L"Win + ";
    LONG scan=LONG(MapVirtualKeyW(key,MAPVK_VK_TO_VSC)<<16);
    if(key==VK_INSERT||key==VK_DELETE||key==VK_HOME||key==VK_END||key==VK_PRIOR||key==VK_NEXT||
       key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN||key==VK_DIVIDE||key==VK_NUMLOCK)scan|=1<<24;
    wchar_t name[64]{};
    if(GetKeyNameTextW(scan,name,64))text+=name;
    else text+=L"Key "+std::to_wstring(key);
    return text;
}
std::wstring customValue(unsigned index,int value){
    const auto unit=voiceControlInfo[index].unit;
    if(unit==VoiceUnit::Semitones||unit==VoiceUnit::TenthsHertz){
        const int magnitude=std::abs(value);
        return std::wstring(value<0?L"-":value>0&&unit==VoiceUnit::Semitones?L"+":L"")+std::to_wstring(magnitude/10)+L"."+std::to_wstring(magnitude%10)+(unit==VoiceUnit::Semitones?L" st":L" Hz");
    }
    return std::wstring(unit==VoiceUnit::Decibels&&value>0?L"+":L"")+std::to_wstring(value)+(unit==VoiceUnit::Decibels?L" dB":unit==VoiceUnit::Hertz?L" Hz":unit==VoiceUnit::Milliseconds?L" ms":L"%");
}
}
bool Window::isSlider(HWND h) const {return h==strength_||h==threshold_||h==intensity_||isMediaFader(h)||std::find(customSliders_.begin(),customSliders_.end(),h)!=customSliders_.end();}
bool Window::isToggle(HWND h) const {return h==suppression_||h==gate_||h==effectToggle_||h==hearSounds_||h==hearMyself_;}
bool Window::isTile(HWND h) const {return std::find(voiceTiles_.begin(),voiceTiles_.end(),h)!=voiceTiles_.end()||std::find(clipTiles_.begin(),clipTiles_.end(),h)!=clipTiles_.end();}
bool Window::tileSelected(HWND h) const {
    for(unsigned i=0;i<voiceCount;++i)if(voiceTiles_[i]==h)return unsigned(preferences_.voice.preset)==i;
    for(size_t i=0;i<clipTiles_.size();++i)if(clipTiles_[i]==h)return selectedClip_==int(i);
    return false;
}
bool Window::sideInspector() const {return selectedClip_>=0&&contentWidth_>=700.f;}
float Window::soundGridWidth() const {return sideInspector()?contentWidth_-264.f:contentWidth_;}
unsigned Window::voiceColumns() const {return contentWidth_>=760.f?4u:contentWidth_>=540.f?3u:2u;}
float Window::voiceSettingsTop() const {return 12.f;}
float Window::customControlTop(unsigned index) const {return 202.f+shortcutHeight+shortcutExtraHeight(true)+float(index/4)*190.f+float((index%4)/2)*74.f;}
unsigned Window::gridColumns() const {return soundGridWidth()>=510.f?3:2;}
float Window::clipEditorTop() const {
    const float rows=float((clipTiles_.size()+gridColumns()-1)/gridColumns());
    return sideInspector()?80.f:std::max(280.f,80.f+rows*138.f)+60.f;
}
float Window::pageHeight() const {
    if(page_==Page::Media)return 620.f;
    if(page_==Page::Microphone)return contentHeight;
    if(page_==Page::Voice)return customEditor_?customControlTop(voiceControlCount-1)+86.f:216.f+shortcutHeight+shortcutExtraHeight(true)+float((voiceCount+voiceColumns()-1)/voiceColumns())*138.f;
    const float bottom=std::max(280.f,80.f+float((clipTiles_.size()+gridColumns()-1)/gridColumns())*138.f);
    return selectedClip_<0?bottom+60.f:sideInspector()?std::max(440.f+shortcutExtraHeight(false),bottom+60.f):clipEditorTop()+352.f+shortcutExtraHeight(false);
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
    voiceShortcut_=button(L"Set shortcut",VoiceShortcut,false);
    clearVoiceShortcut_=button(L"Clear",ClearVoiceShortcut,false);
    voiceShortcutConfirm_=button(L"Use shortcut",ConfirmVoiceShortcut,false);voiceShortcutCancel_=button(L"Cancel",CancelVoiceShortcut,false);
    for(unsigned i=0;i<voiceCount;++i)voiceTiles_[i]=button(voiceNames[i],VoiceFirst+i,false);
    intensity_=slider(L"Voice effect intensity, percent",Intensity,preferences_.voice.intensity,false);
    for(unsigned i=0;i<voiceControlCount;++i){
        const auto& info=voiceControlInfo[i];
        customSliders_[i]=make(TRACKBAR_CLASSW,info.name,TBS_NOTICKS,CustomFirst+i,false);
        SendMessageW(customSliders_[i],TBM_SETRANGE,TRUE,MAKELPARAM(info.min,info.max));
        SendMessageW(customSliders_[i],TBM_SETPOS,TRUE,preferences_.voice.custom.values[i]);
    }
    customReset_=button(L"Reset Custom",CustomReset,false);customBack_=button(L"Back to voices",CustomBack,false);
    import_=button(L"Import Sound",Import,true);stopSounds_=button(L"Stop All",StopSounds,true);
    hearSounds_=make(WC_BUTTONW,L"Hear soundboard clips",BS_AUTOCHECKBOX,HearSounds,true);
    SendMessageW(hearSounds_,BM_SETCHECK,preferences_.hearSounds?BST_CHECKED:BST_UNCHECKED,0);
    clipName_=make(WC_EDITW,L"",ES_AUTOHSCROLL,ClipName,true);SendMessageW(clipName_,EM_SETLIMITTEXT,100,0);
    clipEmoji_=button(L"Change emoji",ClipEmoji,true);closeClip_=button(L"Done",CloseClip,true);removeClip_=button(L"Remove Sound",RemoveClip,true);
    clipShortcut_=button(L"Set shortcut",ClipShortcut,true);clearClipShortcut_=button(L"Clear",ClearClipShortcut,true);
    clipShortcutConfirm_=button(L"Use shortcut",ConfirmClipShortcut,true);clipShortcutCancel_=button(L"Cancel",CancelClipShortcut,true);
    SetWindowSubclass(clipName_,editorProcedure,2,reinterpret_cast<DWORD_PTR>(this));
    ComPtr<IAccPropServices> accessibility;
    if(SUCCEEDED(CoCreateInstance(CLSID_AccPropServices,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&accessibility)))){
        accessibility->SetHwndPropStr(clipName_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_NAME,L"Sound name");
        accessibility->SetHwndPropStr(voiceShortcut_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_DESCRIPTION,L"Change the global shortcut for Normal and your last selected voice. Use the Cancel button to cancel, or Clear to remove it.");
        accessibility->SetHwndPropStr(clipShortcut_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_DESCRIPTION,L"Set a global shortcut to play or stop this sound, including while Gate is in the tray. Use Cancel to cancel, or Clear to remove it.");
    }
    registerVoiceShortcut();
    rebuildClips();
}
void Window::updateVoiceShortcut(){
    const auto text=capturingVoiceShortcut_?(shortcutPrompt_==ShortcutPrompt::None?std::wstring(L"Press your keys..."):shortcutName(pendingShortcut_)):shortcutName(preferences_.voiceShortcut);
    SetWindowTextW(voiceShortcut_,text.c_str());
    EnableWindow(clearVoiceShortcut_,preferences_.voiceShortcut!=0);
    InvalidateRect(voiceShortcut_,nullptr,FALSE);
    if(page_==Page::Voice){RECT r{0,px(top(108)),px(contentWidth_),px(top(186))};InvalidateRect(content_,&r,FALSE);}
}
void Window::registerVoiceShortcut(){
    voiceShortcutError_.clear();
    if(!voiceHotkey_.set(hwnd_,voiceHotkeyId,preferences_.voiceShortcut))
        voiceShortcutError_=L"Shortcut unavailable or in use. Choose another; this one is inactive.";
    updateVoiceShortcut();
}
void Window::endShortcutCapture(bool restore){
    const bool voice=capturingVoiceShortcut_;const int clip=capturingClipShortcut_;
    if(!voice&&clip<0)return;
    const HWND origin=voice?voiceShortcut_:clipShortcut_;
    const HWND focus=GetFocus();
    capturingVoiceShortcut_=false;capturingClipShortcut_=-1;
    shortcutPrompt_=ShortcutPrompt::None;pendingShortcut_=0;pendingShortcutOwner_=approvedShortcutOwner_=-2;typingShortcutApproved_=false;
    if(restore){
        if(voice)registerVoiceShortcut();
        if(clip>=0&&size_t(clip)<clipHotkeys_.size())registerClipShortcut(size_t(clip));
    }
    if(focus==voiceShortcutConfirm_||focus==voiceShortcutCancel_||focus==clipShortcutConfirm_||focus==clipShortcutCancel_)SetFocus(origin);
    refreshShortcutEditor();
}
std::wstring Window::shortcutPromptText() const {
    if(shortcutPrompt_==ShortcutPrompt::TypingKey)
        return L"Using "+shortcutName(pendingShortcut_)+L" alone will trigger this action while typing in other apps. Use this shortcut?";
    if(shortcutPrompt_==ShortcutPrompt::Replace){
        const std::wstring owner=pendingShortcutOwner_==-1?L"Voice Changer":L"Soundboard: "+preferences_.clips[size_t(pendingShortcutOwner_)].name;
        return shortcutName(pendingShortcut_)+L" is used by "+owner+L". Remove that shortcut and use it here?";
    }
    const auto& error=capturingVoiceShortcut_?voiceShortcutError_:clipHotkeys_[size_t(capturingClipShortcut_)]->error;
    return error.empty()?L"Press your keys, or choose Cancel.":error;
}
void Window::measureShortcutMessage(){
    if(!capturingVoiceShortcut_&&capturingClipShortcut_<0){shortcutMessageHeight_=0;return;}
    const float width=capturingVoiceShortcut_?contentWidth_-132:contentWidth_-(sideInspector()?soundGridWidth()+32:0);
    const auto text=shortcutPromptText();
    if(text==shortcutMeasuredText_&&width==shortcutMeasuredWidth_&&shortcutMessageHeight_>0)return;
    auto& format=textFormats_[25][0];
    if(!format)check(textFactory_->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,12.5f,L"en-us",&format));
    ComPtr<IDWriteTextLayout> textLayout;
    check(textFactory_->CreateTextLayout(text.c_str(),UINT32(text.size()),format.Get(),std::max(1.f,width),1000,&textLayout));
    textLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    DWRITE_TEXT_METRICS metrics{};check(textLayout->GetMetrics(&metrics));
    shortcutMessageHeight_=std::ceil(metrics.height)+2;shortcutMeasuredText_=text;shortcutMeasuredWidth_=width;
}
float Window::shortcutExtraHeight(bool voice) const {
    if(voice?!capturingVoiceShortcut_:capturingClipShortcut_<0)return 0;
    return 38.f+std::max(0.f,shortcutMessageHeight_-(voice?36.f:64.f));
}
void Window::refreshShortcutEditor(){
    updateVoiceShortcut();updateClipShortcut();
    const wchar_t* action=shortcutPrompt_==ShortcutPrompt::Replace?L"Replace":L"Use shortcut";
    SetWindowTextW(voiceShortcutConfirm_,action);SetWindowTextW(clipShortcutConfirm_,action);
    if(shortcutPrompt_!=ShortcutPrompt::None){
        ComPtr<IAccPropServices> accessibility;
        if(SUCCEEDED(CoCreateInstance(CLSID_AccPropServices,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&accessibility))))
            accessibility->SetHwndPropStr(capturingVoiceShortcut_?voiceShortcutConfirm_:clipShortcutConfirm_,DWORD(OBJID_CLIENT),CHILDID_SELF,PROPID_ACC_DESCRIPTION,shortcutPromptText().c_str());
    }
    layout();
}
bool Window::acceptShortcut(uint32_t value){
    pendingShortcut_=value;typingShortcutApproved_=false;approvedShortcutOwner_=-2;
    advanceShortcut();return true;
}
void Window::confirmShortcut(){
    if(shortcutPrompt_==ShortcutPrompt::TypingKey)typingShortcutApproved_=true;
    else if(shortcutPrompt_==ShortcutPrompt::Replace)approvedShortcutOwner_=pendingShortcutOwner_;
    else return;
    advanceShortcut();
}
void Window::advanceShortcut(){
    const bool voice=capturingVoiceShortcut_;const int clip=capturingClipShortcut_;
    if(!voice&&clip<0)return;
    const uint32_t value=pendingShortcut_;
    auto& error=voice?voiceShortcutError_:clipHotkeys_[size_t(clip)]->error;
    shortcutPrompt_=ShortcutPrompt::None;error.clear();
    if(!GlobalShortcut::valid(value)){error=L"This key could not be used. Try another key or combination.";refreshShortcutEditor();return;}
    auto showPrompt=[&](ShortcutPrompt prompt){
        shortcutPrompt_=prompt;refreshShortcutEditor();
        // Focus Cancel so the captured key cannot accept its own warning.
        SetFocus(voice?voiceShortcutCancel_:clipShortcutCancel_);
    };
    if(GlobalShortcut::typingKey(value)&&!typingShortcutApproved_){showPrompt(ShortcutPrompt::TypingKey);return;}
    const int owner=preferences_.shortcutOwner(value,voice?-1:clip);
    if(owner!=-2&&owner!=approvedShortcutOwner_){pendingShortcutOwner_=owner;showPrompt(ShortcutPrompt::Replace);return;}
    if(owner==-1)voiceHotkey_.clear();else if(owner>=0)clipHotkeys_[size_t(owner)]->key.clear();
    auto& hotkey=voice?voiceHotkey_:clipHotkeys_[size_t(clip)]->key;
    if(!hotkey.set(hwnd_,voice?voiceHotkeyId:clipHotkeyFirst+clip,value)){
        // Commit saved bindings only after Windows accepts the replacement.
        if(owner==-1)registerVoiceShortcut();else if(owner>=0)registerClipShortcut(size_t(owner));
        endShortcutCapture();error=L"Windows or another app is using this shortcut. Your saved shortcuts have not changed.";
        updateVoiceShortcut();updateClipShortcut();return;
    }
    if(owner==-1){preferences_.voiceShortcut=0;voiceShortcutError_.clear();}
    else if(owner>=0){preferences_.clips[size_t(owner)].shortcut=0;clipHotkeys_[size_t(owner)]->error.clear();}
    if(voice)preferences_.voiceShortcut=value;else preferences_.clips[size_t(clip)].shortcut=value;
    error.clear();endShortcutCapture(false);save();
}
bool Window::captureShortcut(MSG& message){
    if(!capturingVoiceShortcut_&&capturingClipShortcut_<0)return false;
    const HWND origin=capturingVoiceShortcut_?voiceShortcut_:clipShortcut_;
    const HWND confirm=capturingVoiceShortcut_?voiceShortcutConfirm_:clipShortcutConfirm_;
    const HWND cancel=capturingVoiceShortcut_?voiceShortcutCancel_:clipShortcutCancel_;
    const HWND focus=GetFocus();
    if(focus!=origin&&focus!=confirm&&focus!=cancel){endShortcutCapture();return false;}
    if(shortcutPrompt_!=ShortcutPrompt::None){
        if(message.message==WM_HOTKEY)return true;
        if(message.message==WM_KEYDOWN&&message.wParam==VK_ESCAPE){endShortcutCapture();return true;}
        return false;
    }
    if(focus!=origin)return message.message==WM_HOTKEY;
    // Registered Gate chords arrive as WM_HOTKEY instead of ordinary keydown.
    // Treat them as attempted assignments, never as playback while editing.
    if(message.message==WM_HOTKEY)return acceptShortcut(uint32_t(HIWORD(message.lParam)|(LOWORD(message.lParam)<<8)));
    if(message.message==WM_KEYUP||message.message==WM_SYSKEYUP||message.message==WM_CHAR||message.message==WM_SYSCHAR)return true;
    if(message.message!=WM_KEYDOWN&&message.message!=WM_SYSKEYDOWN)return false;
    const auto key=unsigned(message.wParam);
    if(message.lParam&(LPARAM(1)<<30))return true;
    if(key==VK_SHIFT||key==VK_CONTROL||key==VK_MENU||key==VK_LWIN||key==VK_RWIN)return true;
    unsigned modifiers=0;
    if(GetKeyState(VK_CONTROL)&0x8000)modifiers|=MOD_CONTROL;
    if(GetKeyState(VK_MENU)&0x8000)modifiers|=MOD_ALT;
    if(GetKeyState(VK_SHIFT)&0x8000)modifiers|=MOD_SHIFT;
    if((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)modifiers|=MOD_WIN;
    return acceptShortcut(key|(modifiers<<8));
}
void Window::toggleVoiceShortcut(){
    if(capturingVoiceShortcut_||capturingClipShortcut_>=0||!voiceHotkey_.active())return;
    const auto previous=preferences_.voice.preset;
    if(!preferences_.toggleVoice())return;
    engine_.setVoice(preferences_.voice);
    SendMessageW(effectToggle_,BM_SETCHECK,preferences_.voice.enabled?BST_CHECKED:BST_UNCHECKED,0);
    InvalidateRect(voiceTiles_[unsigned(previous)],nullptr,FALSE);
    InvalidateRect(voiceTiles_[unsigned(preferences_.voice.preset)],nullptr,FALSE);
    // Switching from a tray hotkey never opens a page or moves keyboard focus.
    if(preferences_.voice.preset!=VoicePreset::Custom)customEditor_=false;
    if(page_==Page::Voice&&(previous==VoicePreset::Custom||preferences_.voice.preset==VoicePreset::Custom))layout();
    updateVoiceShortcut();save();
}
void Window::registerClipShortcut(size_t index){
    auto& hotkey=*clipHotkeys_[index];hotkey.error.clear();
    if(!hotkey.key.set(hwnd_,clipHotkeyFirst+int(index),preferences_.clips[index].shortcut))
        hotkey.error=L"Shortcut unavailable or in use. Choose another; this one is inactive.";
}
void Window::updateClipShortcut(){
    if(selectedClip_<0||size_t(selectedClip_)>=preferences_.clips.size())return;
    const auto value=preferences_.clips[size_t(selectedClip_)].shortcut;
    const auto text=capturingClipShortcut_==selectedClip_?(shortcutPrompt_==ShortcutPrompt::None?std::wstring(L"Press your keys..."):shortcutName(pendingShortcut_)):shortcutName(value);
    SetWindowTextW(clipShortcut_,text.c_str());EnableWindow(clearClipShortcut_,value!=0);
    InvalidateRect(clipShortcut_,nullptr,FALSE);
    if(page_==Page::Soundboard){
        const float x=sideInspector()?soundGridWidth()+32:0,y=clipEditorTop();
        RECT r{px(x),px(top(y+122)),px(contentWidth_),px(top(y+262))};InvalidateRect(content_,&r,FALSE);
    }
}
void Window::triggerClip(size_t index){
    if(index>=preferences_.clips.size())return;
    const auto& clip=preferences_.clips[index];
    if(!importMessage_.empty()){importMessage_.clear();InvalidateRect(content_,nullptr,FALSE);}
    engine_.playClip(clip.file,Preferences::clipPath(clip.file));
    if(selectedClip_>=0){
        if(emojiPopup_)DestroyWindow(emojiPopup_);
        selectClip(-1);if(page_==Page::Soundboard)layout();
    }
}
void Window::updateFeatureTheme(){
    auto apply=[&](HWND h){SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);};
    apply(soundNav_);for(auto h:voiceControls_)apply(h);for(auto h:soundControls_)apply(h);for(auto h:clipTiles_)apply(h);for(auto h:clipSettings_)apply(h);
    SendMessageW(clipName_,WM_SETFONT,reinterpret_cast<WPARAM>(boldFont_),TRUE);
    SetWindowTheme(clipName_,theme_.colors().dark?L"DarkMode_Explorer":L"Explorer",nullptr);
}
void Window::rebuildClips(){
    endShortcutCapture();clipHotkeys_.clear();
    for(size_t i=0;i<preferences_.clips.size();++i){clipHotkeys_.push_back(std::make_unique<ClipHotkey>());registerClipShortcut(i);}
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
    updateClipShortcut();
}
void Window::selectClip(int index){
    endShortcutCapture();
    commitClipName();
    selectedClip_=index;
    if(index>=0&&size_t(index)<preferences_.clips.size()){
        const auto& c=preferences_.clips[size_t(index)];SetWindowTextW(clipName_,c.name.c_str());
        updateClipShortcut();
    }
}
void Window::layoutVoicePages(LayoutBatch& batch){
    auto pos=[&](HWND h,float x,float y,float w,float height){batch.move(h,px(x),px(top(y)),px(w),px(height));};
    const float span=contentWidth_;
    if(page_==Page::Voice){
        const float extra=shortcutExtraHeight(true);
        const unsigned columns=voiceColumns();const float tile=(span-14.f*float(columns-1))/float(columns);
        for(unsigned i=0;i<voiceCount;++i)pos(voiceTiles_[i],float(i%columns)*(tile+14),200+shortcutHeight+extra+float(i/columns)*138,tile,124);
        const float y=voiceSettingsTop(),half=(span-32)/2;
        pos(effectToggle_,half-48,y,46,28);pos(hearMyself_,span-48,y,46,28);
        pos(voiceShortcut_,132,y+100,span-216,34);pos(clearVoiceShortcut_,span-72,y+100,72,34);
        const float actionsY=y+140+std::max(36.f,shortcutMessageHeight_)+8;
        pos(voiceShortcutConfirm_,132,actionsY,112,30);pos(voiceShortcutCancel_,252,actionsY,92,30);
        pos(intensity_,0,y+132+shortcutHeight+extra,span,30);
        if(customEditor_){
            pos(customBack_,0,112+shortcutHeight+extra,132,34);pos(customReset_,span-132,112+shortcutHeight+extra,132,34);
            for(unsigned i=0;i<voiceControlCount;++i)pos(customSliders_[i],float(i%2)*(half+32),customControlTop(i)+30,half,30);
        }
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
            const float extra=shortcutExtraHeight(false),actionsY=y+196+std::max(64.f,shortcutMessageHeight_)+8;
            pos(clipEmoji_,x,y+6,42,42);pos(clipName_,x+52,y+12,w-52,30);
            pos(clipShortcut_,x,y+152,w-68,34);pos(clearClipShortcut_,x+w-60,y+152,60,34);
            pos(clipShortcutConfirm_,x,actionsY,112,30);pos(clipShortcutCancel_,x+120,actionsY,92,30);
            pos(removeClip_,x,y+268+extra,w,36);pos(closeClip_,x,y+312+extra,w,32);
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
        const float extra=shortcutExtraHeight(true);
        const float y=voiceSettingsTop(),half=(span-32)/2,right=half+32;
        text(L"Voice Effects",0,y,half-64,28,15,true);
        secondary(L"Apply the selected effect to your microphone.",0,y+38,half,38);
        text(L"Hear Myself",right,y,half-64,28,15,true);
        secondary(state_.testMessage.empty()?L"Listen to your voice with the effect applied.":state_.testMessage,right,y+38,half,38);
        line(0,y+88,span);
        text(L"Voice shortcut",0,y+104,124,26,14,true);
        std::wstring hint=capturingVoiceShortcut_?shortcutPromptText():
            !preferences_.voiceShortcut?L"Set a shortcut to switch between Normal and your last selected effect.":
            preferences_.lastVoicePreset==VoicePreset::Normal?L"Select an effect first. The shortcut will switch it to Normal and back.":
            std::wstring(L"Normal \u2194 ")+voiceNames[unsigned(preferences_.lastVoicePreset)]+L". Works in other apps and in the tray.";
        if(!voiceShortcutError_.empty())hint=voiceShortcutError_;
        const bool warning=!voiceShortcutError_.empty()||(capturingVoiceShortcut_&&shortcutPrompt_!=ShortcutPrompt::None);
        label(target,hint,{132,top(y+140),span,top(y+140+(capturingVoiceShortcut_?std::max(36.f,shortcutMessageHeight_):36.f))},12.5f,warning?p.warning:p.secondary,false,DWRITE_TEXT_ALIGNMENT_LEADING,true);
        if(customEditor_){
            text(L"Custom",148,114+shortcutHeight+extra,span-296,30,16,true);
            constexpr const wchar_t* sections[]={L"Voice and tone",L"Filters and distortion",L"Robot and alien",L"Movement and echo",L"Space and output"};
            for(unsigned group=0;group<5;++group){
                const float heading=170.f+shortcutHeight+extra+float(group)*190.f;
                text(sections[group],0,heading,span,26,15,true);
                for(unsigned j=0;j<4;++j){
                    const unsigned i=group*4+j;const float x=float(i%2)*(half+32),cy=customControlTop(i);
                    text(voiceControlInfo[i].name,x,cy,half-80,26,12.5f);
                    label(target,customValue(i,preferences_.voice.custom.values[i]),{x+half-80,top(cy),x+half,top(cy+26)},12.5f,p.accent,false,DWRITE_TEXT_ALIGNMENT_TRAILING);
                }
                if(group<4)line(0,heading+178,span);
            }
        }else if(preferences_.voice.preset==VoicePreset::Custom){
            text(L"Custom voice",0,y+102+shortcutHeight+extra,span,26,15,true);
            secondary(L"Select Custom to adjust your saved voice settings.",0,y+136+shortcutHeight+extra,span,36);
        }else{
            text(L"Intensity",0,y+102+shortcutHeight+extra,span-65,26,15,true);
            label(target,std::to_wstring(preferences_.voice.intensity)+L"%",{span-60,top(y+102+shortcutHeight+extra),span,top(y+128+shortcutHeight+extra)},14,p.text,false,DWRITE_TEXT_ALIGNMENT_TRAILING);
        }
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
        if(sideInspector())target->DrawLine({grid+16,top(80)},{grid+16,top(y+350+shortcutExtraHeight(false))},brush.Get(),1);
        if(selectedClip_>=0){
            line(x,y+64,w);
            secondary(L"Click the name to rename. Click the emoji to change it.",x,y+76,w,40);
            text(L"Shortcut",x,y+122,w,26,14,true);
            const auto& shortcut=*clipHotkeys_[size_t(selectedClip_)];
            const std::wstring hint=!shortcut.error.empty()?shortcut.error:capturingClipShortcut_==selectedClip_?
                shortcutPromptText():L"Play or stop this sound from any app, even while Gate is in the tray.";
            const bool warning=!shortcut.error.empty()||(capturingClipShortcut_==selectedClip_&&shortcutPrompt_!=ShortcutPrompt::None);
            label(target,hint,{x,top(y+196),x+w,top(y+196+(capturingClipShortcut_==selectedClip_?std::max(64.f,shortcutMessageHeight_):64.f))},12.5f,warning?p.warning:p.secondary,false,DWRITE_TEXT_ALIGNMENT_LEADING,true);
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
    else{
        const auto found=std::find(customSliders_.begin(),customSliders_.end(),control);if(found==customSliders_.end())return false;
        preferences_.voice.custom.values[size_t(found-customSliders_.begin())]=int(SendMessageW(control,TBM_GETPOS,0,0));engine_.setVoice(preferences_.voice);
    }
    save();invalidateSliderValue(control);return true;
}
bool Window::featureCommand(unsigned id,unsigned notification){
    if(id==ClipName){if(notification==EN_KILLFOCUS)commitClipName();return true;}
    if(notification!=BN_CLICKED)return false;
    if(id>=VoiceFirst&&id<VoiceFirst+voiceCount){
        const auto previous=unsigned(preferences_.voice.preset);
        preferences_.selectVoice(VoicePreset(id-VoiceFirst));
        customEditor_=preferences_.voice.preset==VoicePreset::Custom;
        engine_.setVoice(preferences_.voice);
        InvalidateRect(voiceTiles_[previous],nullptr,FALSE);InvalidateRect(voiceTiles_[id-VoiceFirst],nullptr,FALSE);
        if(customEditor_||previous==unsigned(VoicePreset::Custom)){scroll_=scrollTarget_=0;layout();}
        if(customEditor_)SetFocus(customBack_);
        updateVoiceShortcut();save();return true;
    }
    if(id>=SettingsFirst&&id-SettingsFirst<preferences_.clips.size()){
        selectClip(int(id-SettingsFirst));layout();
        if(!sideInspector())scrollTo(clipEditorTop(),false);
        SetFocus(clipName_);SendMessageW(clipName_,EM_SETSEL,0,-1);return true;
    }
    if(id>=ClipFirst&&id<SettingsFirst&&id-ClipFirst<preferences_.clips.size()){
        triggerClip(size_t(id-ClipFirst));return true;
    }
    switch(id){
    case ConfirmVoiceShortcut:case ConfirmClipShortcut:confirmShortcut();return true;
    case CancelVoiceShortcut:case CancelClipShortcut:endShortcutCapture();return true;
    case VoiceShortcut:
        endShortcutCapture();capturingVoiceShortcut_=true;voiceHotkey_.clear();voiceShortcutError_.clear();SetFocus(voiceShortcut_);refreshShortcutEditor();return true;
    case ClearVoiceShortcut:
        endShortcutCapture();preferences_.voiceShortcut=0;registerVoiceShortcut();save();return true;
    case ClipShortcut:
        if(selectedClip_>=0){endShortcutCapture();capturingClipShortcut_=selectedClip_;auto& shortcut=*clipHotkeys_[size_t(selectedClip_)];shortcut.key.clear();shortcut.error.clear();SetFocus(clipShortcut_);refreshShortcutEditor();}return true;
    case ClearClipShortcut:
        if(selectedClip_>=0){endShortcutCapture();preferences_.clips[size_t(selectedClip_)].shortcut=0;registerClipShortcut(size_t(selectedClip_));updateClipShortcut();save();}return true;
    case CustomBack:customEditor_=false;scroll_=scrollTarget_=0;layout();SetFocus(voiceTiles_[unsigned(VoicePreset::Custom)]);return true;
    case CustomReset:
        preferences_.voice.custom={};
        for(unsigned i=0;i<voiceControlCount;++i)SendMessageW(customSliders_[i],TBM_SETPOS,TRUE,preferences_.voice.custom.values[i]);
        engine_.setVoice(preferences_.voice);save();InvalidateRect(content_,nullptr,FALSE);return true;
    case SoundNav:setPage(Page::Soundboard);return true;
    case EffectToggle:preferences_.voice.enabled=SendMessageW(effectToggle_,BM_GETCHECK,0,0)==BST_CHECKED;engine_.setVoice(preferences_.voice);save();return true;
    case HearMyself:testRequested_=!testRequested_;testPending_=true;engine_.setTest(testRequested_);syncTest();return true;
    case Import:importClip();return true;
    case StopSounds:engine_.stopClips();return true;
    case HearSounds:preferences_.hearSounds=SendMessageW(hearSounds_,BM_GETCHECK,0,0)==BST_CHECKED;engine_.setHearSounds(preferences_.hearSounds);save();return true;
    case ClipEmoji:openEmojiPicker();return true;
    case CloseClip:selectClip(-1);break;
    case RemoveClip:if(selectedClip_>=0){
        endShortcutCapture();
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
