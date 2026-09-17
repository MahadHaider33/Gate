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
    bool dark=false;
    try {if(settings_){auto fg=settings_.GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Foreground);dark=(5*fg.G+2*fg.R+fg.B)>8*128;}}catch(...){}
    palette_=dark?Palette{color(0x14191d),color(0x191f24),color(0x1c2227),color(0x272e35),color(0xf5f6f7),color(0xb3c0d4),color(0x2386ff),color(0x30373e),color(0x37424c),color(0x20dd77),color(0x25313c),color(0xf0bb68),color(0xffffff),true,false}
        :Palette{color(0xfbf8f2),color(0xfffcf7),color(0xf7f3ea),color(0xfffcf6),color(0x291d16),color(0x6d645e),color(0xa77a24),color(0xe5ded2),color(0xe0dbd1),color(0xc4a045),color(0xf3ecdf),color(0x8c5619),color(0xffffff),false,false};
    HIGHCONTRASTW hc{sizeof(hc)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0);
    if(hc.dwFlags&HCF_HIGHCONTRASTON){
        palette_={system(COLOR_WINDOW),system(COLOR_WINDOW),system(COLOR_WINDOW),system(COLOR_WINDOW),system(COLOR_WINDOWTEXT),system(COLOR_WINDOWTEXT),system(COLOR_HIGHLIGHT),system(COLOR_WINDOWTEXT),system(COLOR_GRAYTEXT),system(COLOR_HIGHLIGHT),system(COLOR_HIGHLIGHT),system(COLOR_WINDOWTEXT),system(COLOR_HIGHLIGHTTEXT),dark,true};
    }
}
}
