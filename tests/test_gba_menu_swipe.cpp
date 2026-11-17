#include <SDL.h>
#include "ui.h"
#include "gba_glass_impl.h"
#include <cassert>

static bool Acted(const GbaGlass::Input& in) { return in.accept || in.left || in.right || in.back; }
static std::pair<int,int> Find(Menu& menu, int item, int w, int h)
{
    for (const auto& part : menu.parts)
        if (part.item==item && part.intent==UiInput::Intent::Set && part.value==1)
            return {part.x+part.w/2,part.y+part.h/2};
    for (int y=20; y<h; y+=4) for (int x=20; x<w; x+=4)
        if (menu.ItemAt(x,y)==item) return {x,y};
    assert(false); return {};
}
int main()
{
    for (auto size : {std::pair<int,int>{1280,720}, {720,1280}})
    {
        GbaGlass::Page page;
        page.style=GbaGlass::PAGE; page.title=std::to_string(size.first); page.selected=1;
        page.items.push_back({"BACK", ""});
        for(int i=0;i<50;i++) { page.items.push_back({"CHEAT "+std::to_string(i), "OFF"}); page.items.back().kind=GbaGlass::TOGGLE; }
        bool changed=false;
        GbaGlass::Draw(page,size.first,size.second,changed);
        auto& menu=GbaGlass::S().menu;
        auto point=Find(menu,5,size.first,size.second);
        GbaGlass::Input in; int slider=-1,pick=-1;
        GbaGlass::Touch(page,GbaGlass::TOUCH_DOWN,7,point.first,point.second,in,slider,pick);
        assert(!Acted(in));
        GbaGlass::Touch(page,GbaGlass::TOUCH_MOVE,8,point.first,point.second-100,in,slider,pick);
        assert(menu.scrollPx==0); // another finger cannot take over the drag
        GbaGlass::Touch(page,GbaGlass::TOUCH_MOVE,7,point.first,point.second-100,in,slider,pick);
        assert(menu.scrollPx==100 && !Acted(in));
        GbaGlass::Touch(page,GbaGlass::TOUCH_UP,7,point.first,point.second-100,in,slider,pick);
        assert(!Acted(in) && page.selected==1);
        GbaGlass::Draw(page,size.first,size.second,changed);
        point=Find(menu,5,size.first,size.second);
        GbaGlass::Touch(page,GbaGlass::TOUCH_DOWN,9,point.first,point.second,in,slider,pick);
        assert(!Acted(in));
        GbaGlass::Touch(page,GbaGlass::TOUCH_UP,9,point.first,point.second,in,slider,pick);
        assert(Acted(in) && page.selected==5); // a short tap still acts, once on release
        in={};
        GbaGlass::Touch(page,GbaGlass::TOUCH_DOWN,10,point.first,point.second,in,slider,pick);
        GbaGlass::CancelTouch();
        GbaGlass::Touch(page,GbaGlass::TOUCH_UP,10,point.first,point.second,in,slider,pick);
        assert(!Acted(in));
        GbaGlass::Touch(page,GbaGlass::TOUCH_DOWN,11,point.first,point.second,in,slider,pick);
        page.title+=" next";
        GbaGlass::Touch(page,GbaGlass::TOUCH_UP,11,point.first,point.second,in,slider,pick);
        assert(!Acted(in)); // a gesture from the old page cannot activate the new page
        GbaGlass::Draw(page,size.first,size.second,changed);
        point=Find(menu,5,size.first,size.second);
        GbaGlass::Touch(page,GbaGlass::TOUCH_DOWN,12,point.first,point.second,in,slider,pick);
        GbaGlass::Frame(nullptr,0,0);
        GbaGlass::Touch(page,GbaGlass::TOUCH_UP,12,point.first,point.second,in,slider,pick);
        assert(!Acted(in)); // reopening pause cannot finish an old touch
        page.style=GbaGlass::DIALOG; page.title="QUIT?"; page.items={{"QUIT",""},{"CANCEL",""}};
        GbaGlass::Draw(page,size.first,size.second,changed);
        const int dx=menu.dialogX[1]+menu.dialogW/2, dy=menu.dialogY+menu.dialogH/2;
        GbaGlass::Touch(page,GbaGlass::TOUCH_DOWN,13,dx,dy,in,slider,pick);
        assert(Acted(in)); // non-scrolling confirmation buttons keep their original tap behavior
        in={};
        GbaGlass::Touch(page,GbaGlass::TOUCH_UP,13,dx,dy,in,slider,pick);
        assert(!Acted(in));
    }
}
