#include "companion_rom.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace Companion;
void put16(Bytes& b,size_t p,uint16_t v){b[p]=v;b[p+1]=v>>8;}
void put32(Bytes& b,size_t p,uint32_t v){put16(b,p,v);put16(b,p+2,v>>16);}
Bytes mon(int permutation,bool encrypted,bool gen5)
{
    Bytes b(gen5?220:236);uint32_t pid=permutation<<13;put32(b,0,pid);put16(b,4,encrypted?0:3);
    std::string order="ABCD";for(int i=0;i<permutation;i++)std::next_permutation(order.begin(),order.end());
    size_t a=8+order.find('A')*32, m=8+order.find('B')*32,c=8+order.find('C')*32;
    put16(b,a,25);put16(b,a+2,4);put32(b,a+8,1000);for(int i=0;i<6;i++)b[a+16+i]=i*10;
    put16(b,m,33);b[m+8]=35;put32(b,m+16,31u|(12u<<5)|(8u<<10));
    for(int i=0;i<11;i++)put16(b,c+i*2,65535);
    put32(b,136,16);b[140]=12;put16(b,142,30);put16(b,144,40);
    for(int i=0;i<5;i++)put16(b,146+i*2,20+i);
    uint16_t sum=0;for(size_t i=8;i<136;i+=2)sum+=U16(b.data()+i);put16(b,6,sum);
    if(encrypted){auto box=Crypt(b.data()+8,128,sum);std::copy(box.begin(),box.end(),b.begin()+8);auto p=Crypt(b.data()+136,b.size()-136,pid);std::copy(p.begin(),p.end(),b.begin()+136);}
    return b;
}
void CheckLearning()
{
    Companion::Metadata data;data.learnsets.resize(2);
    auto add=[&](int move,int level){int value=(level<<9)|move;data.learnsets[1].push_back(value&255);data.learnsets[1].push_back(value>>8);};
    add(5,20);add(2,5);add(3,10);add(3,15);add(4,12);add(0,30);add(6,101);add(511,127);
    Companion::Mon mon;mon.species=1;mon.level=5;mon.moves[0]=4;
    auto next=data.Upcoming(mon);assert(next.size()==2&&next[0].move==3&&next[0].level==10&&next[1].move==5);
    assert(data.Upcoming(mon,1).size()==1);mon.egg=true;assert(data.Upcoming(mon).empty());
}
int main(int argc,char** argv)
{
    CheckLearning();
    for(bool gen5:{false,true})for(bool encrypted:{false,true})for(int p=0;p<24;p++)
    {auto b=mon(p,encrypted,gen5);Mon m;assert(DecodeMon(b.data(),b.size(),gen5,m));assert(m.species==25&&m.level==12&&m.hp==30&&m.maxHp==40&&m.stats[1]==20&&m.stats[5]==24&&m.moves[0]==33&&m.iv[0]==31&&m.iv[1]==12&&m.ev[5]==50);b[6]^=1;assert(!DecodeMon(b.data(),b.size(),gen5,m));}
    Bytes ram(4*1024*1024);auto profile=ProfileFor("IPGE",0);
    auto reader=[&](uint32_t at,void* out,size_t n){if(!MainRange(at,n))return false;memcpy(out,ram.data()+at-0x02000000,n);return true;};
    assert(!Read(profile,reader).ready);
    put32(ram,profile.pointer-0x02000000,0x02010000);
    size_t base=0x10000+profile.body;put32(ram,base+profile.party,6);put32(ram,base+profile.party+4,1);
    auto m=mon(17,true,false);std::copy(m.begin(),m.end(),ram.begin()+base+profile.party+8);
    put16(ram,base+profile.bag,4);put16(ram,base+profile.bag+2,2);
    put32(ram,base+profile.dex,0xBEEFCAFE);ram[base+profile.dex+4+3]=1;ram[base+profile.dex+4+64+3]=1;
    auto snap=Read(profile,reader);assert(snap.ready&&snap.party.size()==1&&snap.items.size()==1&&snap.items[0].quantity==2&&snap.dexReady&&snap.caught[25]);
    put32(ram,base+profile.party+4,7);assert(!Read(profile,reader).ready);
    assert(!ProfileFor("IPGE",10).supported&&!ProfileFor("IPGF",0).supported&&!ProfileFor("IREO",0).supported);
    assert(Narc(Bytes(20)).empty()&&Text(Bytes(15),false).empty()&&RomFile(Bytes(20),"a/b").empty());
    // Independently encode a two-character Gen 4 text bank.
    Bytes text(18);put16(text,0,1);put32(text,4,12);put32(text,8,3);uint16_t seed=596947u&65535;
    for(auto pair:{std::pair<size_t,int>{12,0x12B},{14,0x12C},{16,65535}}){put16(text,pair.first,pair.second^seed);seed+=18749;}
    auto lines=Text(text,false);assert(lines.size()==1&&lines[0]=="AB");
    std::cout<<"PASS: 96 encrypted/plain party variants, checksums, stats, snapshot bounds, bag, dex and text decoding\n";
    if(argc>1)
    {
        std::ifstream f(argv[1],std::ios::binary);Bytes rom((std::istreambuf_iterator<char>(f)),{});assert(rom.size()>512);
        std::string code((const char*)rom.data()+12,4);auto p=ProfileFor(code,rom[30]);
        if(!p.supported){std::cout<<"ROM profile unsupported; no metadata assumed\n";return 0;}
        Metadata data;data.Load(rom,code,p);
        assert(data.species.size()>490&&data.moves.size()>460&&data.items.size()>400&&data.personal.size()>490&&data.learnsets.size()>490);
        assert(data.itemDescriptions.size()>400&&data.ItemDescription(17).size()>15);
        assert(data.regionMap.Ready());
        assert(data.mapRecords.size()>500&&data.mapPlaces.size()==data.mapRecords.size());
        int encounterMaps=0;for(size_t map=0;map<data.mapRecords.size();map++)if(!data.Encounters(map).empty())encounterMaps++;
        assert(encounterMaps>100);
        int verifiedTeams=0;for(size_t id=1;id<data.trainerData.size();id++)if(!data.TrainerTeam(id).empty())verifiedTeams++;
        assert(verifiedTeams>100&&!data.ReferenceEntries(4,"").empty());
        assert(data.Type(25,0)==13); // Pikachu's Electric type, without printing ROM content.
        assert(!data.LearnedMoves(25).empty());
        std::cout<<"PASS: player's ROM metadata parsed in memory; no extracted text or data written\n";
        if(argc>2)
        {
            std::ifstream saveFile(argv[2],std::ios::binary);Bytes save((std::istreambuf_iterator<char>(saveFile)),{});
            assert(save.size()>=0x20000);
            Bytes memory(4*1024*1024);
            put32(memory,p.pointer-0x02000000,0x02010000);
            std::copy_n(save.begin(),0x20000,memory.begin()+0x10000+p.body);
            auto snapshot=Read(p,[&](uint32_t at,void* out,size_t n){if(!MainRange(at,n))return false;memcpy(out,memory.data()+at-0x02000000,n);return true;});
            assert(snapshot.ready&&!snapshot.party.empty()&&snapshot.invalidSlots==0);
            std::cout<<"PASS: isolated read-only existing save decoded through the runtime snapshot reader\n";
        }

    }
}
