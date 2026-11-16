#pragma once
// The Pokedex page's data, read from the player's own ROM at run time: each species' personal
// record (stats, types, catch rate, gender, egg groups, abilities, EV yield, hatch steps, TM and HM
// compatibility), its egg moves, its height and weight, and its battle picture (normal and shiny).
// Formats adapted from PokeDaisy NdsArt / Gen4Dex / Gen5Lz (GPL-3.0).
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
// No game graphics or data are bundled or written to disk.
#include "companion_art.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
namespace Companion
{
struct DexStats
{
    bool ok=false;
    int base[6]={};          // HP, Attack, Defense, Sp. Atk, Sp. Def, Speed (display order)
    int ev[6]={};            // the same order
    int type1=0,type2=0,catchRate=0,gender=0,hatchCycles=0,friendship=0,growth=0;
    int eggGroup1=0,eggGroup2=0,ability1=0,ability2=0,hidden=0,baseExp=0;
};
// Gen 4: 0x2C bytes; Gen 5: 0x4C. Stats are stored HP, Atk, Def, Speed, SpA, SpD.
inline DexStats PersonalStats(const Bytes& p,bool gen5)
{
    DexStats s;
    if(p.size()<(gen5?0x38u:0x2Cu))return s;
    const int order[6]={0,1,2,4,5,3};
    unsigned evs=U16(p.data()+0xA);
    for(int i=0;i<6;i++){s.base[i]=p[order[i]];s.ev[i]=(evs>>(order[i]*2))&3;}
    s.type1=p[6];s.type2=p[7];s.catchRate=p[8];
    if(gen5)
    {
        s.gender=p[0x12];s.hatchCycles=p[0x13];s.friendship=p[0x14];s.growth=p[0x15];
        s.eggGroup1=p[0x16];s.eggGroup2=p[0x17];s.ability1=p[0x18];s.ability2=p[0x19];s.hidden=p[0x1A];
        s.baseExp=U16(p.data()+0x22);
    }
    else
    {
        s.baseExp=p[9];s.gender=p[0x10];s.hatchCycles=p[0x11];s.friendship=p[0x12];s.growth=p[0x13];
        s.eggGroup1=p[0x14];s.eggGroup2=p[0x15];s.ability1=p[0x16];s.ability2=p[0x17];
    }
    s.ok=true;return s;
}
// TM and HM compatibility: one bit per machine in the ARM9's machine-move order (Gen 4: TM01-92
// then HM01-08 at 0x1C; Gen 5: TM01-92, HM01-06, TM93-95 at 0x28).
inline bool CanLearnMachine(const Bytes& p,bool gen5,int index)
{
    size_t at=(gen5?0x28:0x1C)+index/8;
    return index>=0&&at<p.size()&&(p[at]>>(index%8)&1);
}
// "TM06" / "HM01" for the machine at [index] of the machine-move order.
inline std::string MachineLabel(bool gen5,int index)
{
    char b[8];
    if(index<92)snprintf(b,sizeof b,"TM%02d",index+1);
    else if(!gen5)snprintf(b,sizeof b,"HM%02d",index-91);
    else if(index<98)snprintf(b,sizeof b,"HM%02d",index-91);
    else snprintf(b,sizeof b,"TM%02d",index-5);
    return b;
}
// The machine moves: found by HM01-04 (Cut, Fly, Surf, Strength) in the ARM9, the 92 TMs just
// before them (vanilla and most hacks keep this table where it was).
inline std::vector<uint16_t> FindMachineMoves(const Bytes& arm,bool gen5)
{
    const unsigned char hm[8]={15,0,19,0,57,0,70,0};
    auto at=std::search(arm.begin(),arm.end(),std::begin(hm),std::end(hm));
    while(at!=arm.end()&&(at-arm.begin())%2)at=std::search(at+1,arm.end(),std::begin(hm),std::end(hm));
    if(at==arm.end())return {};
    size_t hmAt=at-arm.begin(),count=gen5?101:100;
    if(hmAt<92*2||!Range(arm.size(),hmAt-92*2,count*2))return {};
    std::vector<uint16_t> out;
    for(size_t i=0;i<count;i++)
    {
        unsigned m=U16(arm.data()+hmAt-92*2+i*2);
        if(!m||m>1000)return {};
        out.push_back((uint16_t)m);
    }
    return out;
}
// Gen 4 egg moves: one list, each species' moves after a marker of 20000 + species, 0xFFFF at the end.
inline std::vector<std::vector<uint16_t>> Gen4EggMoves(const Bytes& data,size_t start)
{
    std::vector<std::vector<uint16_t>> out;
    int current=-1;
    for(size_t at=start;Range(data.size(),at,2);at+=2)
    {
        unsigned v=U16(data.data()+at);
        if(v==0xFFFF)break;
        if(v>20000&&v<21000){current=(int)v-20000;if((int)out.size()<=current)out.resize(current+1);continue;}
        if(current<0||v>1000)break;
        out[current].push_back((uint16_t)v);
    }
    return out;
}
// Platinum keeps that list in an overlay: where 20001 starts a short run of moves ending at 20004's
// marker (Bulbasaur's then Charmander's).
inline size_t FindGen4EggList(const Bytes& data)
{
    for(size_t at=0;Range(data.size(),at,2);at+=2)
    {
        if(U16(data.data()+at)!=20001)continue;
        size_t end=at+2;
        while(Range(data.size(),end,2)&&U16(data.data()+end)>0&&U16(data.data()+end)<1000)end+=2;
        if(Range(data.size(),end,2)&&U16(data.data()+end)==20004&&end-at>4)return at;
    }
    return std::string::npos;
}
// Gen 5 egg moves: a NARC member per species, a count and the moves.
inline std::vector<uint16_t> Gen5EggMoves(const Bytes& member)
{
    std::vector<uint16_t> out;
    if(member.size()<2)return out;
    size_t n=U16(member.data());
    for(size_t i=0;i<n&&Range(member.size(),2+i*2,2);i++)out.push_back(U16(member.data()+2+i*2));
    return out;
}
// An LZ10 / LZ11 stream (Gen 5's NARC members), or nothing.
inline Bytes UnpackLz(const Bytes& d)
{
    if(d.size()<4||(d[0]!=0x10&&d[0]!=0x11))return {};
    const bool lz11=d[0]==0x11;
    size_t size=d[1]|d[2]<<8|d[3]<<16,p=4;
    if(!size&&d.size()>=8){size=U32(d.data()+4);p=8;}
    if(!size||size>(4u<<20))return {};
    Bytes out(size);size_t o=0;
    auto at=[&](size_t i)->int{return i<d.size()?d[i]:-1;};
    while(o<size)
    {
        int flags=at(p++);if(flags<0)return {};
        for(int bit=7;bit>=0&&o<size;bit--)
        {
            if(!(flags>>bit&1)){int v=at(p++);if(v<0)return {};out[o++]=(uint8_t)v;continue;}
            size_t n,disp;
            if(!lz11){int a=at(p),b=at(p+1);if(b<0)return {};unsigned v=a<<8|b;p+=2;n=(v>>12)+3;disp=(v&0xFFF)+1;}
            else
            {
                int a=at(p);if(a<0)return {};
                if(a>>4==0){int b=at(p+1),c=at(p+2);if(c<0)return {};n=(((a&15)<<4)|(b>>4))+0x11;disp=(((b&15)<<8)|c)+1;p+=3;}
                else if(a>>4==1){int b=at(p+1),c=at(p+2),e=at(p+3);if(e<0)return {};n=(((a&15)<<12)|(b<<4)|(c>>4))+0x111;disp=(((c&15)<<8)|e)+1;p+=4;}
                else {int b=at(p+1);if(b<0)return {};n=(a>>4)+1;disp=(((a&15)<<8)|b)+1;p+=2;}
            }
            if(disp>o)return {};
            for(size_t k=0;k<n&&o<size;k++,o++)out[o]=out[o-disp];
        }
    }
    return out;
}
// An NCLR's first 16 colours (ARGB, colour 0 left clear by the caller).
inline std::vector<uint32_t> FirstPalette(const Bytes& nclr)
{
    auto b=NitroBlock(nclr,"TTLP");if(b.size()<0x18+32)return {};
    std::vector<uint32_t> out;
    for(int c=0;c<16;c++)
    {
        unsigned v=U16(b.data()+0x18+c*2);
        out.push_back(0xFF000000u|((v&31)*255/31<<16)|(((v>>5)&31)*255/31<<8)|((v>>10)&31)*255/31);
    }
    return out;
}
inline Bytes NcgrTiles(const Bytes& ncgr)
{
    // (Found by its tag: Gen 5's packed pictures don't always give the block's length right.)
    const char tag[4]={'R','A','H','C'};
    auto at=std::search(ncgr.begin(),ncgr.end(),tag,tag+4);
    if(at==ncgr.end()||!Range(ncgr.size(),at-ncgr.begin(),0x20))return {};
    size_t o=at-ncgr.begin(),size=U32(ncgr.data()+o+0x18),off=o+8+U32(ncgr.data()+o+0x1C);
    if(off>=ncgr.size())return {};
    return Bytes(ncgr.begin()+off,ncgr.begin()+std::min(ncgr.size(),off+size));
}
// Gen 4 (Platinum, HeartGold and SoulSilver) pokegra: six members a species (female back, back,
// female front, front, palette, shiny palette); the picture 80 x 80 4bpp (the left of a 160-wide pair of frames), its first 3200 u16s
// scrambled (pokepic.c's UnscanPokepic_PtHGSS: XOR with an LCG seeded by the first value).
// Gen 5 a/0/0/4: twenty a species (front 0, female front 1; palettes 18, shiny 19); LZ-packed tiles
// in four OAM blocks of a 96 x 96 picture.
inline MapImage BattlePicture(const std::vector<Bytes>& pics,bool gen5,int species,bool shiny,bool female=false)
{
    MapImage img;
    auto member=[&](size_t i)->const Bytes*{return i<pics.size()&&!pics[i].empty()?&pics[i]:nullptr;};
    if(!gen5)
    {
        const Bytes* pal=member(species*6+(shiny?5:4));if(!pal)return img;
        auto colours=FirstPalette(*pal);if(colours.size()<16)return img;
        Bytes tiles;
        for(int k:{female?2:3,female?3:2})if(auto* m=member(species*6+k)){tiles=NcgrTiles(*m);if(tiles.size()>=6400)break;tiles.clear();}
        if(tiles.size()<6400)return img;
        uint32_t seed=U16(tiles.data());
        for(int i=0;i<3200;i++)
        {
            unsigned v=U16(tiles.data()+i*2)^(seed&0xFFFF);
            tiles[i*2]=(uint8_t)v;tiles[i*2+1]=(uint8_t)(v>>8);
            seed=seed*1103515245u+24691u;
        }
        img.width=img.height=80;img.pixels.assign(80*80,0);
        for(int y=0;y<80;y++)for(int x=0;x<80;x++)
        {
            int v=tiles[y*80+x/2];v=x&1?v>>4:v&15;
            if(v)img.pixels[y*80+x]=colours[v];
        }
        return img;
    }
    const Bytes* pal=member(species*20+(shiny?19:18));if(!pal)return img;
    auto colours=FirstPalette(*pal);if(colours.size()<16)return img;
    Bytes tiles;
    for(int k:{female?1:0,female?0:1})if(auto* m=member(species*20+k)){tiles=NcgrTiles(UnpackLz(*m));if(tiles.size()>=96*96/2)break;tiles.clear();}
    if(tiles.size()<96*96/2)return img;
    img.width=img.height=96;img.pixels.assign(96*96,0);
    const int blocks[4][4]={{0,0,64,64},{64,0,32,64},{0,64,64,32},{64,64,32,32}};
    int tile=0;
    for(auto& b:blocks)
    {
        int across=b[2]/8,count=b[2]*b[3]/64;
        for(int t=0;t<count;t++)for(int y=0;y<8;y++)for(int x=0;x<8;x++)
        {
            size_t i=(size_t)(tile+t)*32+y*4+x/2;if(i>=tiles.size())continue;
            int v=x&1?tiles[i]>>4:tiles[i]&15;
            if(v)img.pixels[(b[1]+(t/across)*8+y)*96+b[0]+(t%across)*8+x]=colours[v];
        }
        tile+=count;
    }
    return img;
}
// Move tutors' moves: found by their first three (Dive, Mud-Slap, Fury Cutter in Platinum,
// HeartGold and SoulSilver; Bug Bite, Covet, Super Fang in Black 2 and White 2), [count] entries
// [stride] bytes apart, the move first. Hacks that changed those first moves are left without.
inline std::vector<uint16_t> FindTutorMoves(const Bytes& data,const int first[3],int stride,int count,size_t& at)
{
    for(at=0;Range(data.size(),at,(size_t)stride*count);at+=2)
    {
        bool ok=true;for(int k=0;k<3&&ok;k++)ok=U16(data.data()+at+k*stride)==first[k];
        if(!ok)continue;
        std::vector<uint16_t> out;
        for(int i=0;i<count;i++){unsigned m=U16(data.data()+at+i*stride);if(!m||m>1000)return {};out.push_back((uint16_t)m);}
        return out;
    }
    at=std::string::npos;return {};
}
}
