#include "ui/Window.h"
#include <algorithm>
#include <cwctype>
#include <uxtheme.h>

namespace gate {
namespace {
struct Emoji {const wchar_t* glyph;const wchar_t* words;};
constexpr Emoji emojis[]={
    {L"🎵",L"Music note sound"},{L"📣",L"Megaphone air horn loud"},
    {L"👏",L"Applause clap hands"},{L"🥁",L"Drum roll rimshot percussion"},
    {L"🚨",L"Alarm siren buzzer"},{L"🔔",L"Bell chime notification"},
    {L"🎺",L"Trumpet brass trombone"},{L"😂",L"Laugh tears joy funny"},
    {L"🤣",L"Rolling laughing hilarious"},{L"😀",L"Smile happy grin"},
    {L"😎",L"Cool sunglasses"},{L"😍",L"Love heart eyes"},
    {L"🥳",L"Party celebration"},{L"😢",L"Sad crying tear"},
    {L"😭",L"Sobbing crying tears"},{L"😡",L"Angry mad rage"},
    {L"😱",L"Scream shocked scared"},{L"🤯",L"Mind blown explosion"},
    {L"🤔",L"Thinking question hmm"},{L"🙄",L"Eye roll bored"},
    {L"😴",L"Sleep snore tired"},{L"🤡",L"Clown funny joke"},
    {L"💀",L"Skull dead skeleton"},{L"👻",L"Ghost spooky boo"},
    {L"👽",L"Alien space"},{L"🤖",L"Robot machine"},
    {L"💩",L"Poop silly"},{L"🔥",L"Fire hot flame"},
    {L"💥",L"Boom explosion bang"},{L"⚡",L"Lightning electric zap"},
    {L"✨",L"Sparkles magic chime"},{L"⭐",L"Star success"},
    {L"❤️",L"Heart love"},{L"💔",L"Broken heart sad"},
    {L"👍",L"Thumbs up yes good"},{L"👎",L"Thumbs down no bad"},
    {L"👋",L"Wave hello goodbye"},{L"🙏",L"Please thanks praying"},
    {L"💪",L"Strong muscle flex"},{L"🎉",L"Confetti celebration party"},
    {L"🎊",L"Confetti ball victory"},{L"🎈",L"Balloon party pop"},
    {L"🏆",L"Trophy win victory"},{L"🎮",L"Game controller gaming"},
    {L"🎲",L"Dice random roll"},{L"🎯",L"Target bullseye hit"},
    {L"🚀",L"Rocket launch space"},{L"💣",L"Bomb explosion"},
    {L"🔫",L"Water pistol shoot pew"},{L"⚔️",L"Swords battle fight"},
    {L"🛎️",L"Service bell ding"},{L"⏰",L"Alarm clock wake"},
    {L"📯",L"Postal horn fanfare"},{L"🎸",L"Guitar rock music"},
    {L"🎹",L"Piano keyboard music"},{L"🎻",L"Violin sad strings"},
    {L"🎷",L"Saxophone jazz"},{L"🎤",L"Microphone singing voice"},
    {L"🎧",L"Headphones listening"},{L"📻",L"Radio broadcast"},
    {L"🔊",L"Speaker loud sound"},{L"🔇",L"Muted silent quiet"},
    {L"🐶",L"Dog bark woof"},{L"🐱",L"Cat meow"},
    {L"🐸",L"Frog croak"},{L"🦆",L"Duck quack"},
    {L"🐔",L"Chicken cluck"},{L"🐮",L"Cow moo"},
    {L"🐷",L"Pig oink"},{L"🐐",L"Goat bleat"},
    {L"🦁",L"Lion roar"},{L"🐒",L"Monkey ape"},
    {L"🦗",L"Cricket silence awkward"},{L"🌧️",L"Rain weather"},
    {L"🌊",L"Wave ocean water"},{L"🌪️",L"Tornado wind whoosh"},
    {L"🚗",L"Car engine horn"},{L"🚂",L"Train whistle"},
    {L"🚓",L"Police siren"},{L"✈️",L"Plane jet flying"},
    {L"🛸",L"Ufo spaceship alien"},{L"🍿",L"Popcorn movie"},
    {L"💰",L"Money cash coins"},{L"🪙",L"Coin reward pickup"},
    {L"✅",L"Check correct yes success"},{L"❌",L"Wrong error no fail"},
    {L"❓",L"Question confused"},{L"⚠️",L"Warning alert"}
};
std::wstring lower(std::wstring value){std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return wchar_t(towlower(c));});return value;}
}

