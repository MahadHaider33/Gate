#include "ui/Window.h"
#include <algorithm>
#include <cwctype>
#include <dwmapi.h>
#include <uxtheme.h>

namespace gate {
namespace {
enum Category {All, Faces, Gestures, Nature, Sounds, Things};
struct Emoji {const wchar_t* glyph;const wchar_t* words;unsigned category=Things;};
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
// Keep the original choices, grouped by their position in the catalog.
unsigned category(size_t index){
    if(index<7||(index>=50&&index<=61))return Sounds;
    if(index<=26)return Faces;
    if(index>=34&&index<=38)return Gestures;
    if(index>=62&&index<=75)return Nature;
    return emojis[index].category;
}
constexpr const wchar_t* categoryIcons[]={L"All",L"😀",L"👋",L"🐶",L"🎵",L"🎉"};
constexpr const wchar_t* categoryNames[]={L"All emoji",L"Smileys",L"Hands",L"Nature",L"Sounds",L"Objects"};
}

void Window::drawEmoji(ID2D1RenderTarget* target,const std::wstring& text,D2D1_RECT_F box,float size){
    const float width=box.right-box.left,height=box.bottom-box.top;
    if(width<=0||height<=0)return;
    auto cached=std::find_if(emojiLayouts_.begin(),emojiLayouts_.end(),[&](const CachedText& item){
        return item.text==text&&item.width==width&&item.height==height&&item.size==size;
    });
    if(cached==emojiLayouts_.end()){
        ComPtr<IDWriteTextFormat> format;
        check(textFactory_->CreateTextFormat(L"Segoe UI Emoji",nullptr,DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&format));
        ComPtr<IDWriteTextLayout> layout;
        check(textFactory_->CreateTextLayout(text.c_str(),UINT32(text.size()),format.Get(),width,height,&layout));
        layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        emojiLayouts_.push_front({text,width,height,size,false,false,DWRITE_TEXT_ALIGNMENT_CENTER,std::move(layout)});
        if(emojiLayouts_.size()>192)emojiLayouts_.pop_back();
    }else emojiLayouts_.splice(emojiLayouts_.begin(),emojiLayouts_,cached);
    ComPtr<ID2D1SolidColorBrush> brush;target->CreateSolidColorBrush(theme_.colors().text,&brush);
    target->DrawTextLayout({box.left,box.top},emojiLayouts_.front().layout.Get(),brush.Get(),
        D2D1_DRAW_TEXT_OPTIONS(D2D1_DRAW_TEXT_OPTIONS_CLIP|D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT));
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
        else if(GetDlgCtrlID(control)>=10&&GetDlgCtrlID(control)<10+int(std::size(categoryIcons)))SendMessageW(control,BM_CLICK,0,0);
        else self->chooseEmoji();
        return 0;
    }
    if(control==self->emojiSearch_&&msg==WM_KEYDOWN&&w==VK_DOWN){
        SetFocus(self->emojiList_);return 0;
    }
    return DefSubclassProc(control,msg,w,l);
}

