#include "companion_types.h"
#include "ui.h"
#include <cassert>
#include <cstdio>
#include <fstream>
int main(int argc,char** argv)
{
    using namespace CompanionTypes;
    assert(Multiplier(13,11,2)==400); // Electric against Water/Flying.
    assert(Multiplier(13,11,4)==0); // Ground immunity overrides Water weakness.
    assert(Multiplier(10,11,16)==25); // Fire against Water/Dragon.
    assert(Multiplier(10,12,12)==200); // A repeated type is counted once.
    assert(Multiplier(7,8)==50 && Multiplier(17,8)==50); // Pre-Fairy Steel chart.
    assert(Multiplier(0,7)==0 && Multiplier(1,7)==0);
    assert(Multiplier(15,16,4)==400);
    assert(Cycle(8,1)==10 && Cycle(10,-1)==8);
    assert(Cycle(0,-1)==17 && Cycle(17,1)==0);
    assert(Cycle(0,-1,true)==None && Cycle(None,1,true)==0 && Cycle(None,-1,true)==17);
    for(int a=0;a<18;a++) for(int d=0;d<18;d++)
        assert(Multiplier(a,d)==Multiplier(a,d,None));
    Menu menu;menu.open=true;
    menu.items.push_back({"MANUAL TYPE CHART - GEN 3-5","",true});
    menu.items.push_back({"NO ABILITIES / ITEMS / MOVE EXCEPTIONS","",true});
    menu.items.push_back({"DEFENDER TYPE 1","WATER"});
    menu.items.push_back({"DEFENDER TYPE 2","FLYING"});
    for(int a=0;a<18;a++) if(Valid(a)) menu.items.push_back({Name(a),Label(Multiplier(a,11,2)),true});
    menu.items.push_back({"BASED ON POKEDAISY","",true});
    menu.items.push_back({"BY LIDOR ITZHARI","",true});
    menu.items.push_back({"BACK",""});
    for(auto size: {std::pair<int,int>{1280,720},{720,1280},{640,480}})
    {
        Canvas c;c.Resize(size.first,size.second);c.Clear();
        for(int i=0;i<(int)menu.items.size();i++)
        {
            menu.selected=i;
            menu.Draw(c,"TYPE MATCHUPS","SELECT / BACK","",260);
            assert(menu.scroll<=i && menu.drawnW<=c.w && menu.drawnH<=c.h);
        }
        if(argc>1 && size.first==1280)
        {
            menu.selected=2;menu.scroll=0;menu.Draw(c,"TYPE MATCHUPS","SELECT / BACK","",260);
            std::ofstream out(argv[1],std::ios::binary);out<<"P6\n"<<c.w<<" "<<c.h<<"\n255\n";
            for(auto p:c.px){out.put((p>>16)&255);out.put((p>>8)&255);out.put(p&255);}
        }
    }
    puts("PASS: dual types, immunities, original Steel rules, cycling and menu rendering at three sizes");
}
