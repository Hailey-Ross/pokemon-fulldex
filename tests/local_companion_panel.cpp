#include "companion_panel.h"
#include <cassert>
#include <iostream>
#include <fstream>
int main(int argc,char** argv)
{
    assert(Companion::GuideFor("IPKE").size()==37&&Companion::GuideFor("IPGE").size()==37);
    assert(Companion::GuideFor("CPUE").size()==23&&Companion::GuideFor("IRBO").size()==12);
    assert(Companion::GuideFor("UNKNOWN").empty());
    for(const auto& entry:Companion::GuideFor("IPKE"))assert(std::string(entry.answer).find("$bell")==std::string::npos);
    CompanionPanel panel;Companion::Snapshot s;s.supported=s.ready=s.dexReady=true;s.seen.resize(650);s.caught.resize(650);
    Companion::Metadata names;names.species.resize(650,"TEST SPECIES");names.moves.resize(600,"TEST MOVE");names.items.resize(800,"TEST ITEM");names.descriptions.resize(650,"Synthetic description for layout checks.");
    for(int i=0;i<6;i++){Companion::Mon mon;mon.species=i+1;mon.hp=25;mon.maxHp=50;mon.level=10;s.party.push_back(mon);}
    s.items.push_back({1,5,0});auto profile=Companion::ProfileFor("IPKE",0);
    std::vector<Companion::GuideEntry> guide={{"TEST GUIDE","A short hint.","An answer hidden until requested."}};
    for(int tab=0;tab<6;tab++)
    {
        panel.Touch(tab*106+20,40);const auto& canvas=panel.Draw(s,names,profile,guide);
        assert(canvas.w==640&&canvas.h==480&&canvas.px.size()==640*480);
        panel.Touch(600,455);panel.Draw(s,names,profile,guide);
        panel.Touch(320,455);panel.Draw(s,names,profile,guide);
    }
    for(auto dims:{std::pair<int,int>{1280,720},{2400,1080},{720,1280}})
    {
        auto rect=CompanionPanel::Overlay(dims.first,dims.second);int x=0,y=0;
        assert(CompanionPanel::OverlayTouch(dims.first,dims.second,rect.x,rect.y,x,y)&&x==0&&y==0);
        assert(!CompanionPanel::OverlayTouch(dims.first,dims.second,rect.x-1,rect.y,x,y));
        assert(!CompanionPanel::OverlayTouch(dims.first,dims.second,rect.x+rect.w,rect.y,x,y));
    }
    names.species[2]="TEST EVOLUTION";names.evolutions.resize(3);
    names.evolutions[1]={4,0,16,0,2,0,0,0,0}; // One complete record and an incomplete tail.
    assert(names.EvolutionDetails(1)==std::vector<std::string>{"EVOLVES TO TEST EVOLUTION: LEVEL 16"});
    assert(names.EvolutionDetails(-1).empty()&&names.EvolutionDetails(30).empty());
    panel.Touch(230,40);panel.Touch(20,455);panel.Draw(s,names,profile,guide);
    std::string dexText;for(const auto& line:panel.TextLines())dexText+=line+" ";
    assert(dexText.find("EVOLVES TO TEST EVOLUTION: LEVEL 16")!=std::string::npos);
    // An unavailable progress bitmap must not be indexed by species number.
    s.seen.clear();s.caught.clear();panel.Draw(s,names,profile,guide);
    s.seen.resize(650);s.caught.resize(650);
    {
        CompanionPanel scrolled,fresh;
        auto longNames=names;
        std::string longDescription;for(int i=0;i<40;i++)longDescription+="A long reference paragraph with several words. ";
        longNames.descriptions[1]=longNames.descriptions[2]=longDescription;
        scrolled.Touch(230,40);scrolled.Draw(s,longNames,profile,guide);
        scrolled.Touch(300,425);scrolled.Draw(s,longNames,profile,guide);
        scrolled.Touch(600,455);
        fresh.Touch(230,40);fresh.Touch(600,455);
        assert(scrolled.Draw(s,longNames,profile,guide).px==fresh.Draw(s,longNames,profile,guide).px);
    }
    // Check the actual displayed bag content, rather than only successful drawing.
    s.items={{1,5,0},{2,3,1}};names.items[1]="POTION";names.items[2]="BICYCLE";
    panel.Touch(120,40);panel.Draw(s,names,profile,guide);
    auto contains=[&](const std::string& text){const auto& lines=panel.TextLines();return std::find(lines.begin(),lines.end(),text)!=lines.end();};
    assert(contains("ALL - 2 STACKS / 8 ITEMS"));
    panel.Touch(600,455);panel.Draw(s,names,profile,guide);
    assert(contains("ITEMS - 1 STACK / 5 ITEMS")&&contains("POTION X5")&&!contains("BICYCLE X3"));
    panel.Touch(600,455);panel.Draw(s,names,profile,guide);
    assert(contains("KEY ITEMS - 1 STACK / 3 ITEMS")&&!contains("POTION X5")&&contains("BICYCLE X3"));
    panel.Touch(600,455);panel.Draw(s,names,profile,guide);
    assert(contains("THIS POCKET IS EMPTY"));
    panel.Touch(320,455);panel.Draw(s,names,profile,guide);
    assert(contains("ALL - 2 STACKS / 8 ITEMS"));
    names.itemDescriptions.resize(3);names.itemDescriptions[1]="Synthetic healing item description.";
    panel.Touch(20,126);panel.Draw(s,names,profile,guide);
    assert(contains("Synthetic healing item description.")&&contains("IN BAG: 5"));
    panel.Touch(320,455);panel.Draw(s,names,profile,guide);
    assert(contains("ALL - 2 STACKS / 8 ITEMS"));
    assert(names.ItemDescription(-1)=="NO DESCRIPTION AVAILABLE FOR THIS ITEM.");
    assert(names.ItemDescription(99)=="NO DESCRIPTION AVAILABLE FOR THIS ITEM.");
    // A map remains hidden until requested and can be hidden again.
    names.regionMap.width=2;names.regionMap.height=2;names.regionMap.pixels.assign(4,0xFF123456);
    panel.Touch(340,40);auto hidden=panel.Draw(s,names,profile,guide).px;
    panel.Touch(320,455);auto shown=panel.Draw(s,names,profile,guide).px;
    assert(shown!=hidden&&shown[200*640+320]==0xFF123456);
    panel.Touch(320,455);assert(panel.Draw(s,names,profile,guide).px==hidden);
    // Every mapped pocket is reachable in both directions, including ALL.
    for(const char* code:{"IPKE","CPUE","IRBO"})
    {
        auto p=Companion::ProfileFor(code, std::string(code)=="CPUE"?1:0);
        int pocket=-1;
        for(int i=0;i<(int)p.pockets.size();i++)
        {
            pocket=Companion::StepPocket(p,pocket,1);assert(pocket==i);
            assert(std::string(Companion::PocketName(p,pocket))!="UNKNOWN POCKET");
        }
        assert(Companion::StepPocket(p,pocket,1)==-1);
        assert(Companion::StepPocket(p,-1,-1)==(int)p.pockets.size()-1);
        for(int i=(int)p.pockets.size()-1;i>=0;i--)
        {assert(pocket==i);pocket=Companion::StepPocket(p,pocket,-1);}
        assert(pocket==-1);
    }
    assert(Companion::StepPocket(Companion::Profile{},-1,1)==-1);
    assert(std::string(Companion::PocketName(profile,99))=="UNKNOWN POCKET");
    // Render each pocket, long-list paging, and a transition to a smaller profile.
    for(int i=0;i<40;i++)s.items.push_back({i+2,i+1,i%8});
    auto before=s.items;
    panel.Touch(120,40);
    for(int i=0;i<10;i++)
    {
        panel.Draw(s,names,profile,guide);panel.Touch(600,455);
        panel.Draw(s,names,profile,guide);panel.Touch(300,425);
        panel.Draw(s,names,profile,guide);
    }
    panel.Draw(s,names,Companion::ProfileFor("IRBO",0),guide);
    panel.Touch(320,455);panel.Draw(s,names,profile,guide);
    assert(s.items.size()==before.size());
    for(size_t i=0;i<before.size();i++)assert(s.items[i].id==before[i].id&&s.items[i].quantity==before[i].quantity&&s.items[i].pocket==before[i].pocket);
    s.items.clear();panel.Draw(s,names,profile,guide);
    assert(s.party[0].hp==25);panel.Touch(20,40);const auto& canvas=panel.Draw(s,names,profile,guide);
    if(argc>1){std::ofstream f(argv[1],std::ios::binary);f<<"P6\n640 480\n255\n";for(uint32_t p:canvas.px){f.put((p>>16)&255);f.put((p>>8)&255);f.put(p&255);}}
    std::cout<<"PASS: all companion tabs, paging, overlay hit boundaries and read-only model\n";
}
