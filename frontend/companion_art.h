#pragma once
// Runtime map graphics decoding adapted from PokeDaisy NdsArt/PokegearMap (GPL-3.0).
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
// No game graphics are bundled or written to disk.
#include "companion_data.h"
namespace Companion
{
struct MapImage
{
    int width=0,height=0;
    std::vector<uint32_t> pixels;
    bool Ready()const{return width>0&&height>0&&pixels.size()==(size_t)width*height;}
};
inline Bytes NitroBlock(const Bytes& file,const char* tag)
{
    if(file.size()<16)return {};
    size_t at=U16(file.data()+12);
    if(at<16)return {};
    for(int i=0;i<32&&Range(file.size(),at,8);i++)
    {
        size_t length=U32(file.data()+at+4);
        if(length<8||!Range(file.size(),at,length))return {};
        if(!memcmp(file.data()+at,tag,4))return Bytes(file.begin()+at,file.begin()+at+length);
        at+=length;
    }
    return {};
}
inline Bytes MapTiles(const Bytes& file)
{
    auto b=NitroBlock(file,"RAHC");if(b.size()<32)return {};
    size_t size=U32(b.data()+24),offset=U32(b.data()+28);
    if(offset>b.size()-8||!Range(b.size(),offset+8,size))return {};
    return Bytes(b.begin()+offset+8,b.begin()+offset+8+size);
}
inline std::vector<uint32_t> MapColours(const Bytes& file)
{
    auto b=NitroBlock(file,"TTLP");if(b.size()<24)return {};
    size_t size=U32(b.data()+16),offset=U32(b.data()+20);
    if(size>32768||size%2||offset>b.size()-8||!Range(b.size(),offset+8,size))return {};
    std::vector<uint32_t> out;
    for(size_t at=offset+8;at<offset+8+size;at+=2)
    {
        unsigned v=U16(b.data()+at);
        out.push_back(0xFF000000u|((v&31)*255/31<<16)|(((v>>5)&31)*255/31<<8)|((v>>10)&31)*255/31);
    }
    return out;
}
inline bool MapLayer(MapImage& image,const Bytes& screen,const Bytes& tiles,const std::vector<uint32_t>& colours,bool eightBit,bool extendedPalette=true)
{
    auto b=NitroBlock(screen,"NRCS");if(b.size()<20||!image.Ready()||tiles.empty()||colours.empty())return false;
    int width=U16(b.data()+8),height=U16(b.data()+10);
    if(width<=0||height<=0||width%8||height%8||width>1024||height>1024)return false;
    size_t count=(size_t)(width/8)*(height/8);
    if(!Range(b.size(),20,count*2))return false;
    int stride=eightBit?64:32;
    for(int y=0;y<std::min(height,image.height);y++)for(int x=0;x<std::min(width,image.width);x++)
    {
        unsigned entry=U16(b.data()+20+((y/8)*(width/8)+x/8)*2);
        int sx=entry&0x400?7-x%8:x%8,sy=entry&0x800?7-y%8:y%8;
        size_t offset=(entry&1023)*stride+sy*(eightBit?8:4)+(eightBit?sx:sx/2);
        if(offset>=tiles.size())return false;
        unsigned value=tiles[offset];if(!eightBit)value=(value>>(sx%2*4))&15;
        if(!value)continue;
        size_t colour=(extendedPalette?(entry>>12)*(eightBit?256:16):0)+value;
        if(colour>=colours.size())return false;
        image.pixels[y*image.width+x]=colours[colour];
    }
    return true;
}
inline MapImage ReferenceMap(const std::vector<Bytes>& archive,bool gen5,bool hgss)
{
    MapImage image;
    size_t required=gen5?18:hgss?21:25;if(archive.size()<required)return image;
    auto colours=MapColours(archive[hgss?20:0]);if(colours.empty())return image;
    image.width=hgss?376:256;image.height=hgss?160:168;
    image.pixels.assign(image.width*image.height,colours[0]);
    bool ok=false;
    if(hgss)ok=MapLayer(image,archive[11],MapTiles(archive[10]),colours,true,false);
    else if(gen5)ok=MapLayer(image,archive[16],MapTiles(archive[7]),colours,true)&&MapLayer(image,archive[17],MapTiles(archive[5]),colours,true);
    else ok=MapLayer(image,archive[22],MapTiles(archive[19]),colours,false)&&MapLayer(image,archive[24],MapTiles(archive[19]),colours,false);
    if(!ok)return {};
    if(!hgss&&!gen5)
    {
        MapImage crop;crop.width=216;crop.height=168;crop.pixels.resize(crop.width*crop.height);
        for(int y=0;y<crop.height;y++)std::copy_n(image.pixels.begin()+y*image.width+20,crop.width,crop.pixels.begin()+y*crop.width);
        return crop;
    }
    return image;
}
}