void Window::commitClipName(){
    if(!clipName_||selectedClip_<0||size_t(selectedClip_)>=preferences_.clips.size())return;
    wchar_t buffer[101]{};GetWindowTextW(clipName_,buffer,101);std::wstring name=buffer;
    const auto first=name.find_first_not_of(L" \t\r\n"),last=name.find_last_not_of(L" \t\r\n");
    auto& clip=preferences_.clips[size_t(selectedClip_)];
    if(first==std::wstring::npos){SetWindowTextW(clipName_,clip.name.c_str());return;}
    name=name.substr(first,last-first+1);
    if(name==clip.name)return;
    clip.name=name;SetWindowTextW(clipName_,name.c_str());
    SetWindowTextW(clipTiles_[size_t(selectedClip_)],name.c_str());
    SetWindowTextW(clipSettings_[size_t(selectedClip_)],(L"Settings for "+name).c_str());save();
}

LRESULT CALLBACK Window::editorProcedure(HWND control,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
    auto* self=reinterpret_cast<Window*>(data);
    if(msg==WM_GETDLGCODE&&(w==VK_RETURN||w==VK_ESCAPE))return DLGC_WANTALLKEYS;
    if(msg==WM_KEYDOWN&&(w==VK_RETURN||w==VK_ESCAPE)){
        if(control==self->clipName_){
            if(w==VK_ESCAPE&&self->selectedClip_>=0)SetWindowTextW(control,self->preferences_.clips[size_t(self->selectedClip_)].name.c_str());
            else self->commitClipName();
            SetFocus(self->closeClip_);
        }else if(w==VK_ESCAPE)DestroyWindow(self->emojiPopup_);
        else self->chooseEmoji();
        return 0;
    }
    const auto result=DefSubclassProc(control,msg,w,l);
    if(control==self->emojiList_&&msg==WM_LBUTTONUP){
        const auto hit=SendMessageW(control,LB_ITEMFROMPOINT,0,l);
        if(!HIWORD(hit))self->chooseEmoji();
    }
    return result;
}

void Window::openEmojiPicker(){
    if(selectedClip_<0)return;
    commitClipName();
    if(emojiPopup_){SetForegroundWindow(emojiPopup_);return;}
    WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=emojiProcedure;cls.hInstance=instance_;
    cls.hCursor=LoadCursor(nullptr,IDC_ARROW);cls.lpszClassName=L"GateEmojiPicker";RegisterClassExW(&cls);
    RECT anchor{};GetWindowRect(clipEmoji_,&anchor);
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT bounds{0,0,px(330),px(390)};const DWORD style=WS_POPUP|WS_CAPTION|WS_SYSMENU;
    AdjustWindowRectExForDpi(&bounds,style,FALSE,WS_EX_TOOLWINDOW,dpi_);
    const int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    const int x=std::clamp(int(anchor.left),int(monitor.rcWork.left),std::max(int(monitor.rcWork.left),int(monitor.rcWork.right)-width));
    const int y=std::clamp(int(anchor.bottom+6),int(monitor.rcWork.top),std::max(int(monitor.rcWork.top),int(monitor.rcWork.bottom)-height));
    emojiPopup_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT,cls.lpszClassName,L"Choose emoji",style,x,y,width,height,hwnd_,nullptr,instance_,this);
    if(!emojiPopup_)return;
    emojiSearch_=CreateWindowExW(0,WC_EDITW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,px(12),px(12),px(306),px(32),emojiPopup_,reinterpret_cast<HMENU>(1),instance_,nullptr);
    SendMessageW(emojiSearch_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Search emoji, e.g. laugh or horn"));
    SendMessageW(emojiSearch_,EM_SETLIMITTEXT,80,0);
    emojiList_=CreateWindowExW(0,WC_LISTBOXW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT,px(12),px(56),px(306),px(322),emojiPopup_,reinterpret_cast<HMENU>(2),instance_,nullptr);
    SendMessageW(emojiList_,LB_SETITEMHEIGHT,0,px(42));
    for(auto h:{emojiSearch_,emojiList_}){
        SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
        SetWindowTheme(h,theme_.colors().dark?L"DarkMode_Explorer":L"Explorer",nullptr);
        SetWindowSubclass(h,editorProcedure,2,reinterpret_cast<DWORD_PTR>(this));
    }
    filterEmojis();ShowWindow(emojiPopup_,SW_SHOW);SetFocus(emojiSearch_);
}

