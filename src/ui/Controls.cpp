#include "ui/Window.h"
#include <windowsx.h>
#include <algorithm>
#include <cmath>

namespace gate {
namespace {
constexpr float sliderEdge=12.f;
D2D1_COLOR_F blend(D2D1_COLOR_F a,D2D1_COLOR_F b,float amount) {
    return {a.r+(b.r-a.r)*amount,a.g+(b.g-a.g)*amount,a.b+(b.b-a.b)*amount,1.f};
}
}
void Window::label(ID2D1RenderTarget* target,const std::wstring& text,D2D1_RECT_F box,float size,
                   D2D1_COLOR_F color,bool bold,DWRITE_TEXT_ALIGNMENT alignment,bool wrap) {
    if(box.right<=box.left || box.bottom<=box.top)return;
    const float width=box.right-box.left,height=box.bottom-box.top;
    auto cached=std::find_if(textLayouts_.begin(),textLayouts_.end(),[&](const CachedText& item){
        return item.width==width&&item.height==height&&item.size==size&&item.bold==bold&&item.wrap==wrap&&item.alignment==alignment&&item.text==text;
    });
    if(cached==textLayouts_.end()){
        auto& format=textFormats_[unsigned(size*2)][bold?1:0];
        if(!format)check(textFactory_->CreateTextFormat(L"Segoe UI",nullptr,
            bold?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&format));
        ComPtr<IDWriteTextLayout> layout;
        check(textFactory_->CreateTextLayout(text.c_str(),UINT32(text.size()),format.Get(),width,height,&layout));
        layout->SetTextAlignment(alignment);
        layout->SetParagraphAlignment(wrap?DWRITE_PARAGRAPH_ALIGNMENT_NEAR:DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        layout->SetWordWrapping(wrap?DWRITE_WORD_WRAPPING_WRAP:DWRITE_WORD_WRAPPING_NO_WRAP);
        if(!wrap){
            DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};
            ComPtr<IDWriteInlineObject> ellipsis;textFactory_->CreateEllipsisTrimmingSign(format.Get(),&ellipsis);
            layout->SetTrimming(&trim,ellipsis.Get());
        }
        textLayouts_.push_front({text,width,height,size,bold,wrap,alignment,std::move(layout)});
        if(textLayouts_.size()>192)textLayouts_.pop_back();
    }else textLayouts_.splice(textLayouts_.begin(),textLayouts_,cached);
    auto* layout=textLayouts_.front().layout.Get();
    ComPtr<ID2D1SolidColorBrush> brush;target->CreateSolidColorBrush(color,&brush);
    target->DrawTextLayout(D2D1::Point2F(box.left,box.top),layout,brush.Get(),D2D1_DRAW_TEXT_OPTIONS(D2D1_DRAW_TEXT_OPTIONS_CLIP|D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT));
}
bool Window::beginControlPaint(HDC dc,const RECT& bounds) {
    if(!controlTarget_){
        auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),float(dpi_),float(dpi_));
        if(FAILED(factory_->CreateDCRenderTarget(&properties,&controlTarget_)))return false;
    }
    if(FAILED(controlTarget_->BindDC(dc,&bounds)))return false;
    controlTarget_->SetDpi(float(dpi_),float(dpi_));
    controlTarget_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    controlTarget_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    controlTarget_->BeginDraw();return true;
}
void Window::endControlPaint(){if(controlTarget_->EndDraw()==D2DERR_RECREATE_TARGET)controlTarget_.Reset();}

