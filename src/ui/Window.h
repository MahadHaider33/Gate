#pragma once
#include "app/Preferences.h"
#include "audio/Engine.h"
#include "ui/Theme.h"
#include <dwrite.h>
#include <shellapi.h>
#include <commctrl.h>
#include <array>
namespace gate {
class Window {
public:
    explicit Window(HINSTANCE instance);
    int run(int show);
private:
    static constexpr float headerHeight=104,contentHeight=547;
    static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK contentProcedure(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK controlProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    LRESULT message(UINT,WPARAM,LPARAM);
    void createControls();
    void layout();
    void positionControls();
    void scrollTo(float position,bool animate=true);
    void animateScroll();
    float maxScroll() const;
    void paint();
    void paintContent();
    void paintControl(HWND,HDC);
    float sliderThumbX(HWND) const;
    bool beginControlPaint(HDC,const RECT&);
    void endControlPaint();
    void label(ID2D1RenderTarget*,const std::wstring&,D2D1_RECT_F,float,D2D1_COLOR_F,bool=false,DWRITE_TEXT_ALIGNMENT=DWRITE_TEXT_ALIGNMENT_LEADING,bool=false);
    void drawItem(const DRAWITEMSTRUCT& item);
    void refreshStatus();
    void populate(HWND combo,const std::vector<Device>& list,const std::wstring& id);
    void syncTest();
    void updateTheme();
    void hide();
    void trayMenu();
    void updateTimer();
    void save();
    void setPage(bool voice);
    float scale() const {return dpi_/96.f;}
    int px(float v) const {return int(v*scale()+.5f);}
    float top(float y) const {return y-float(scroll_);}
    HWND hwnd_{},content_{},input_{},listener_{},suppression_{},gate_{},strength_{},threshold_{},test_{},microphoneNav_{},voiceNav_{},setup_{},tooltip_{},hoverControl_{};
    HINSTANCE instance_;
    HFONT font_{},boldFont_{};
    HBRUSH fieldBrush_{};
    unsigned dpi_=96;
    float scroll_=0,scrollTarget_=0,scrollFrom_=0,viewportHeight_=0,contentWidth_=0;
    ULONGLONG scrollStarted_=0;
    bool scrolling_=false,draggingScroll_=false;
    float scrollGrab_=0;
    float sliderGrab_=0;
    float width_=1100,height_=710,sidebar_=240;
    bool voice_=false,paused_=false,testPending_=false,testRequested_=false,timerActive_=false;
    Preferences preferences_;
    Engine engine_;
    EngineStatus state_;
    Theme theme_;
    ComPtr<ID2D1Factory> factory_;
    ComPtr<ID2D1HwndRenderTarget> target_;
    ComPtr<ID2D1HwndRenderTarget> contentTarget_;
    ComPtr<ID2D1DCRenderTarget> controlTarget_;
    ComPtr<IDWriteFactory> textFactory_;
    std::array<std::array<ComPtr<IDWriteTextFormat>,2>,65> textFormats_{};
    NOTIFYICONDATAW tray_{};
};
}
