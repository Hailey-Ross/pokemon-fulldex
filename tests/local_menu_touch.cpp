#include "ui.h"
#include <cassert>
#include <iostream>
int main(){
 for(auto size:{std::pair<int,int>{1280,720},{2992,1344},{1080,2400}}){
  Canvas c;c.Resize(size.first,size.second);Menu m;m.open=true;
  m.items={{"TEST PARTY","L5 21/21",true},{"FAST FORWARD","OFF",false},{"VOLUME","100%",false,10}};
  m.Draw(c,"TEST","BACK",{},260);
  auto rowY=[&](int row){return m.drawnFirstY+row*m.drawnLineH+m.drawnLineH/2;};
  for(int x:{m.drawnX+5,m.drawnX+m.drawnW-5}){MenuInput input;int slider=-1,dir=99;m.Tap(x,rowY(0),input,slider);assert(input.accept&&!input.left&&!input.right);assert(m.Update(input,dir)==0&&dir==0);}
  m.items[0]={"SLOT 1","2026-11-02 12:00",true};MenuInput slot;int slider=-1,dir=0;m.Tap(m.drawnX+5,rowY(0),slot,slider);assert(slot.accept&&m.Update(slot,dir)==0&&dir==0);
  MenuInput left;m.Tap(m.drawnX+5,rowY(1),left,slider);assert(left.left&&!left.accept&&m.Update(left,dir)==1&&dir==-1);
  MenuInput right;m.Tap(m.drawnX+m.drawnW-5,rowY(1),right,slider);assert(right.right&&!right.accept&&m.Update(right,dir)==1&&dir==1);
  MenuInput volume;m.Tap(m.barX+4*m.barStep,rowY(2),volume,slider);assert(volume.accept&&slider==5);
  MenuInput outside;m.Tap(m.drawnX-1,rowY(0),outside,slider);assert(outside.back);
 }
 std::cout<<"PASS: party/state information rows activate by touch; settings, sliders and outside-back retain their behaviour across three screen sizes\n";
}
