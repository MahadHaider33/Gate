#include "ui/Window.h"
#include <winrt/base.h>
#include <commctrl.h>
#include <exception>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\Gate.Desktop.0.1");
    if(!singleton)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){
        if(auto existing=FindWindowW(L"GateDesktopWindow",nullptr)) {ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);}
        CloseHandle(singleton);return 0;
    }
    int result=1;
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};InitCommonControlsEx(&controls);
        gate::Window window(instance);result=window.run(show);
    } catch(...) { MessageBoxW(nullptr,L"Gate could not start. Check that Windows audio is available and reinstall Gate if the problem continues.",L"Gate",MB_OK|MB_ICONERROR); }
    CloseHandle(singleton);return result;
}