void Window::openEmojiPicker(){
    if(selectedClip_<0)return;
    commitClipName();
    if(emojiPopup_){SetForegroundWindow(emojiPopup_);return;}
    WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=emojiProcedure;cls.hInstance=instance_;
    cls.hCursor=LoadCursor(nullptr,IDC_ARROW);cls.lpszClassName=L"GateEmojiPicker";RegisterClassExW(&cls);
    RECT anchor{};GetWindowRect(clipEmoji_,&anchor);
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT bounds{0,0,px(364),px(430)};const DWORD style=WS_POPUP|WS_CAPTION|WS_SYSMENU;
    AdjustWindowRectExForDpi(&bounds,style,FALSE,WS_EX_TOOLWINDOW,dpi_);
    const int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    const int x=std::clamp(int(anchor.left),int(monitor.rcWork.left),std::max(int(monitor.rcWork.left),int(monitor.rcWork.right)-width));
    const int y=std::clamp(int(anchor.bottom+6),int(monitor.rcWork.top),std::max(int(monitor.rcWork.top),int(monitor.rcWork.bottom)-height));
    emojiPopup_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT,cls.lpszClassName,L"Choose emoji",style,x,y,width,height,hwnd_,nullptr,instance_,this);
    if(!emojiPopup_)return;
    BOOL dark=theme_.colors().dark;
    const COLORREF caption=rgb(theme_.colors().field),captionText=rgb(theme_.colors().text);
    DwmSetWindowAttribute(emojiPopup_,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    DwmSetWindowAttribute(emojiPopup_,DWMWA_CAPTION_COLOR,&caption,sizeof(caption));
    DwmSetWindowAttribute(emojiPopup_,DWMWA_TEXT_COLOR,&captionText,sizeof(captionText));
    emojiSearch_=CreateWindowExW(WS_EX_CLIENTEDGE,WC_EDITW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,px(12),px(12),px(340),px(32),emojiPopup_,reinterpret_cast<HMENU>(1),instance_,nullptr);
    SendMessageW(emojiSearch_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Search emoji"));
    SendMessageW(emojiSearch_,EM_SETLIMITTEXT,80,0);
    emojiCategory_=All;
    for(unsigned i=0;i<std::size(categoryIcons);++i){
        auto button=CreateWindowExW(0,WC_BUTTONW,categoryNames[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            px(12+float(i)*56),px(52),px(52),px(38),emojiPopup_,reinterpret_cast<HMENU>(INT_PTR(10+i)),instance_,nullptr);
        SetWindowSubclass(button,editorProcedure,2,reinterpret_cast<DWORD_PTR>(this));
    }
    emojiList_=CreateWindowExW(0,WC_LISTVIEWW,L"Emoji",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_ICON|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_AUTOARRANGE|LVS_SHAREIMAGELISTS,
        px(12),px(100),px(340),px(316),emojiPopup_,reinterpret_cast<HMENU>(2),instance_,nullptr);
    // Native icon view supplies scrolling, keyboard navigation, tooltips and accessible names.
    // The transparent image only reserves space; DirectWrite paints each color emoji.
    emojiImages_=ImageList_Create(px(32),px(32),ILC_COLOR32,1,1);
    if(emojiImages_){ImageList_SetImageCount(emojiImages_,1);ListView_SetImageList(emojiList_,emojiImages_,LVSIL_NORMAL);}
    ListView_SetExtendedListViewStyle(emojiList_,LVS_EX_DOUBLEBUFFER|LVS_EX_HIDELABELS|LVS_EX_INFOTIP);
    ListView_SetIconSpacing(emojiList_,px(46),px(48));
    ListView_SetBkColor(emojiList_,rgb(theme_.colors().field));
    ListView_SetTextBkColor(emojiList_,rgb(theme_.colors().field));
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
    SendMessageW(emojiList_,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(emojiList_);
    for(size_t i=0;i<std::size(emojis);++i){
        const auto& e=emojis[i];if(!query.empty()&&lower(e.words).find(query)==std::wstring::npos&&std::wstring(e.glyph).find(query)==std::wstring::npos)continue;
        if(query.empty()&&emojiCategory_!=All&&category(i)!=emojiCategory_)continue;
        LVITEMW item{};item.mask=LVIF_TEXT|LVIF_IMAGE|LVIF_PARAM;item.iItem=ListView_GetItemCount(emojiList_);
        item.pszText=const_cast<wchar_t*>(e.words);item.iImage=0;item.lParam=LPARAM(i);
        ListView_InsertItem(emojiList_,&item);
    }
    if(ListView_GetItemCount(emojiList_)>0){
        ListView_SetItemState(emojiList_,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        ListView_EnsureVisible(emojiList_,0,FALSE);
    }
    SendMessageW(emojiList_,WM_SETREDRAW,TRUE,0);InvalidateRect(emojiList_,nullptr,TRUE);
}

void Window::chooseEmoji(){
    if(selectedClip_<0||!emojiList_)return;
    const auto row=ListView_GetNextItem(emojiList_,-1,LVNI_SELECTED);if(row<0)return;
    LVITEMW item{};item.mask=LVIF_PARAM;item.iItem=row;if(!ListView_GetItem(emojiList_,&item))return;
    const auto index=item.lParam;if(index<0||size_t(index)>=std::size(emojis))return;
    preferences_.clips[size_t(selectedClip_)].emoji=emojis[index].glyph;save();
    DestroyWindow(emojiPopup_);InvalidateRect(clipEmoji_,nullptr,FALSE);InvalidateRect(clipTiles_[size_t(selectedClip_)],nullptr,FALSE);SetFocus(clipEmoji_);
}

LRESULT CALLBACK Window::emojiProcedure(HWND h,UINT msg,WPARAM w,LPARAM l){
    auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(h,msg,w,l);
    switch(msg){
    case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE)PostMessageW(h,WM_CLOSE,0,0);return 0;
    case WM_COMMAND:
        if(LOWORD(w)==1&&HIWORD(w)==EN_CHANGE)self->filterEmojis();
        else if(LOWORD(w)>=10&&LOWORD(w)<10+std::size(categoryIcons)&&HIWORD(w)==BN_CLICKED){
            self->emojiCategory_=LOWORD(w)-10;SetWindowTextW(self->emojiSearch_,L"");self->filterEmojis();
            RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
        }return 0;
    case WM_NOTIFY:{
        const auto* header=reinterpret_cast<NMHDR*>(l);if(header->hwndFrom!=self->emojiList_)break;
        if(header->code==NM_CLICK){
            if(reinterpret_cast<NMITEMACTIVATE*>(l)->iItem>=0)self->chooseEmoji();return 0;
        }
        if(header->code==LVN_GETINFOTIPW){
            auto* tip=reinterpret_cast<NMLVGETINFOTIPW*>(l);
            LVITEMW item{};item.mask=LVIF_PARAM;item.iItem=tip->iItem;
            if(ListView_GetItem(self->emojiList_,&item)&&size_t(item.lParam)<std::size(emojis))
                lstrcpynW(tip->pszText,emojis[item.lParam].words,tip->cchTextMax);
            return 0;
        }
        if(header->code==LVN_KEYDOWN){
            if(reinterpret_cast<NMLVKEYDOWN*>(l)->wVKey==VK_SPACE)self->chooseEmoji();return 0;
        }
        if(header->code==NM_CUSTOMDRAW){
            const auto& draw=*reinterpret_cast<NMLVCUSTOMDRAW*>(l);
            if(draw.nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW|CDRF_NOTIFYPOSTPAINT;
            if(draw.nmcd.dwDrawStage==CDDS_POSTPAINT&&ListView_GetItemCount(self->emojiList_)==0){
                RECT rect{};GetClientRect(self->emojiList_,&rect);
                if(self->beginControlPaint(draw.nmcd.hdc,rect)){
                    self->controlTarget_->Clear(self->theme_.colors().field);
                    self->label(self->controlTarget_.Get(),L"No emoji found",{0,0,rect.right/self->scale(),80},14,self->theme_.colors().secondary,false,DWRITE_TEXT_ALIGNMENT_CENTER);
                    self->endControlPaint();
                }
            }
            if(draw.nmcd.dwDrawStage!=CDDS_ITEMPREPAINT)return CDRF_DODEFAULT;
            const auto index=size_t(draw.nmcd.lItemlParam);if(index>=std::size(emojis))return CDRF_SKIPDEFAULT;
            RECT rect{};ListView_GetItemRect(self->emojiList_,int(draw.nmcd.dwItemSpec),&rect,LVIR_BOUNDS);
            if(self->beginControlPaint(draw.nmcd.hdc,rect)){
                const auto& p=self->theme_.colors();const float width=(rect.right-rect.left)/self->scale(),height=(rect.bottom-rect.top)/self->scale();
                self->controlTarget_->Clear(p.field);
                if(ListView_GetItemState(self->emojiList_,int(draw.nmcd.dwItemSpec),LVIS_SELECTED)&LVIS_SELECTED){
                    ComPtr<ID2D1SolidColorBrush> brush;self->controlTarget_->CreateSolidColorBrush(p.selected,&brush);
                    self->controlTarget_->FillRoundedRectangle(D2D1::RoundedRect({1,1,width-1,height-1},6,6),brush.Get());
                }
                self->drawEmoji(self->controlTarget_.Get(),emojis[index].glyph,{0,0,width,height},26);
                self->endControlPaint();
            }return CDRF_SKIPDEFAULT;
        }break;
    }
    case WM_DRAWITEM:{
        const auto& item=*reinterpret_cast<DRAWITEMSTRUCT*>(l);
        if(item.CtlID<10||item.CtlID>=10+std::size(categoryIcons))return FALSE;
        if(self->beginControlPaint(item.hDC,item.rcItem)){
            const auto index=item.CtlID-10;
            const auto& p=self->theme_.colors();self->controlTarget_->Clear(p.field);
            const float width=(item.rcItem.right-item.rcItem.left)/self->scale(),height=(item.rcItem.bottom-item.rcItem.top)/self->scale();
            if(index==self->emojiCategory_||(item.itemState&(ODS_SELECTED|ODS_FOCUS))){
                ComPtr<ID2D1SolidColorBrush> brush;self->controlTarget_->CreateSolidColorBrush(p.selected,&brush);
                self->controlTarget_->FillRoundedRectangle(D2D1::RoundedRect({0,0,width,height},6,6),brush.Get());
            }
            if(index==0)self->label(self->controlTarget_.Get(),L"All",{0,0,width,height},13,p.text,false,DWRITE_TEXT_ALIGNMENT_CENTER);
            else self->drawEmoji(self->controlTarget_.Get(),categoryIcons[index],{0,0,width,height},22);
            self->endControlPaint();
        }return TRUE;
    }
    case WM_ERASEBKGND:{RECT r{};GetClientRect(h,&r);FillRect(reinterpret_cast<HDC>(w),&r,self->fieldBrush_);return 1;}
    case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:
        SetTextColor(reinterpret_cast<HDC>(w),rgb(self->theme_.colors().text));SetBkColor(reinterpret_cast<HDC>(w),rgb(self->theme_.colors().field));return reinterpret_cast<LRESULT>(self->fieldBrush_);
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:
        if(self->emojiImages_){ListView_SetImageList(self->emojiList_,nullptr,LVSIL_NORMAL);ImageList_Destroy(self->emojiImages_);self->emojiImages_=nullptr;}
        self->emojiPopup_=self->emojiSearch_=self->emojiList_=nullptr;return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
}
