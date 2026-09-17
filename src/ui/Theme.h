#pragma once
#include <windows.h>
#include <d2d1.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <functional>
namespace gate {
inline constexpr UINT themeChanged=WM_APP+21;
struct Palette {
    D2D1_COLOR_F background, sidebar, card, field, text, secondary, accent, border, track, meter, selected, warning, buttonText;
    bool dark=false, highContrast=false;
};
class Theme {
public:
    void connect(HWND window);
    void refresh();
    const Palette& colors() const { return palette_; }
private:
    Palette palette_{};
    winrt::Windows::UI::ViewManagement::UISettings settings_{nullptr};
    winrt::Windows::UI::ViewManagement::UISettings::ColorValuesChanged_revoker revoker_;
};
COLORREF rgb(D2D1_COLOR_F color);
}
