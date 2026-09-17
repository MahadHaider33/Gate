#include "ui/Window.h"
#include <algorithm>
#include <cmath>

namespace gate {
namespace {
constexpr UINT scrollTimer=3;
constexpr float processingTop=237,rowHeight=140,rowGap=12,rowInset=20;
constexpr float headingOffset=16,headingHeight=28,descriptionOffset=48,descriptionHeight=24;
constexpr float sliderOffset=82,sliderInset=rowInset-12;
constexpr float suppressionTop=processingTop,gateTop=processingTop+rowHeight+rowGap;
constexpr float strengthSliderTop=suppressionTop+sliderOffset,gateSliderTop=gateTop+sliderOffset;
}
float Window::maxScroll() const {return voice_?0.f:std::max(0.f,contentHeight-viewportHeight_);}
void Window::layout() {
    RECT client{};GetClientRect(hwnd_,&client);width_=client.right/scale();height_=client.bottom/scale();
    sidebar_=width_<900?200.f:240.f;contentWidth_=std::max(380.f,width_-sidebar_-64);viewportHeight_=std::max(1.f,height_-headerHeight-8);
    scroll_=std::clamp(scroll_,0.f,maxScroll());scrollTarget_=std::clamp(scrollTarget_,0.f,maxScroll());
    KillTimer(hwnd_,scrollTimer);scrolling_=false;scrollTarget_=scroll_;
    MoveWindow(microphoneNav_,px(12),px(88),px(sidebar_-24),px(46),TRUE);
    MoveWindow(voiceNav_,px(12),px(142),px(sidebar_-24),px(46),TRUE);
    MoveWindow(content_,px(sidebar_+32),px(headerHeight),px(contentWidth_),px(viewportHeight_),FALSE);
    ShowWindow(content_,voice_?SW_HIDE:SW_SHOW);
    positionControls();
    auto resize=[](ID2D1HwndRenderTarget* target,UINT w,UINT h){if(target){auto s=target->GetPixelSize();if(s.width!=w||s.height!=h)target->Resize(D2D1::SizeU(w,h));}};
    resize(target_.Get(),client.right,client.bottom);resize(contentTarget_.Get(),px(contentWidth_),px(viewportHeight_));
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::positionControls() {
    const float span=contentWidth_,column=(span-24)/2;
    HDWP batch=BeginDeferWindowPos(8);
    auto pos=[&](HWND h,float x,float y,float w,float height){batch=DeferWindowPos(batch,h,nullptr,px(x),px(top(y)),px(w),px(height),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOREDRAW);};
    pos(input_,0,42,column,260);pos(listener_,column+24,42,column,230);
    pos(test_,0,116,174,42);pos(setup_,span-117,195,117,24);
    auto processingRow=[&](HWND toggle,HWND slider,float y){
        pos(toggle,span-rowInset-44,y+headingOffset,46,headingHeight);
        pos(slider,sliderInset,y+sliderOffset,span-2*sliderInset,30);
    };
    processingRow(suppression_,strength_,suppressionTop);
    processingRow(gate_,threshold_,gateTop);
    if(batch)EndDeferWindowPos(batch);
    ShowWindow(setup_,state_.devices.cables.empty()?SW_SHOW:SW_HIDE);
    RedrawWindow(content_,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
    RECT bar{px(width_-22),px(headerHeight),px(width_),px(height_)};InvalidateRect(hwnd_,&bar,FALSE);
}
void Window::scrollTo(float position,bool animate) {
    scrollTarget_=std::clamp(position,0.f,maxScroll());
    BOOL effects=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&effects,0);
    if(!animate||!effects){KillTimer(hwnd_,scrollTimer);scrolling_=false;scroll_=scrollTarget_;positionControls();return;}
    scrollFrom_=scroll_;scrollStarted_=GetTickCount64();scrolling_=true;SetTimer(hwnd_,scrollTimer,16,nullptr);
}
void Window::animateScroll() {
    const float t=std::min(1.f,float(GetTickCount64()-scrollStarted_)/160.f);
    const float ease=1.f-(1.f-t)*(1.f-t)*(1.f-t);
    scroll_=scrollFrom_+(scrollTarget_-scrollFrom_)*ease;
    if(t>=1){scroll_=scrollTarget_;scrolling_=false;KillTimer(hwnd_,scrollTimer);}
    positionControls();
}
void Window::paint() {
    PAINTSTRUCT ps{};BeginPaint(hwnd_,&ps);
    if(!target_){RECT r{};GetClientRect(hwnd_,&r);
        auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,D2D1::PixelFormat(),float(dpi_),float(dpi_));
        if(FAILED(factory_->CreateHwndRenderTarget(properties,D2D1::HwndRenderTargetProperties(hwnd_,D2D1::SizeU(r.right,r.bottom)),&target_))){EndPaint(hwnd_,&ps);return;}}
    const auto& p=theme_.colors();target_->BeginDraw();target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    target_->PushAxisAlignedClip({ps.rcPaint.left/scale(),ps.rcPaint.top/scale(),ps.rcPaint.right/scale(),ps.rcPaint.bottom/scale()},D2D1_ANTIALIAS_MODE_ALIASED);
    target_->Clear(p.background);ComPtr<ID2D1SolidColorBrush> brush;target_->CreateSolidColorBrush(p.sidebar,&brush);
    target_->FillRectangle({0,0,sidebar_,height_},brush.Get());brush->SetColor(p.border);target_->FillRectangle({sidebar_-1,0,sidebar_,height_},brush.Get());
    constexpr float heights[]={9,20,30,18,8};brush->SetColor(p.accent);
    for(int i=0;i<5;++i)target_->FillRoundedRectangle(D2D1::RoundedRect({25.f+i*5,48-heights[i]/2,28.f+i*5,48+heights[i]/2},1.5f,1.5f),brush.Get());
    label(target_.Get(),L"Gate",{64,27,sidebar_-16,59},21,p.text,true);
    if(paused_||!state_.capturing)label(target_.Get(),paused_?L"Processing paused":L"Microphone unavailable",{24,height_-44,sidebar_-18,height_-18},11.5f,p.secondary);
    const float left=sidebar_+32;
    label(target_.Get(),voice_?L"Voice":L"Microphone",{left,26,width_-32,68},30,p.text,true);
    label(target_.Get(),voice_?L"Voice effects are not available yet.":L"Reduce background noise from your microphone.",{left,72,width_-32,99},15.5f,p.secondary);
    if(maxScroll()>0){
        const float thumb=std::max(32.f,viewportHeight_*viewportHeight_/contentHeight);
        const float y=headerHeight+(viewportHeight_-thumb)*scroll_/maxScroll();
        brush->SetColor(draggingScroll_?p.secondary:p.track);
        target_->FillRoundedRectangle(D2D1::RoundedRect({width_-14,y,width_-9,y+thumb},2.5f,2.5f),brush.Get());
    }
    target_->PopAxisAlignedClip();if(target_->EndDraw()==D2DERR_RECREATE_TARGET)target_.Reset();EndPaint(hwnd_,&ps);
}
void Window::paintContent() {
    PAINTSTRUCT ps{};BeginPaint(content_,&ps);
    if(!contentTarget_){RECT r{};GetClientRect(content_,&r);
        auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,D2D1::PixelFormat(),float(dpi_),float(dpi_));
        if(FAILED(factory_->CreateHwndRenderTarget(properties,D2D1::HwndRenderTargetProperties(content_,D2D1::SizeU(r.right,r.bottom)),&contentTarget_))){EndPaint(content_,&ps);return;}}
    auto* target=contentTarget_.Get();const auto& p=theme_.colors();const float span=contentWidth_;
    target->BeginDraw();target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    target->PushAxisAlignedClip({ps.rcPaint.left/scale(),ps.rcPaint.top/scale(),ps.rcPaint.right/scale(),ps.rcPaint.bottom/scale()},D2D1_ANTIALIAS_MODE_ALIASED);
    target->Clear(p.background);ComPtr<ID2D1SolidColorBrush> brush;target->CreateSolidColorBrush(p.card,&brush);
    auto text=[&](const std::wstring& s,float x,float y,float w,float h,float size,D2D1_COLOR_F c,bool bold=false,bool wrap=false){label(target,s,{x,top(y),x+w,top(y+h)},size,c,bold,DWRITE_TEXT_ALIGNMENT_LEADING,wrap);};
    const float column=(span-24)/2;
    text(L"Microphone",0,8,column,26,16,p.text,true);
    text(L"Listening Device",column+24,8,column,26,16,p.text,true);
    const float input=state_.testActive&&state_.capturing?engine_.diagnostics().inputLevel.load(std::memory_order_relaxed):0.f;
    const float level=std::clamp((20.f*std::log10(std::max(input,.000001f))+60.f)/60.f,0.f,1.f);
    const int segments=std::max(12,int((span-198)/8));const float step=(span-198)/segments;
    for(int i=0;i<segments;++i){brush->SetColor(float(i)/segments<level?p.meter:p.track);target->FillRoundedRectangle(D2D1::RoundedRect({198+i*step,top(124),198+i*step+step-3,top(150)},2.f,2.f),brush.Get());}
    const auto message=state_.testActive?L"Live test active":state_.testMessage;
    text(message,0,170,span,22,12,state_.testActive?p.accent:p.secondary);
    const bool routeReady=state_.capturing&&state_.cableActive;
    const float statusWidth=span-(state_.devices.cables.empty()?129.f:0.f);
    const auto statusBounds=D2D1::RoundedRect({.5f,top(194)+.5f,statusWidth-.5f,top(225)-.5f},7,7);
    brush->SetColor(p.card);target->FillRoundedRectangle(statusBounds,brush.Get());
    brush->SetColor(p.border);target->DrawRoundedRectangle(statusBounds,brush.Get(),1);
    if(routeReady){
        text(L"Ready",12,194,42,31,11.5f,p.text,true);
        brush->SetColor(p.border);target->DrawLine({65,top(203)},{65,top(216)},brush.Get(),1);
        text(L"Select CABLE Output as your microphone in other apps.",77,194,statusWidth-89,31,11.5f,p.secondary);
    }else{
        text(state_.routeMessage,12,194,statusWidth-24,31,11.5f,p.secondary);
    }
    auto section=[&](const wchar_t* heading,const wchar_t* description,float y){
        const auto bounds=D2D1::RoundedRect({.5f,top(y)+.5f,span-.5f,top(y+rowHeight)-.5f},10,10);
        brush->SetColor(p.card);target->FillRoundedRectangle(bounds,brush.Get());
        brush->SetColor(p.border);target->DrawRoundedRectangle(bounds,brush.Get(),1);
        text(heading,rowInset,y+headingOffset,span-2*rowInset-62,headingHeight,16,p.text,true);
        text(description,rowInset,y+descriptionOffset,span-2*rowInset,descriptionHeight,13,p.secondary);
    };
    section(L"Background Noise Reduction",L"Reduce fan, keyboard, and room noise.",suppressionTop);
    section(L"Noise Gate",L"Mute background sounds when you’re not speaking.",gateTop);
    auto sliderValue=[&](HWND slider,float y,int value){
        if(!IsWindowEnabled(slider)||(hoverControl_!=slider&&GetCapture()!=slider&&GetFocus()!=slider))return;
        const float x=sliderInset+sliderThumbX(slider);
        const float center=std::clamp(x,rowInset+30,span-rowInset-30);
        const auto bubble=D2D1::RoundedRect({center-29,top(y-39),center+29,top(y-7)},7,7);
        brush->SetColor(p.field);target->FillRoundedRectangle(bubble,brush.Get());
        brush->SetColor(p.border);target->DrawRoundedRectangle(bubble,brush.Get(),1);
        ComPtr<ID2D1PathGeometry> arrow;factory_->CreatePathGeometry(&arrow);ComPtr<ID2D1GeometrySink> sink;arrow->Open(&sink);
        sink->BeginFigure({center-6,top(y-8)},D2D1_FIGURE_BEGIN_FILLED);sink->AddLine({x,top(y)});sink->AddLine({center+6,top(y-8)});sink->EndFigure(D2D1_FIGURE_END_CLOSED);sink->Close();
        brush->SetColor(p.field);target->FillGeometry(arrow.Get(),brush.Get());
        label(target,std::to_wstring(value)+L"%",{center-29,top(y-39),center+29,top(y-7)},13,p.text,true,DWRITE_TEXT_ALIGNMENT_CENTER);
    };
    sliderValue(strength_,strengthSliderTop,int(preferences_.processing.strength));
    sliderValue(threshold_,gateSliderTop,(preferences_.processing.thresholdDb+70)*2);
    target->PopAxisAlignedClip();if(target->EndDraw()==D2DERR_RECREATE_TARGET)contentTarget_.Reset();EndPaint(content_,&ps);
}
LRESULT CALLBACK Window::contentProcedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(window,message,w,l);
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT){try{self->paintContent();}catch(...){}return 0;}
    if(message==WM_COMMAND||message==WM_NOTIFY||message==WM_DRAWITEM||message==WM_MEASUREITEM||message==WM_HSCROLL||message==WM_MOUSEWHEEL||message==WM_CTLCOLORLISTBOX||message==WM_CTLCOLORSTATIC||message==WM_CTLCOLORBTN)
        return SendMessageW(self->hwnd_,message,w,l);
    return DefWindowProcW(window,message,w,l);
}
}