void Window::drawIcon(ID2D1RenderTarget* target,unsigned icon,float x,float y,float size,D2D1_COLOR_F color) {
    ComPtr<ID2D1SolidColorBrush> brush;target->CreateSolidColorBrush(color,&brush);
    const float unit=size/32.f,stroke=std::max(1.4f,size/23.f);
    auto point=[&](float a,float b){return D2D1::Point2F(x+a*unit,y+b*unit);};
    auto line=[&](float a,float b,float c,float d){target->DrawLine(point(a,b),point(c,d),brush.Get(),stroke);};
    auto round=[&](float a,float b,float c,float d,float radius){target->DrawRoundedRectangle(D2D1::RoundedRect({x+a*unit,y+b*unit,x+c*unit,y+d*unit},radius*unit,radius*unit),brush.Get(),stroke);};
    auto circle=[&](float a,float b,float radius,bool fill=false){auto shape=D2D1::Ellipse(point(a,b),radius*unit,radius*unit);if(fill)target->FillEllipse(shape,brush.Get());else target->DrawEllipse(shape,brush.Get(),stroke);};
    switch(icon){
    case 0:
        round(12,3,20,21,4);line(8,16,8,20);line(24,16,24,20);
        line(8,20,11,25);line(11,25,21,25);line(21,25,24,20);line(16,25,16,30);line(12,30,20,30);break;
    case 1:case 2:{
        constexpr float deep[]={9,17,26,19,11},high[]={18,9,23,11,18};const float* heights=icon==1?deep:high;
        for(int i=0;i<5;++i)line(5.f+i*5,16-heights[i]/2,5.f+i*5,16+heights[i]/2);break;
    }
    case 3:
        round(5,10,27,27,3);line(16,10,16,5);circle(16,3,1.5f,true);circle(11,17,1.5f,true);circle(21,17,1.5f,true);
        line(12,23,20,23);line(2,16,2,22);line(30,16,30,22);break;
    case 4:
        round(4,11,28,28,2);line(7,11,24,4);circle(12,20,4);line(20,17,25,17);line(20,21,25,21);line(20,25,25,25);break;
    case 5:circle(16,16,12);circle(16,16,7);circle(16,16,2,true);break;
    case 6:
        line(13,23,13,7);line(13,7,27,4);line(27,4,27,20);line(13,11,27,8);
        target->FillEllipse(D2D1::Ellipse(point(8,24),5*unit,3.7f*unit),brush.Get());
        target->FillEllipse(D2D1::Ellipse(point(22,21),5*unit,3.7f*unit),brush.Get());break;
    case 7:for(int row=0;row<2;++row)for(int col=0;col<2;++col)round(4.f+col*14,4.f+row*14,13.f+col*14,13.f+row*14,1);break;
    case 8:case 9:
        circle(12,10,6);round(3,20,21,29,5);
        if(icon==8){line(27,5,27,20);line(23,16,27,20);line(31,16,27,20);}
        else{line(27,5,27,20);line(23,9,27,5);line(31,9,27,5);}break;
    case 10:
        circle(16,17,11);circle(12,15,1,true);circle(20,15,1,true);line(12,22,20,22);line(13,5,16,2);line(16,2,19,5);break;
    case 11:
        round(6,10,26,29,5);line(6,13,3,3);line(3,3,12,10);line(26,13,29,3);line(29,3,20,10);
        line(10,15,14,17);line(22,15,18,17);line(11,23,21,23);line(13,23,14,27);line(19,23,18,27);break;
    case 12:
        target->DrawEllipse(D2D1::Ellipse(point(16,15),11*unit,13*unit),brush.Get(),stroke);
        line(8,12,13,17);line(24,12,19,17);line(14,23,18,23);break;
    case 13:
        round(7,3,25,29,4);line(12,7,20,7);line(12,24,20,24);line(12,13,12,18);line(16,11,16,20);line(20,13,20,18);break;
    case 14:
        circle(16,16,3,true);circle(16,16,8);line(3,4,3,28);line(29,4,29,28);line(3,4,8,4);line(24,28,29,28);break;
    case 15:
        drawIcon(target,0,x+size*.2f,y+size*.1f,size*.65f,color);line(2,8,6,11);line(27,8,31,4);line(28,16,32,16);line(1,20,5,18);break;
    case 16:
        for(int i=0;i<3;++i){const float cy=7.f+9*i,cx=i==1?11.f:22.f;line(3,cy,cx-3,cy);line(cx+3,cy,29,cy);circle(cx,cy,3);}break;
    default:break;
    }
}

