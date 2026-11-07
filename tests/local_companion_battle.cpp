#include "companion_battle.h"
#include <cassert>
#include <iostream>
using namespace Companion;
static void move(Metadata& d,int id,int type,int category=0){if(d.moveRecords.size()<600)d.moveRecords.resize(600);auto& b=d.moveRecords[id];b.assign(7,0);b[2]=category;b[3]=category==2?0:60;if(d.gen5){b[0]=type>=10?type-1:type;b[4]=100;b[5]=20;}else{b[4]=type;b[5]=100;b[6]=20;}}
int main()
{
    Metadata d;d.items={"NONE","Iron Ball","Ring Target","Air Balloon"};
    Battler own,foe;own.type1=0;foe.type1=13;foe.type2=-1;foe.statusKnown=true;
    move(d,1,10);move(d,2,4);move(d,3,13);move(d,4,11);move(d,5,1);move(d,6,0);move(d,7,12);move(d,8,15);move(d,9,14,2);move(d,237,0);
    foe.ability=26;assert(CompanionBattle::Evaluate(2,d,own,foe).percent==0);
    own.ability=104;assert(CompanionBattle::Evaluate(2,d,own,foe).percent==200);own.ability=0;
    foe.item=1;assert(CompanionBattle::Evaluate(2,d,own,foe).percent==200);foe.item=0;
    foe.ability=10;assert(CompanionBattle::Evaluate(3,d,own,foe).percent==0);
    foe.ability=11;assert(CompanionBattle::Evaluate(4,d,own,foe).percent==0);
    foe.type1=12;foe.ability=18;assert(CompanionBattle::Evaluate(1,d,own,foe).percent==0);
    foe.status=32;assert(CompanionBattle::Evaluate(1,d,own,foe).percent==200);foe.status=0;foe.statusKnown=false;
    assert(!CompanionBattle::Evaluate(1,d,own,foe).known);foe.statusKnown=true;
    foe.type1=0;foe.ability=25;assert(CompanionBattle::Evaluate(6,d,own,foe).percent==0);
    assert(CompanionBattle::Evaluate(5,d,own,foe).percent==200);
    foe.type1=7;foe.ability=0;own.ability=113;assert(CompanionBattle::Evaluate(5,d,own,foe).percent==100);own.ability=0;
    assert(CompanionBattle::Evaluate(5,d,own,foe).percent==0);
    assert(!CompanionBattle::Evaluate(9,d,own,foe).known&&!CompanionBattle::Evaluate(237,d,own,foe).known);
    d.gen5=true;for(int id=1;id<=8;id++)move(d,id,id==2?4:id==3?13:id==4?11:id==5?1:id==6?0:id==7?12:id==8?15:10);
    foe.type1=4;foe.item=2;assert(CompanionBattle::Evaluate(3,d,own,foe).percent==100);
    foe.type1=13;foe.item=3;assert(CompanionBattle::Evaluate(2,d,own,foe).percent==0);
    own.ability=164;assert(CompanionBattle::Evaluate(2,d,own,foe).percent==0);own.ability=0;
    foe.item=0;foe.ability=157;assert(CompanionBattle::Evaluate(7,d,own,foe).percent==0);
    foe.ability=114;assert(CompanionBattle::Evaluate(4,d,own,foe).percent==0);
    foe.ability=0;foe.type1=11;foe.moves[0]=1;
    d.personal.resize(2);d.personal[1].resize(8);d.personal[1][6]=d.personal[1][7]=11; // Gen5 Grass id.
    Snapshot s;s.battlers={own,foe};s.battlers[1].species=2;Mon mon;mon.species=1;mon.hp=10;mon.moves[0]=7;mon.pp[0]=10;s.party.push_back(mon);
    auto lines=CompanionBattle::PartyCoverage(s,d);assert(lines.size()==1&&lines[0].find("OUT SUPER 2X")!=std::string::npos);
    s.party[0].hp=0;assert(CompanionBattle::PartyCoverage(s,d).empty());
    std::cout<<"PASS: ability bypass/immunities, grounding, Ring Target, Air Balloon, contextual/status moves and living-party coverage\n";
}

