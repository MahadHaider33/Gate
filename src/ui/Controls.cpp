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
    auto& format=textFormats_[unsigned(size*2)][bold?1:0];
    if(!format)check(textFactory_->CreateTextFormat(L"Segoe UI",nullptr,
        bold?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&format));
    ComPtr<IDWriteTextLayout> layout;
    check(textFactory_->CreateTextLayout(text.c_str(),UINT32(text.size()),format.Get(),box.right-box.left,box.bottom-box.top,&layout));
    layout->SetTextAlignment(alignment);
    layout->SetParagraphAlignment(wrap?DWRITE_PARAGRAPH_ALIGNMENT_NEAR:DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    layout->SetWordWrapping(wrap?DWRITE_WORD_WRAPPING_WRAP:DWRITE_WORD_WRAPPING_NO_WRAP);
    if(!wrap){
        DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};
        ComPtr<IDWriteInlineObject> ellipsis;
        textFactory_->CreateEllipsisTrimmingSign(format.Get(),&ellipsis);
        layout->SetTrimming(&trim,ellipsis.Get());
    }
    ComPtr<ID2D1SolidColorBrush> brush;target->CreateSolidColorBrush(color,&brush);
    target->DrawTextLayout(D2D1::Point2F(box.left,box.top),layout.Get(),brush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
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
    const bool nav=control==microphoneNav_||control==voiceNav_;
    const bool combo=control==input_||control==listener_;
    const bool slider=control==strength_||control==threshold_;
    const bool toggle=control==suppression_||control==gate_;
    const bool enabled=IsWindowEnabled(control)!=FALSE;
    const bool hover=hoverControl_==control&&enabled;
    const bool pressed=(SendMessageW(control,BM_GETSTATE,0,0)&BST_PUSHED)!=0;
    const auto background=nav?p.sidebar:(slider||toggle)?p.card:p.background;
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
        else wcscpy_s(text,control==input_?L"Select a microphone":L"Select a listening device");
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
        auto track=on?p.accent:p.track;if(hover)track=blend(track,p.text,.10f);
        round({2,3,w-2,h-3},(h-6)/2,track);
        const float radius=(h-12)/2;const float x=on?w-5-radius:5+radius;
        brush->SetColor(p.highContrast?p.buttonText:D2D1::ColorF(D2D1::ColorF::White));
        target->FillEllipse(D2D1::Ellipse({x,h/2},radius,radius),brush.Get());
    }else if(slider){
        const float x=sliderThumbX(control),y=h/2;
        const auto left=enabled?p.accent:p.secondary;
        const auto right=p.track;
        constexpr float halfTrack=3.f;
        round({sliderEdge,y-halfTrack,w-sliderEdge,y+halfTrack},halfTrack,right);
        if(x>sliderEdge)round({sliderEdge,y-halfTrack,x,y+halfTrack},halfTrack,left);
        constexpr float radius=10.f;
        brush->SetColor(blend(p.card,p.text,.12f));target->FillEllipse(D2D1::Ellipse({x,y+1},radius+.5f,radius+.5f),brush.Get());
        brush->SetColor(enabled?(p.highContrast?p.text:D2D1::ColorF(0xfafafa)):p.secondary);
        target->FillEllipse(D2D1::Ellipse({x,y},radius,radius),brush.Get());
    }else if(nav){
        const bool selected=(control==voiceNav_)==voice_;
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
    const bool slider=control==self->strength_||control==self->threshold_;
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
        SendMessageW(self->hwnd_,WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);
    }
    if(message==WM_MOUSEWHEEL){
        if(slider&&GetCapture()==control)return 0;
        const bool open=(control==self->input_||control==self->listener_)&&SendMessageW(control,CB_GETDROPPEDSTATE,0,0);
        if(!open)return SendMessageW(self->hwnd_,message,w,l);
    }
    if(message==WM_MOUSEMOVE&&self->hoverControl_!=control){
        self->hoverControl_=control;TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,control,0};TrackMouseEvent(&track);InvalidateRect(control,nullptr,FALSE);
        if(slider)InvalidateRect(self->content_,nullptr,FALSE);
    }
    if(message==WM_MOUSELEAVE){if(self->hoverControl_==control)self->hoverControl_=nullptr;InvalidateRect(control,nullptr,FALSE);if(slider)InvalidateRect(self->content_,nullptr,FALSE);}
    if(slider){
        auto moveThumb=[&](LPARAM point){
            RECT bounds{};GetClientRect(control,&bounds);
            const float width=std::max(1.f,bounds.right/self->scale()-2*sliderEdge);
            const float fraction=std::clamp((GET_X_LPARAM(point)/self->scale()-self->sliderGrab_-sliderEdge)/width,0.f,1.f);
            const int low=int(SendMessageW(control,TBM_GETRANGEMIN,0,0));
            const int high=int(SendMessageW(control,TBM_GETRANGEMAX,0,0));
            const int value=low+int(std::lround(fraction*(high-low)));
            if(value!=SendMessageW(control,TBM_GETPOS,0,0)){
                SendMessageW(control,TBM_SETPOS,TRUE,value);
                SendMessageW(GetParent(control),WM_HSCROLL,MAKEWPARAM(TB_THUMBTRACK,value),reinterpret_cast<LPARAM>(control));
            }
        };
        if((message==WM_LBUTTONDOWN||message==WM_LBUTTONDBLCLK)&&IsWindowEnabled(control)){
            RECT bounds{};GetClientRect(control,&bounds);
            const float x=GET_X_LPARAM(l)/self->scale(),y=GET_Y_LPARAM(l)/self->scale();
            const float center=self->sliderThumbX(control);
            // Preserve the grab point on the handle; track clicks jump immediately.
            self->sliderGrab_=std::abs(x-center)<=12.f&&std::abs(y-bounds.bottom/(2*self->scale()))<=12.f?x-center:0.f;
            SetFocus(control);SetCapture(control);moveThumb(l);
            InvalidateRect(control,nullptr,FALSE);InvalidateRect(self->content_,nullptr,FALSE);return 0;
        }
        if(message==WM_MOUSEMOVE&&GetCapture()==control){moveThumb(l);return 0;}
        if(message==WM_LBUTTONUP&&GetCapture()==control){
            moveThumb(l);ReleaseCapture();
            SendMessageW(GetParent(control),WM_HSCROLL,MAKEWPARAM(TB_ENDTRACK,0),reinterpret_cast<LPARAM>(control));return 0;
        }
        if((message==WM_CANCELMODE||message==WM_KILLFOCUS||(message==WM_ENABLE&&!w)||(message==WM_SHOWWINDOW&&!w))&&GetCapture()==control)
            ReleaseCapture();
    }
    const auto result=DefSubclassProc(control,message,w,l);
    if(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_ENABLE||message==BM_SETCHECK||message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_UPDATEUISTATE)
        InvalidateRect(control,nullptr,FALSE);
    if(slider&&(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_CAPTURECHANGED||message==WM_ENABLE))InvalidateRect(self->content_,nullptr,FALSE);
    return result;
}
}
