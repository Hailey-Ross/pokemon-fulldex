#include "companion_rom.h"
#include <cassert>
#include <iostream>
using namespace Companion;
static void put16(Bytes& b,size_t at,unsigned v){b[at]=v;b[at+1]=v>>8;}
static void put32(Bytes& b,size_t at,unsigned v){put16(b,at,v);put16(b,at+2,v>>16);}
static Bytes nitro(const char* tag,int length){Bytes b(16+length);put16(b,12,16);memcpy(b.data()+16,tag,4);put32(b,20,length);return b;}
int main()
{
    assert(AsciiChar(0x188,false)=="e"&&AsciiChar(0x00E9,true)=="e"&&AsciiChar(0x00DF,true)=="ss");
    Bytes packed={0,0xC0,'C','B','A',0x10,14,0,0,8,4,0,0,0};
    auto unpacked=UnpackBlz(packed);assert(std::string(unpacked.begin(),unpacked.end())=="CBACBACBACBACBACBA");
    auto badPacked=packed;badPacked[0]=255;assert(UnpackBlz(badPacked).empty());
    badPacked=packed;badPacked[9]=0xFF;assert(UnpackBlz(badPacked).empty());
    for(bool gen5:{false,true})
    {
        Bytes rom(512+64+packed.size());put32(rom,0x20,512);put32(rom,0x28,0x02000000);put32(rom,0x2C,64+packed.size());
        const unsigned char magic4[]={0x21,0x06,0xC0,0xDE,0xDE,0xC0,0x06,0x21};
        const unsigned char magic5[]={0xDE,0xC0,0x06,0x21,0x21,0x06,0xC0,0xDE};
        std::copy_n(gen5?magic5:magic4,8,rom.begin()+512+32);put32(rom,512+24,0x02000000+64+packed.size());
        std::copy(packed.begin(),packed.end(),rom.begin()+512+64);
        auto arm=RomArm9(rom);assert(arm.size()==82&&std::string(arm.end()-18,arm.end())=="CBACBACBACBACBACBA");
        put32(rom,512+24,0x03000000);assert(RomArm9(rom).empty());
    }
    Metadata markerData;markerData.regionMap.width=376;markerData.regionMap.height=160;markerData.regionMap.pixels.resize(376*160);
    markerData.hgss=true;markerData.mapRecords.resize(2);markerData.mapRecords[0].matrix=0;
    Snapshot position;position.ready=true;position.map=0;position.x=32*11;position.y=32*11;
    auto marker=markerData.PlayerMarker(position);assert(marker.ready&&marker.x==92&&marker.y==108&&!marker.approximate);
    position.x=-1;assert(!markerData.PlayerMarker(position).ready);
    position.map=1;markerData.mapRecords[1].worldX=10;markerData.mapRecords[1].worldY=8;
    marker=markerData.PlayerMarker(position);assert(marker.ready&&marker.approximate&&marker.x==84&&marker.y==84);
    position.map=500;assert(!markerData.PlayerMarker(position).ready);
    auto pal=nitro("TTLP",56);put32(pal,32,32);put32(pal,36,16);put16(pal,42,31);
    auto colours=MapColours(pal);assert(colours.size()==16&&colours[1]==0xFFFF0000);
    auto tiles=nitro("RAHC",64);put32(tiles,40,32);put32(tiles,44,24);tiles[48]=1;
    auto pixels=MapTiles(tiles);assert(pixels.size()==32&&pixels[0]==1);
    auto screen=nitro("NRCS",22);put16(screen,24,8);put16(screen,26,8);
    MapImage image;image.width=image.height=8;image.pixels.assign(64,0xFF000000);
    assert(MapLayer(image,screen,pixels,colours,false));assert(image.pixels[0]==0xFFFF0000&&image.pixels[1]==0xFF000000);
    put16(screen,36,0x400);image.pixels.assign(64,0);
    assert(MapLayer(image,screen,pixels,colours,false)&&image.pixels[7]==0xFFFF0000);
    put16(screen,36,0x800);image.pixels.assign(64,0);
    assert(MapLayer(image,screen,pixels,colours,false)&&image.pixels[56]==0xFFFF0000);
    put16(screen,36,0xC00);image.pixels.assign(64,0);
    assert(MapLayer(image,screen,pixels,colours,false)&&image.pixels[63]==0xFFFF0000);
    Bytes eight(64);eight[1]=1;put16(screen,36,0);image.pixels.assign(64,0);
    assert(MapLayer(image,screen,eight,colours,true)&&image.pixels[1]==0xFFFF0000);
    put16(screen,36,0x1000);assert(!MapLayer(image,screen,eight,colours,true));
    put16(screen,36,0x3FF);assert(!MapLayer(image,screen,eight,colours,true));
    put16(screen,24,7);assert(!MapLayer(image,screen,eight,colours,true));
    for(size_t n=0;n<tiles.size();n++){auto truncated=tiles;truncated.resize(n);assert(MapTiles(truncated).empty());}
    put32(tiles,44,0xFFFFFFFF);assert(MapTiles(tiles).empty());
    put32(pal,36,0xFFFFFFFF);assert(MapColours(pal).empty());
    assert(!ReferenceMap({},false,false).Ready());
    Metadata data;data.personal.resize(2);data.personal[1].resize(28);data.abilities={"NONE","ONE","TWO","HIDDEN"};
    data.personal[1][22]=1;data.personal[1][23]=1;data.personal[1][24]=2;data.personal[1][25]=2;data.personal[1][26]=3;
    assert(data.AbilityDetails(1)==std::vector<std::string>{"ABILITY: ONE"});
    data.gen5=true;assert((data.AbilityDetails(1)==std::vector<std::string>{"ABILITY: TWO","HIDDEN ABILITY: HIDDEN"}));
    data.personal[1].resize(25);assert(data.AbilityDetails(1).empty());
    data.species={"NONE","Alpha Test","Beta Name"};data.items={"NONE","First Item"};data.places={"Test Town","Test Cave"};
    assert(data.ReferenceEntries(0,"test ALPHA").size()==1);
    assert(data.ReferenceEntries(0,"no match").empty());
    assert(data.ReferenceEntries(1,"").size()==1&&data.ReferenceEntries(3,"test").size()==2);
    assert(data.ReferenceEntries(-1,"").empty()&&data.ReferenceEntries(4,"").empty());
    assert(data.ReferenceMatches("Mixed CASE Name"," name MIXED "));
    data.moveRecords.resize(2);data.moveRecords[1]={13,0,1,80,95,15};
    auto move=data.MoveData(1);assert(move.ready&&move.type==14&&move.category==1&&move.power==80&&move.accuracy==95&&move.pp==15);
    data.gen5=false;data.moveRecords[1]={0,0,0,60,13,100,20};move=data.MoveData(1);
    assert(move.ready&&move.type==13&&move.pp==20&&move.accuracy==100);
    data.moveRecords[1].resize(6);assert(!data.MoveData(1).ready&&!data.MoveData(-1).ready);
    data.trainerData.resize(2);data.trainerTeams.resize(2);
    for(bool gen5:{false,true})for(int flags=0;flags<4;flags++)
    {
        data.gen5=gen5;data.trainerData[1]={static_cast<unsigned char>(flags),0,0,1};
        auto& team=data.trainerTeams[1];team.assign(8+((flags&1)?8:0)+((flags&2)?2:0),0);
        put16(team,2,50);put16(team,4,25);if(gen5)put16(team,6,1);
        int at=gen5?8:6;if(flags&2){put16(team,at,17);at+=2;}
        if(flags&1)for(int n=0;n<4;n++)put16(team,at+n*2,n+1);
        auto mons=data.TrainerTeam(1);assert(mons.size()==1&&mons[0].species==25&&mons[0].level==50);
        assert(mons[0].customMoves==bool(flags&1)&&mons[0].item==((flags&2)?17:0));
        if(flags&1)assert(mons[0].moves[3]==4);
        if(gen5)assert(mons[0].form==1);
        team.pop_back();assert(data.TrainerTeam(1).empty());
    }
    assert(data.TrainerTeam(-1).empty()&&data.TrainerTeam(100).empty());
    Metadata encounters;encounters.hgss=true;encounters.mapRecords.resize(1);encounters.mapRecords[0].encounter=0;encounters.wildRecords.resize(1);
    auto& wild=encounters.wildRecords[0];wild.assign(196,0);wild[0]=1;wild[8]=5;
    put16(wild,20,25);put16(wild,44,26);put16(wild,68,27);
    auto list=encounters.Encounters(0);assert(list.size()==3&&list[0].species==25&&list[0].low==5&&list[2].method=="NIGHT GRASS");
    wild[1]=1;wild[100]=10;wild[101]=20;put16(wild,102,28);
    list=encounters.Encounters(0);assert(list.size()==4&&list.back().low==10&&list.back().high==20);
    wild.resize(195);assert(encounters.Encounters(0).empty());
    encounters.hgss=false;wild.assign(424,0);put32(wild,0,10);put32(wild,4,7);put32(wild,8,25);
    list=encounters.Encounters(0);assert(list.size()==1&&list[0].species==25&&list[0].low==7);
    encounters.gen5=true;wild.assign(928,0);wild[0]=20;put16(wild,8,504);wild[10]=2;wild[11]=4;
    wild[232]=20;put16(wild,240,506);wild[242]=3;wild[243]=5;
    list=encounters.Encounters(0);assert(list.size()==2&&list[0].method=="SPRING GRASS"&&list[1].method=="SUMMER GRASS");
    assert(encounters.Encounters(-1).empty()&&encounters.Encounters(99).empty());
    // Platinum's scene template is the child's first word, not its update callback.
    auto p=ProfileFor("CPUE",1);Bytes ram(4*1024*1024);
    auto pointer=[&](uint32_t at,uint32_t value){put32(ram,at-0x02000000,value);};
    uint32_t base=0x02010000+p.body;
    pointer(p.pointer,0x02010000);pointer(base+p.party,6);pointer(base+p.party+4,0);
    pointer(0x021C07DC,0x02040000);pointer(0x02040000,0x02041000);pointer(0x02041004,0x02042000);
    pointer(0x020EA358,0x02077777);pointer(0x02042000,0x02077777);pointer(0x02042004,0x02088888);
    pointer(0x0204201C,0x02043000);pointer(0x02043030,0x02050000);
    for(int i=0;i<2;i++)
    {
        size_t at=0x50000+0x2D40+i*0xC0;put16(ram,at,25+i);ram[at+0x34]=10;put32(ram,at+0x4C,20);put32(ram,at+0x50,30);
    }
    auto read=[&](uint32_t at,void* out,size_t count){if(!MainRange(at,count))return false;memcpy(out,ram.data()+at-0x02000000,count);return true;};
    auto snap=Read(p,read);assert(snap.ready&&snap.battlers.size()==4&&snap.battlers[0].species==25&&snap.battlers[1].species==26);
    pointer(0x02042000,0x02099999);assert(Read(p,read).battlers.empty());
    std::cout<<"PASS: Nitro map decoding, transparency, flips, bounds and missing art; generation-specific abilities; Platinum scene guard\n";
}
