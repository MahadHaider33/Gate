#include "ui/Theme.h"
namespace gate {
namespace {
D2D1_COLOR_F color(unsigned rgb) { return D2D1::ColorF(rgb); }
D2D1_COLOR_F system(int id) { auto c=GetSysColor(id);return D2D1::ColorF(GetRValue(c)/255.f,GetGValue(c)/255.f,GetBValue(c)/255.f); }
}
COLORREF rgb(D2D1_COLOR_F c) {return RGB(int(c.r*255),int(c.g*255),int(c.b*255));}
void Theme::connect(HWND window) {
    try {
        settings_=winrt::Windows::UI::ViewManagement::UISettings();
        revoker_=settings_.ColorValuesChanged(winrt::auto_revoke,[window](auto const&,auto const&){PostMessageW(window,themeChanged,0,0);});
    } catch(...) {}
    refresh();
}
void Theme::refresh() {
    // Quiet Dark is Gate's selected visual theme, independent of the OS app theme.
    const bool dark=true;
    palette_={color(0x181b1e),color(0x15191b),color(0x202528),color(0x24292d),
        color(0xf1f4f3),color(0xa1a9ad),color(0x86efc2),color(0x343a3e),
        color(0x3c4248),color(0x86efc2),color(0x243b34),color(0xf08080),color(0x10241c),true,false};
    HIGHCONTRASTW hc{sizeof(hc)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0);
    if(hc.dwFlags&HCF_HIGHCONTRASTON){
        palette_={system(COLOR_WINDOW),system(COLOR_WINDOW),system(COLOR_WINDOW),system(COLOR_WINDOW),system(COLOR_WINDOWTEXT),system(COLOR_WINDOWTEXT),system(COLOR_HIGHLIGHT),system(COLOR_WINDOWTEXT),system(COLOR_GRAYTEXT),system(COLOR_HIGHLIGHT),system(COLOR_HIGHLIGHT),system(COLOR_WINDOWTEXT),system(COLOR_HIGHLIGHTTEXT),dark,true};
    }
}
}