void Window::filterEmojis(){
    if(!emojiList_)return;
    wchar_t buffer[81]{};GetWindowTextW(emojiSearch_,buffer,81);const auto query=lower(buffer);
    SendMessageW(emojiList_,WM_SETREDRAW,FALSE,0);SendMessageW(emojiList_,LB_RESETCONTENT,0,0);
    for(size_t i=0;i<std::size(emojis);++i){
        const auto& e=emojis[i];if(!query.empty()&&lower(e.words).find(query)==std::wstring::npos&&std::wstring(e.glyph).find(query)==std::wstring::npos)continue;
        const auto row=SendMessageW(emojiList_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(e.words));
        if(row>=0)SendMessageW(emojiList_,LB_SETITEMDATA,row,LPARAM(i));
    }
    if(SendMessageW(emojiList_,LB_GETCOUNT,0,0)>0)SendMessageW(emojiList_,LB_SETCURSEL,0,0);
    SendMessageW(emojiList_,WM_SETREDRAW,TRUE,0);InvalidateRect(emojiList_,nullptr,TRUE);
}

void Window::chooseEmoji(){
    if(selectedClip_<0||!emojiList_)return;
    const auto row=SendMessageW(emojiList_,LB_GETCURSEL,0,0);if(row==LB_ERR)return;
    const auto index=SendMessageW(emojiList_,LB_GETITEMDATA,row,0);if(index<0||size_t(index)>=std::size(emojis))return;
    preferences_.clips[size_t(selectedClip_)].emoji=emojis[index].glyph;save();
    DestroyWindow(emojiPopup_);InvalidateRect(clipEmoji_,nullptr,FALSE);InvalidateRect(clipTiles_[size_t(selectedClip_)],nullptr,FALSE);SetFocus(clipEmoji_);
}

LRESULT CALLBACK Window::emojiProcedure(HWND h,UINT msg,WPARAM w,LPARAM l){
    auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(h,msg,w,l);
    switch(msg){
    case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE)PostMessageW(h,WM_CLOSE,0,0);return 0;
    case WM_COMMAND:if(LOWORD(w)==1&&HIWORD(w)==EN_CHANGE)self->filterEmojis();return 0;
    case WM_MEASUREITEM:reinterpret_cast<MEASUREITEMSTRUCT*>(l)->itemHeight=self->px(42);return TRUE;
    case WM_DRAWITEM:{
        const auto& item=*reinterpret_cast<DRAWITEMSTRUCT*>(l);
        if(item.itemID==UINT(-1)||item.itemData>=std::size(emojis))return TRUE;
        if(self->beginControlPaint(item.hDC,item.rcItem)){
            const auto& p=self->theme_.colors();self->controlTarget_->Clear(item.itemState&ODS_SELECTED?p.selected:p.field);
            const float width=(item.rcItem.right-item.rcItem.left)/self->scale(),height=(item.rcItem.bottom-item.rcItem.top)/self->scale();
            const auto& e=emojis[item.itemData];
            self->label(self->controlTarget_.Get(),e.glyph,{6,0,46,height},24,p.accent,false,DWRITE_TEXT_ALIGNMENT_CENTER);
            self->label(self->controlTarget_.Get(),e.words,{54,0,width-8,height},12.5f,p.text);
            self->endControlPaint();
        }return TRUE;
    }
    case WM_ERASEBKGND:{RECT r{};GetClientRect(h,&r);FillRect(reinterpret_cast<HDC>(w),&r,self->fieldBrush_);return 1;}
    case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:
        SetTextColor(reinterpret_cast<HDC>(w),rgb(self->theme_.colors().text));SetBkColor(reinterpret_cast<HDC>(w),rgb(self->theme_.colors().field));return reinterpret_cast<LRESULT>(self->fieldBrush_);
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:self->emojiPopup_=self->emojiSearch_=self->emojiList_=nullptr;return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
}
