#pragma once
#include <windows.h>
#include <vector>

namespace gate {
// Defer geometry/visibility changes until the whole page is ready, then let
// the normal paint queue draw it once. Never force a paint per control.
class LayoutBatch {
public:
    explicit LayoutBatch(int count){changes_.reserve(count);}
    ~LayoutBatch(){
        if(changes_.empty())return;
        HDWP batch=BeginDeferWindowPos(int(changes_.size()));
        for(const auto& c:changes_){
            if(!batch)break;
            batch=DeferWindowPos(batch,c.control,nullptr,c.x,c.y,c.width,c.height,c.flags);
        }
        if(batch&&EndDeferWindowPos(batch))return;
        // A failed deferred batch discards its earlier operations; replay all.
        for(const auto& c:changes_)SetWindowPos(c.control,nullptr,c.x,c.y,c.width,c.height,c.flags);
    }
    void show(HWND control,bool visible){
        if(!control||bool(GetWindowLongPtrW(control,GWL_STYLE)&WS_VISIBLE)==visible)return;
        apply(control,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|(visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
    }
    void move(HWND control,int x,int y,int width,int height){
        RECT current{};GetWindowRect(control,&current);
        MapWindowPoints(nullptr,GetParent(control),reinterpret_cast<POINT*>(&current),2);
        if(current.left==x&&current.top==y&&current.right-current.left==width&&current.bottom-current.top==height)return;
        apply(control,x,y,width,height,0);
    }
private:
    void apply(HWND control,int x,int y,int width,int height,UINT flags){
        flags|=SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOCOPYBITS;
        changes_.push_back({control,x,y,width,height,flags});
    }
    struct Change {HWND control;int x,y,width,height;UINT flags;};
    std::vector<Change> changes_;
};
}