void Window::invalidateSliderValue(HWND control){
    if(isMediaFader(control)){RECT r{0,px(top(358)),px(contentWidth_),px(top(383))};InvalidateRect(content_,&r,FALSE);return;}
    RECT bounds{};GetWindowRect(control,&bounds);
    MapWindowPoints(nullptr,content_,reinterpret_cast<POINT*>(&bounds),2);
    bounds.bottom=bounds.top;bounds.top-=px(40);bounds.left=0;bounds.right=px(contentWidth_);
    InvalidateRect(content_,&bounds,FALSE);
}
float Window::sliderThumbX(HWND control) const {
    RECT bounds{};GetClientRect(control,&bounds);
    const int low=int(SendMessageW(control,TBM_GETRANGEMIN,0,0));
    const int high=int(SendMessageW(control,TBM_GETRANGEMAX,0,0));
    const int value=int(SendMessageW(control,TBM_GETPOS,0,0));
    const float fraction=high>low?float(value-low)/float(high-low):0.f;
    return sliderEdge+std::max(1.f,bounds.right/scale()-2*sliderEdge)*std::clamp(fraction,0.f,1.f);
}

void Window::paintControl(HWND control,HDC dc) {
    RECT bounds{};GetClientRect(control,&bounds);
    if(!beginControlPaint(dc,bounds))return;
    auto* target=controlTarget_.Get();const auto& p=theme_.colors();
    const float w=bounds.right/scale(),h=bounds.bottom/scale();
    if(paintMediaControl(control,target,w,h)){endControlPaint();return;}
    const bool nav=control==microphoneNav_||control==voiceNav_||control==soundNav_||control==mediaNav_;
    const bool combo=control==input_||control==listener_||control==mediaApp_;
    const bool slider=isSlider(control);
    const bool toggle=isToggle(control);
    const bool enabled=IsWindowEnabled(control)!=FALSE;
    const bool hover=hoverControl_==control&&enabled;
    const bool pressed=(SendMessageW(control,BM_GETSTATE,0,0)&BST_PUSHED)!=0;
    const bool microphoneControl=control==strength_||control==threshold_||control==suppression_||control==gate_;
    const auto background=nav?p.sidebar:microphoneControl?p.card:p.background;
    target->Clear(background);
    ComPtr<ID2D1SolidColorBrush> brush;target->CreateSolidColorBrush(p.text,&brush);
    auto round=[&](D2D1_RECT_F r,float radius,D2D1_COLOR_F color){brush->SetColor(color);target->FillRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush.Get());};
    auto line=[&](float x1,float y1,float x2,float y2,D2D1_COLOR_F color,float width=1.5f){brush->SetColor(color);target->DrawLine({x1,y1},{x2,y2},brush.Get(),width);};
    auto outline=[&](D2D1_RECT_F r,float radius,D2D1_COLOR_F color){brush->SetColor(color);target->DrawRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush.Get(),1.f);};
    if(combo){
        round({.5f,.5f,w-.5f,h-.5f},6,p.field);
        outline({.5f,.5f,w-.5f,h-.5f},6,hover?p.secondary:p.border);
        const auto selected=SendMessageW(control,CB_GETCURSEL,0,0);
        wchar_t text[2048]{};
        if(selected!=CB_ERR)SendMessageW(control,CB_GETLBTEXT,selected,reinterpret_cast<LPARAM>(text));
        else wcscpy_s(text,control==mediaApp_?L"Select an app":control==input_?L"Select a microphone":L"Select a listening device");
        const float cy=h/2;
        if(control==input_){
            outline({17,cy-9,23,cy+2},3,p.secondary);
            ComPtr<ID2D1PathGeometry> shape;factory_->CreatePathGeometry(&shape);ComPtr<ID2D1GeometrySink> sink;shape->Open(&sink);
            sink->BeginFigure({13,cy-1},D2D1_FIGURE_BEGIN_HOLLOW);sink->AddBezier({{13,cy+9},{27,cy+9},{27,cy-1}});sink->EndFigure(D2D1_FIGURE_END_OPEN);sink->Close();
            brush->SetColor(p.secondary);target->DrawGeometry(shape.Get(),brush.Get(),1.5f);line(20,cy+6,20,cy+10,p.secondary);
        }else{
            ComPtr<ID2D1PathGeometry> shape;factory_->CreatePathGeometry(&shape);ComPtr<ID2D1GeometrySink> sink;shape->Open(&sink);
            sink->BeginFigure({12,cy+3},D2D1_FIGURE_BEGIN_HOLLOW);sink->AddBezier({{10,cy-12},{30,cy-12},{28,cy+3}});sink->EndFigure(D2D1_FIGURE_END_OPEN);sink->Close();
            brush->SetColor(p.secondary);target->DrawGeometry(shape.Get(),brush.Get(),1.7f);
            round({11,cy,16,cy+8},2,p.secondary);round({24,cy,29,cy+8},2,p.secondary);
        }
        label(target,text,{38,0,w-34,h},14.5f,p.text);
        const float y=h/2-1;line(w-23,y-2,w-19,y+2,p.secondary);line(w-19,y+2,w-15,y-2,p.secondary);
    }else if(toggle){
        const bool on=SendMessageW(control,BM_GETCHECK,0,0)==BST_CHECKED;
        auto track=on&&enabled?p.accent:p.track;if(hover)track=blend(track,p.text,.10f);
        round({2,3,w-2,h-3},(h-6)/2,track);
        const float radius=(h-12)/2;const float x=on?w-5-radius:5+radius;
        brush->SetColor(p.highContrast?p.buttonText:on&&enabled?p.buttonText:p.secondary);
        target->FillEllipse(D2D1::Ellipse({x,h/2},radius,radius),brush.Get());
    }else if(slider){
        const float x=sliderThumbX(control),y=h/2;
        const auto left=enabled?p.accent:p.secondary;
        const auto right=p.track;
        constexpr float halfTrack=2.f;
        round({sliderEdge,y-halfTrack,w-sliderEdge,y+halfTrack},halfTrack,right);
        if(x>sliderEdge)round({sliderEdge,y-halfTrack,x,y+halfTrack},halfTrack,left);
        constexpr float radius=8.f;
        brush->SetColor(blend(p.card,p.text,.12f));target->FillEllipse(D2D1::Ellipse({x,y+1},radius+.5f,radius+.5f),brush.Get());
        brush->SetColor(enabled?p.accent:p.secondary);
        target->FillEllipse(D2D1::Ellipse({x,y},radius,radius),brush.Get());
    }else if(nav){
        const bool selected=(control==microphoneNav_&&page_==Page::Microphone)||(control==voiceNav_&&page_==Page::Voice)||(control==soundNav_&&page_==Page::Soundboard)||(control==mediaNav_&&page_==Page::Media);
        if(selected||hover)round({0,0,w,h},8,selected?p.selected:blend(p.sidebar,p.text,.035f));
        if(selected)round({0,10,3,h-10},1.5f,p.accent);
        const auto color=selected?p.accent:p.secondary;
        const float cy=h/2;
        if(control==microphoneNav_){
            outline({23,cy-11,31,cy+3},4,color);
            ComPtr<ID2D1PathGeometry> shape;factory_->CreatePathGeometry(&shape);
            ComPtr<ID2D1GeometrySink> sink;shape->Open(&sink);
            sink->BeginFigure({19,cy-1},D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddBezier({{19,cy+11},{35,cy+11},{35,cy-1}});sink->EndFigure(D2D1_FIGURE_END_OPEN);sink->Close();
            brush->SetColor(color);target->DrawGeometry(shape.Get(),brush.Get(),1.6f);
            line(27,cy+8,27,cy+13,color);
        }else if(control==mediaNav_){
            drawIcon(target,6,15,cy-12,24,color);
        }else if(control==soundNav_){
            drawIcon(target,7,15,cy-12,24,color);
        }else{
            constexpr float heights[]={5,12,20,10,4};
            for(int i=0;i<5;++i)round({17.f+i*4,cy-heights[i]/2,19.f+i*4,cy+heights[i]/2},1,color);
        }
        wchar_t text[64]{};GetWindowTextW(control,text,64);label(target,text,{54,0,w-14,h},16,color,selected);
    }else if(control==test_){
        auto color=enabled?p.accent:p.track;
        if(hover)color=blend(color,p.text,pressed?.02f:.10f);
        round({0,0,w,h},8,color);
        const float iconX=std::max(17.f,w/2-77),cy=h/2;
        brush->SetColor(p.buttonText);
        if(state_.testActive)round({iconX,cy-5,iconX+10,cy+5},2,p.buttonText);
        else{
            ComPtr<ID2D1PathGeometry> shape;factory_->CreatePathGeometry(&shape);ComPtr<ID2D1GeometrySink> sink;shape->Open(&sink);
            sink->BeginFigure({iconX,cy-6},D2D1_FIGURE_BEGIN_FILLED);sink->AddLine({iconX+10,cy});sink->AddLine({iconX,cy+6});sink->EndFigure(D2D1_FIGURE_END_CLOSED);sink->Close();target->FillGeometry(shape.Get(),brush.Get());
        }
        wchar_t text[64]{};GetWindowTextW(control,text,64);
        label(target,text,{iconX+20,0,w-12,h},15.5f,p.buttonText,true);
    }else if(isTile(control)){
        const bool selected=tileSelected(control);
        const auto voice=std::find(voiceTiles_.begin(),voiceTiles_.end(),control);
        wchar_t text[256]{};GetWindowTextW(control,text,256);
        if(voice!=voiceTiles_.end()){
            const unsigned preset=unsigned(voice-voiceTiles_.begin());
            round({.5f,.5f,w-.5f,h-.5f},8,selected?p.selected:hover?blend(p.card,p.text,.035f):p.card);
            outline({.5f,.5f,w-.5f,h-.5f},8,selected?p.accent:p.border);
            drawIcon(target,preset<6?preset:preset+2,w/2-18,19,36,selected?p.accent:p.secondary);
            label(target,text,{12,62,w-12,86},15,p.text,true,DWRITE_TEXT_ALIGNMENT_CENTER);
            label(target,voiceDescriptions[preset],{12,90,w-12,111},12,p.secondary,false,DWRITE_TEXT_ALIGNMENT_CENTER);
            if(selected){
                brush->SetColor(p.accent);target->FillEllipse(D2D1::Ellipse({w-18,18},7,7),brush.Get());
                line(w-21,18,w-19,20,p.buttonText,1.6f);line(w-19,20,w-15,15.5f,p.buttonText,1.6f);
            }
        }else{
            const auto index=size_t(std::find(clipTiles_.begin(),clipTiles_.end(),control)-clipTiles_.begin());
            const bool playing=index<preferences_.clips.size()&&state_.playingClip==preferences_.clips[index].file;
            round({.5f,.5f,w-.5f,h-.5f},7,playing?p.selected:hover?blend(p.card,p.text,.035f):p.card);
            outline({.5f,.5f,w-.5f,h-.5f},7,playing?p.accent:p.border);
            if(index<preferences_.clips.size())drawEmoji(target,preferences_.clips[index].emoji,{12,22,w-12,72},30);
            label(target,text,{14,79,w-14,106},14.5f,p.text,true);
            if(playing){round({w-26,h-13,w-22,h-7},1,p.accent);round({w-19,h-18,w-15,h-7},1,p.accent);}

        }
    }else if(control==clipEmoji_){
        if(hover)round({0,0,w,h},6,p.selected);
        if(selectedClip_>=0)drawEmoji(target,preferences_.clips[size_t(selectedClip_)].emoji,{0,0,w,h},28);
    }else if(std::find(clipSettings_.begin(),clipSettings_.end(),control)!=clipSettings_.end()){
        const auto index=size_t(std::find(clipSettings_.begin(),clipSettings_.end(),control)-clipSettings_.begin());
        const bool playing=index<preferences_.clips.size()&&state_.playingClip==preferences_.clips[index].file;
        target->Clear(playing?p.selected:p.card);
        if(hover)round({1,1,w-1,h-1},6,blend(p.card,p.text,.1f));
        label(target,L"\u2699",{0,0,w,h},21,hover?p.accent:p.secondary,false,DWRITE_TEXT_ALIGNMENT_CENTER);
    }else if(control!=setup_){
        const bool remove=control==removeClip_;
        if(!remove){round({.5f,.5f,w-.5f,h-.5f},6,hover?blend(p.field,p.text,.045f):p.field);outline({.5f,.5f,w-.5f,h-.5f},6,p.border);}
        else if(hover)round({.5f,.5f,w-.5f,h-.5f},6,blend(p.background,p.warning,.09f));
        wchar_t text[256]{};GetWindowTextW(control,text,256);
        float left=10;
        if(control==import_){line(17,h/2,29,h/2,p.secondary);line(23,h/2-6,23,h/2+6,p.secondary);left=38;}
        else if(control==stopSounds_){round({15,h/2-4,23,h/2+4},1,p.secondary);left=31;}
        label(target,text,{left,0,w-10,h},13,enabled?(remove?p.warning:p.text):p.secondary,false,DWRITE_TEXT_ALIGNMENT_CENTER);
    }else{
        if(hover)round({0,0,w,h},4,p.selected);
        wchar_t text[128]{};GetWindowTextW(control,text,128);label(target,text,{4,0,w-4,h},11.5f,p.accent,false,DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    if(GetFocus()==control && !(SendMessageW(control,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){
        outline({1.5f,1.5f,w-1.5f,h-1.5f},5,p.secondary);
    }
    endControlPaint();
}
void Window::drawItem(const DRAWITEMSTRUCT& item) {
    if(item.CtlType!=ODT_COMBOBOX){paintControl(item.hwndItem,item.hDC);return;}
    if(!beginControlPaint(item.hDC,item.rcItem))return;
    const auto& p=theme_.colors();const bool selected=(item.itemState&ODS_SELECTED)!=0;
    controlTarget_->Clear(selected?p.selected:p.field);
    wchar_t name[2048]{};if(item.itemID!=UINT(-1))SendMessageW(item.hwndItem,CB_GETLBTEXT,item.itemID,reinterpret_cast<LPARAM>(name));
    label(controlTarget_.Get(),name,{12,0,(item.rcItem.right-item.rcItem.left)/scale()-12,(item.rcItem.bottom-item.rcItem.top)/scale()},14.5f,p.text);
    endControlPaint();
}
LRESULT CALLBACK Window::controlProcedure(HWND control,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data) {
    auto* self=reinterpret_cast<Window*>(data);
    const bool slider=self->isSlider(control);
    const bool vertical=self->isMediaFader(control);
    if(vertical&&message==WM_KEYDOWN&&IsWindowEnabled(control)){
        int value=int(SendMessageW(control,TBM_GETPOS,0,0));
        switch(w){
        case VK_UP:case VK_RIGHT:++value;break;
        case VK_DOWN:case VK_LEFT:--value;break;
        case VK_PRIOR:value+=10;break;
        case VK_NEXT:value-=10;break;
        case VK_HOME:value=100;break;
        case VK_END:value=0;break;
        default:return DefSubclassProc(control,message,w,l);
        }
        value=std::clamp(value,0,100);SendMessageW(control,TBM_SETPOS,TRUE,value);
        SendMessageW(GetParent(control),WM_VSCROLL,MAKEWPARAM(TB_THUMBTRACK,value),reinterpret_cast<LPARAM>(control));return 0;
    }
    if(message==WM_PAINT){
        // Native trackbars invalidate only thumb fragments. Repaint the complete
        // control into Direct2D's DC buffer so partial updates cannot leave trails.
        InvalidateRect(control,nullptr,FALSE);
        PAINTSTRUCT ps{};BeginPaint(control,&ps);
        try{self->paintControl(control,ps.hdc);}catch(...){}
        EndPaint(control,&ps);return 0;
    }
    if(message==WM_PRINTCLIENT){try{self->paintControl(control,reinterpret_cast<HDC>(w));}catch(...){}return 0;}
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_LBUTTONDOWN||message==WM_LBUTTONDBLCLK||message==WM_RBUTTONDOWN){
        // Mouse interaction should not leave keyboard focus rectangles behind.
        if(!(SendMessageW(self->hwnd_,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS))
            SendMessageW(self->hwnd_,WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);
    }
    if(message==WM_MOUSEWHEEL){
        if(slider&&GetCapture()==control)return 0;
        const bool open=(control==self->input_||control==self->listener_||control==self->mediaApp_)&&SendMessageW(control,CB_GETDROPPEDSTATE,0,0);
        if(!open)return SendMessageW(self->hwnd_,message,w,l);
    }
    if(message==WM_MOUSEMOVE&&self->hoverControl_!=control){
        self->hoverControl_=control;TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,control,0};TrackMouseEvent(&track);InvalidateRect(control,nullptr,FALSE);
        if(slider)self->invalidateSliderValue(control);
    }
    if(message==WM_MOUSELEAVE){if(self->hoverControl_==control)self->hoverControl_=nullptr;InvalidateRect(control,nullptr,FALSE);if(slider)self->invalidateSliderValue(control);}
    if(slider){
        auto moveThumb=[&](LPARAM point){
            RECT bounds{};GetClientRect(control,&bounds);
            const float width=std::max(1.f,(vertical?bounds.bottom:bounds.right)/self->scale()-2*sliderEdge);
            const float position=((vertical?GET_Y_LPARAM(point):GET_X_LPARAM(point))/self->scale()-self->sliderGrab_-sliderEdge)/width;
            const float fraction=std::clamp(vertical?1.f-position:position,0.f,1.f);
            const int low=int(SendMessageW(control,TBM_GETRANGEMIN,0,0));
            const int high=int(SendMessageW(control,TBM_GETRANGEMAX,0,0));
            const int value=low+int(std::lround(fraction*(high-low)));
            if(value!=SendMessageW(control,TBM_GETPOS,0,0)){
                SendMessageW(control,TBM_SETPOS,TRUE,value);
                SendMessageW(GetParent(control),vertical?WM_VSCROLL:WM_HSCROLL,MAKEWPARAM(TB_THUMBTRACK,value),reinterpret_cast<LPARAM>(control));
            }
        };
        if((message==WM_LBUTTONDOWN||message==WM_LBUTTONDBLCLK)&&IsWindowEnabled(control)){
            RECT bounds{};GetClientRect(control,&bounds);
            const float x=GET_X_LPARAM(l)/self->scale(),y=GET_Y_LPARAM(l)/self->scale();
            const float center=vertical?sliderEdge+(bounds.bottom/self->scale()-2*sliderEdge)*(1.f-float(SendMessageW(control,TBM_GETPOS,0,0))/100.f):self->sliderThumbX(control);
            // Preserve the grab point on the handle; track clicks jump immediately.
            self->sliderGrab_=vertical?(std::abs(y-center)<=12.f?y-center:0.f):(std::abs(x-center)<=12.f&&std::abs(y-bounds.bottom/(2*self->scale()))<=12.f?x-center:0.f);
            SetFocus(control);SetCapture(control);moveThumb(l);
            InvalidateRect(control,nullptr,FALSE);self->invalidateSliderValue(control);return 0;
        }
        if(message==WM_MOUSEMOVE&&GetCapture()==control){moveThumb(l);return 0;}
        if(message==WM_LBUTTONUP&&GetCapture()==control){
            moveThumb(l);ReleaseCapture();
            SendMessageW(GetParent(control),vertical?WM_VSCROLL:WM_HSCROLL,MAKEWPARAM(TB_ENDTRACK,0),reinterpret_cast<LPARAM>(control));return 0;
        }
        if((message==WM_CANCELMODE||message==WM_KILLFOCUS||(message==WM_ENABLE&&!w)||(message==WM_SHOWWINDOW&&!w))&&GetCapture()==control)
            ReleaseCapture();
    }
    const auto result=DefSubclassProc(control,message,w,l);
    if(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_ENABLE||message==BM_SETCHECK||message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_UPDATEUISTATE)
        InvalidateRect(control,nullptr,FALSE);
    if(slider&&(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_CAPTURECHANGED||message==WM_ENABLE))self->invalidateSliderValue(control);
    return result;
}
}
