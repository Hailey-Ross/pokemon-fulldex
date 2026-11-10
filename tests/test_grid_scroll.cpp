#include "grid_scroll.h"
#include <cassert>
#include <cstdio>
int main(){
    for(int interval:{8,16,33}){
        GridScrollMotion s;int value=100;
        s.Wheel(80,value,0);
        assert(s.Active());
        s.Advance(interval,1000,value);assert(value>100&&value<130);
        for(int t=interval*2;t<1500;t+=interval)s.Advance(t,1000,value);
        assert(value>=178&&value<=181); // Similar travel at 30/60/120 Hz, without row snapping.
        s.Wheel(-500,value,1600);s.Advance(1616,1000,value);
        for(int t=1632;t<3000;t+=16)s.Advance(t,1000,value);assert(value==0);
        s.Begin(200,3000);s.Drag(260,3020);value=260;s.End(3020);
        s.Advance(3036,1000,value);assert(value>260);
        s.Begin(value,3040);assert(!s.Active());const int stopped=value;s.Advance(3060,1000,value);assert(value==stopped);
        s.Drag(value+50,3080);value+=50;s.End(3300);s.Advance(3316,1000,value);assert(value==stopped+50);
        s.Wheel(500,value,3400);s.Advance(3800,1000,value);assert(value==stopped+50);
        s.Wheel(500,value,3900);s.Advance(3916,0,value);assert(value==0);
        s.Wheel(500,value,4000);s.Stop();s.Advance(4016,1000,value);assert(value==0);
    }
    puts("Scroll: frame-independent coasting, bounds and cancellation");
}
