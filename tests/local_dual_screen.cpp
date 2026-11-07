#include "android_second_screen.h"
#include "layout.h"
#include "ui.h"
#include <cassert>
#include <iostream>
int main()
{
    using namespace DsDisplayState;
    int x=-1,y=-1;
    assert(!Physical(0));
    assert(!Physical(2ULL<<28)); // Display detected but no frame drawn yet: retain the normal view.
    assert(Physical((2ULL<<28)|Ready));
    assert(!Physical((1ULL<<28)|Ready));
    assert(Physical((3ULL<<28)|Ready)&&Swapped((3ULL<<28)|Ready));
    assert(!Touch((3ULL<<28)|Ready|(1ULL<<24),x,y));
    assert(!Physical(Ready));
    const uint64_t down=(1ULL<<24)|(191<<12)|255;
    assert(!Touch(down,x,y));
    assert(!Touch((1ULL<<28)|down,x,y));
    assert(!Touch(Ready|down,x,y));
    assert(Touch((1ULL<<28)|Ready|down,x,y) && x==255 && y==191);
    assert(!Touch((1ULL<<28)|Ready|(1ULL<<24)|(192<<12),x,y));
    assert(!Touch((2ULL<<28)|Ready,x,y)); // Finger lifted or cancelled.
    for(int mode=0;mode<6;mode++) {
        Layout saved;saved.mode=(LayoutMode)mode;
        for(uint64_t state : {0ULL,2ULL<<28,(2ULL<<28)|Ready,(1ULL<<28)|Ready}) {
            Layout effective=saved;
            if(Physical(state))effective.mode=LayoutMode::Top;
            SDL_Rect rects[2];LayoutRects(effective,1280,720,rects);
            assert(saved.mode==(LayoutMode)mode);
            if(Physical(state))assert(rects[0].w>0 && rects[1].w==0);
            else assert(effective.mode==saved.mode);
        }
    }
    for (int mode = 0; mode < 5; ++mode)
        assert(Canvas::TextWidth("SECOND SCREEN", 1)
            + Canvas::TextWidth(std::string("< ") + ModeName(mode) + " >", 1) + 20 <= 190);
    AndroidSecondScreen desktop(false);
    assert(desktop.State()==0);desktop.SetMode(2);desktop.Submit(nullptr);desktop.Stop();
    std::cout<<"PASS: readiness fallback, input gating/release, all saved layouts and desktop no-op\n";
}
