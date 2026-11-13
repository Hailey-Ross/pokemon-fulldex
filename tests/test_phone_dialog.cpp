#include "ui.h"
#include <cassert>
#include <cstdio>
int main() {
    for (auto size : {std::pair<int,int>{360,640}, {640,360}, {1080,2400}, {2400,1080}, {1280,720}}) {
        Canvas c; c.Resize(size.first,size.second);
        Menu m; m.open=true; m.style=2;
        m.items={{"QUIT GAME",""},{"CANCEL",""}}; m.selected=1;
        m.selected=17;
        int repairedDir=0;
        assert(m.Update(MenuInput{},repairedDir)==-1 && m.selected==1);
        MenuInput choose; choose.accept=true;
        assert(m.Update(choose,repairedDir)==1);
        m.touchRows=1;
        m.DrawDialog(c,"QUIT GAME?","Unsaved progress will be lost.","");
        const int oldHeight=m.dialogH;
        m.touchRows=1.5f;
        m.DrawDialog(c,"QUIT GAME?","Unsaved progress will be lost.","");
        assert(m.dialogH>oldHeight);
        assert(m.dialogH>=44);
        assert(m.dialogY>=0 && m.dialogY+m.dialogH<=c.h);
        for (int b=0;b<2;++b) {
            assert(m.dialogX[b]>=0 && m.dialogX[b]+m.dialogW<=c.w);
            MenuInput in; int slider=-1,dir=0;
            m.Tap(m.dialogX[b]+m.dialogW/2,m.dialogY+m.dialogH/2,in,slider);
            assert(in.accept);
            assert(m.Update(in,dir)==(b==0?1:0));
        }
        m.DrawDialog(c,"QUIT GAME?","Unsaved progress will be lost.","");
        assert(m.dialogH>=44);
        printf("PASS: %dx%d dialog and both touch targets\n",c.w,c.h);
    }
}
