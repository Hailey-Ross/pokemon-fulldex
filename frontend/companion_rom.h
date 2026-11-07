#pragma once
// Read a supported cart's own names and dex data; none is bundled or written out.
// Text decoding adapted from PokeDaisy Gen4Text/Gen5Text/NdsArt (GPL-3.0).
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "companion_icons.h"
#include "companion_data.h"
#include "companion_art.h"
#include <map>
#include <sstream>
#include <cctype>
#include <set>
namespace Companion
{
inline Bytes RomFile(const Bytes& rom,std::string path)
{
    if(rom.size()<0x200)return {};
    if(!path.empty()&&path[0]=='/')path.erase(0,1);
    uint32_t fnt=U32(rom.data()+0x40),fntSize=U32(rom.data()+0x44),fat=U32(rom.data()+0x48),fatSize=U32(rom.data()+0x4C);
    if(fntSize>0x400000 || !Range(rom.size(),fnt,fntSize)||!Range(rom.size(),fat,fatSize))return {};
    uint16_t dir=0xF000;
    for(int depth=0;depth<32;depth++)
    {
        size_t entry=(dir&0xFFF)*8,slash=path.find('/');
        if(!Range(fntSize,entry,8))return {};
        size_t pos=U32(rom.data()+fnt+entry);uint32_t id=U16(rom.data()+fnt+entry+4);
        bool descended=false;
        while(pos<fntSize)
        {
            int tag=rom[fnt+pos++];if(!tag)break;
            size_t n=tag&127;if(!Range(fntSize,pos,n))return {};
            std::string name((const char*)rom.data()+fnt+pos,n);pos+=n;
            if(tag&128){if(!Range(fntSize,pos,2))return {};uint16_t sub=U16(rom.data()+fnt+pos);pos+=2;if(slash!=std::string::npos && name==path.substr(0,slash)){dir=sub;path=path.substr(slash+1);descended=true;break;}}
            else {if(slash==std::string::npos && name==path){if(!Range(fatSize,(size_t)id*8,8))return {};uint32_t begin=U32(rom.data()+fat+id*8),end=U32(rom.data()+fat+id*8+4);if(end<begin||end-begin>32*1024*1024||!Range(rom.size(),begin,end-begin))return {};return Bytes(rom.begin()+begin,rom.begin()+end);}id++;}
        }
        if(!descended)return {};
    }
    return {};
}
inline std::vector<Bytes> Narc(const Bytes& data)
{
    std::vector<Bytes> result;
    if(data.size()<16||memcmp(data.data(),"NARC",4))return result;
    size_t pos=U16(data.data()+12),fat=0,img=0,imgSize=0;uint32_t count=0;
    for(int i=0;i<16 && Range(data.size(),pos,8);i++)
    {
        uint32_t len=U32(data.data()+pos+4);if(len<8||!Range(data.size(),pos,len))return {};
        if(!memcmp(data.data()+pos,"BTAF",4)){if(len<12)return {};count=U16(data.data()+pos+8);fat=pos+12;if(count>8192||!Range(len,12,(size_t)count*8))return {};}
        if(!memcmp(data.data()+pos,"GMIF",4)){img=pos+8;imgSize=len-8;}
        pos+=len;
    }
    if(!fat||!img)return {};
    for(uint32_t i=0;i<count;i++){uint32_t a=U32(data.data()+fat+i*8),b=U32(data.data()+fat+i*8+4);if(b<a||!Range(imgSize,a,b-a))return {};result.emplace_back(data.begin()+img+a,data.begin()+img+b);}
    return result;
}
inline std::vector<std::string> Text(const Bytes& file,bool gen5)
{
    std::vector<std::string> result;
    if(file.size()<16)return result;
    const int count=U16(file.data()+(gen5?2:0));if(count>8192)return result;
    size_t section=gen5?U32(file.data()+12):0;
    if(!Range(file.size(),gen5?section+4:4,(size_t)count*8))return result;
    for(int i=0;i<count;i++)
    {
        size_t e=(gen5?section+4:4)+i*8,offset=0,length=0;
        if(gen5){offset=section+U32(file.data()+e);length=U16(file.data()+e+4);}
        else{uint32_t key=(765u*(i+1)*U16(file.data()+2))&65535;key|=key<<16;offset=U32(file.data()+e)^key;length=U32(file.data()+e+4)^key;}
        if(length>16384||!Range(file.size(),offset,length*2)){result.emplace_back();continue;}
        std::vector<uint16_t> codes(length);
        if(gen5){uint16_t key=length?U16(file.data()+offset+(length-1)*2)^0xFFFF:0;for(int j=(int)length-1;j>=0;j--){codes[j]=U16(file.data()+offset+j*2)^key;key=(key>>3)|(key<<13);}}
        else{uint16_t key=(i+1)*596947u;for(size_t j=0;j<length;j++){codes[j]=U16(file.data()+offset+j*2)^key;key+=18749;}}
        std::string line;
        for(size_t j=0;j<codes.size();j++)
        {
            int c=codes[j];if(c==0xFFFF)break;
            if((gen5&&c==0xF000)||(!gen5&&c==0xFFFE)){if(!Range(codes.size(),j,3))break;j+=2+codes[j+2];continue;}
            if(gen5&&c==0xFFFE){line+=' ';continue;}
            if(gen5&&c==0xF100){uint32_t bits=0;int have=0;bool end=false;for(size_t k=j+1;k<codes.size()&&!end;k++){bits|=(uint32_t)codes[k]<<have;have+=16;while(have>=9){int ch=bits&511;bits>>=9;have-=9;if(ch==511){end=true;break;}line+=AsciiChar(ch,true);}}break;}
            line+=AsciiChar(c,gen5);
        }
        while(!line.empty()&&line.back()==' ')line.pop_back();
        result.push_back(line);
    }
    return result;
}
// Decode the ARM9's BLZ segment in memory. Format adapted from PokeDaisy gen_gen5_tables.py.
inline Bytes UnpackBlz(const Bytes& input)
{
    if(input.size()<8)return {};
    size_t n=input.size(),word=U32(input.data()+n-8),extra=U32(input.data()+n-4);
    if(!extra)return input;
    size_t header=word>>24,packed=word&0xFFFFFF;
    if(header<8||packed<header||packed>n||extra>0x400000||n>0x400000-extra)return {};
    Bytes out=input;out.resize(n+extra);size_t start=n-packed,ip=n-header,op=out.size();
    while(ip>start&&op>start)
    {
        unsigned flags=out[--ip];
        for(int bit=0;bit<8&&ip>start&&op>start;bit++,flags<<=1)
        {
            if(flags&128)
            {
                if(ip-start<2)return {};
                ip-=2;
                size_t distance=(((out[ip+1]&15)<<8)|out[ip])+3,length=(out[ip+1]>>4)+3;
                if(length>op-start)return {};
                for(size_t i=0;i<length;i++){--op;if(distance>=out.size()-op)return {};out[op]=out[op+distance];}
            }
            else {if(op<=start)return {};out[--op]=out[--ip];}
        }
    }
    return op==start?out:Bytes{};
}
inline Bytes RomArm9(const Bytes& rom)
{
    if(rom.size()<0x200)return {};
    size_t offset=U32(rom.data()+0x20),size=U32(rom.data()+0x2C);uint32_t ram=U32(rom.data()+0x28);
    if(size>0x400000||!Range(rom.size(),offset,size))return {};
    Bytes arm(rom.begin()+offset,rom.begin()+offset+size);
    // Nitro's module-parameter marker has opposite word order in these Gen4 and Gen5 binaries.
    const unsigned char magic4[]={0x21,0x06,0xC0,0xDE,0xDE,0xC0,0x06,0x21};
    const unsigned char magic5[]={0xDE,0xC0,0x06,0x21,0x21,0x06,0xC0,0xDE};
    auto at=std::search(arm.begin(),arm.end(),std::begin(magic4),std::end(magic4));
    if(at==arm.end())at=std::search(arm.begin(),arm.end(),std::begin(magic5),std::end(magic5));
    if(at==arm.end())return arm;
    size_t marker=at-arm.begin();if(marker<28)return {};
    uint32_t end=U32(arm.data()+marker-8);if(!end)return arm;
    if(end<ram||end-ram>arm.size())return {};
    size_t length=end-ram;auto unpacked=UnpackBlz(Bytes(arm.begin(),arm.begin()+length));
    if(unpacked.empty()||unpacked.size()+arm.size()-length>0x400000)return {};
    unpacked.insert(unpacked.end(),arm.begin()+length,arm.end());return unpacked;
}
struct Metadata
{
    std::vector<std::string> species,moves,items,descriptions,abilities,itemDescriptions;
    std::vector<Bytes> personal,evolutions,learnsets,moveRecords;
    bool gen5=false;
    MapImage regionMap;
    // (Party icons: the ROM's icon archive and its palettes, HGSS and Platinum; companion_icons.h.)
    std::vector<Bytes> monIcons;
    std::vector<std::vector<uint32_t>> iconPalettes;
    // (Gen 5: the icons are every other member from the first NCGR (7 in Black and White, 8 in Black 2
    // and White 2), one for each species; their palettes chosen by a byte for each
    // species in the ARM9 (low nibble; high: its female icon's), found by Gen 4's table, which it repeats.)
    std::vector<uint8_t> gen5IconPalettes;
    int gen5IconStart=-1;
    void FindGen5IconPalettes(const Bytes& arm)
    {
        const size_t n=CompanionIcons::Gen4Egg;
        for(size_t o=0;o+n+700<arm.size();o++)
        {
            bool ok=true;
            for(size_t sp=1;sp<n&&ok;sp++){const uint8_t b=arm[o+sp];ok=(b&15)==CompanionIcons::Gen4Palette[sp]&&(b>>4)==(b&15);}
            if(!ok)continue;
            // (After Gen 4's: a Pokemon with a female icon has its palette in the high nibble.)
            size_t end=o+1;while(end<arm.size()&&(arm[end]&15)<=2&&(arm[end]>>4)<=2)end++;
            for(size_t q=o;q<end;q++)gen5IconPalettes.push_back(arm[q]&15);
            return;
        }
    }
    bool MonIcon(int species,bool egg,std::vector<uint32_t>& out)const
    {
        if(gen5)
        {
            const int member=gen5IconStart+2*species;
            if(egg||iconPalettes.empty()||gen5IconStart<0||species<1||species>=(int)gen5IconPalettes.size()||member>=(int)monIcons.size())return false;
            const int pal=gen5IconPalettes[species];
            return pal<(int)iconPalettes.size()&&CompanionIcons::Icon(monIcons[member],iconPalettes[pal],out);
        }
        if(iconPalettes.empty())return false;
        const int id=egg?CompanionIcons::Gen4Egg:species;
        if(id<1||id>CompanionIcons::Gen4Egg||7+id>=(int)monIcons.size())return false;
        const int pal=CompanionIcons::Gen4Palette[id];
        if(pal>=(int)iconPalettes.size())return false;
        return CompanionIcons::Icon(monIcons[7+id],iconPalettes[pal],out);
    }
    // (Item icons, HGSS and Platinum: the ROM's item icon archive, and the ARM9's table of each
    // item's {data, icon, palette, Gen 3 id}, found by its shape (item i's data file is i).)
    std::vector<Bytes> itemIcons;
    std::vector<std::array<int,2>> itemIconIds;
    bool ItemIcon(int item,std::vector<uint32_t>& out)const
    {
        if(item<=0||item>=(int)itemIconIds.size())return false;
        const auto ids=itemIconIds[item];
        if(ids[0]>=(int)itemIcons.size()||ids[1]>=(int)itemIcons.size())return false;
        const auto pals=CompanionIcons::Palettes(itemIcons[ids[1]]);
        return !pals.empty()&&CompanionIcons::Icon(itemIcons[ids[0]],pals[0],out);
    }
    void LoadItemIcons(const Bytes& rom,const Bytes& arm,bool hg)
    {
        itemIcons=Narc(RomFile(rom,gen5?"a/0/2/5":hg?"a/0/1/8":"itemtool/itemdata/item_icon.narc"));
        const int n=(int)itemIcons.size();
        if(n<2||arm.size()<8*64)return;
        auto u16=[&](size_t o){return (int)U16(arm.data()+o);};
        if(gen5)
        {
            // (Gen 5: {icon, palette} for each item, four bytes; found by item 1 (the Master Ball)
            // being the archive's first icon and palette, and the next ones being an icon and a
            // palette each.)
            auto is=[&](int m,const char* magic){return m>=0&&m<n&&itemIcons[m].size()>4&&!memcmp(itemIcons[m].data(),magic,4);};
            for(size_t o=0;o+4*64<=arm.size();o+=4)
            {
                if(u16(o+4)!=2||u16(o+6)!=3)continue;
                bool ok=true;
                for(int i=1;i<60&&ok;i++)ok=is(u16(o+4*i),"RGCN")&&is(u16(o+4*i+2),"RLCN");
                if(!ok)continue;
                for(size_t e=o;e+4<=arm.size()&&itemIconIds.size()<std::max<size_t>(items.size(),1);e+=4)
                {
                    if(!is(u16(e),"RGCN")||!is(u16(e+2),"RLCN"))break;
                    itemIconIds.push_back({u16(e),u16(e+2)});
                }
                return;
            }
            return;
        }
        for(size_t o=0;o+8*64<=arm.size();o+=4)
        {
            if(u16(o)!=0||u16(o+2)>=n||u16(o+4)>=n)continue;
            bool ok=true;
            for(int i=1;i<60&&ok;i++){const size_t e=o+8*i;ok=u16(e)==i&&u16(e+2)<n&&u16(e+4)<n;}
            if(!ok)continue;
            for(size_t e=o;e+8<=arm.size()&&itemIconIds.size()<std::max<size_t>(items.size(),1);e+=8)
            {
                if(u16(e+2)>=n||u16(e+4)>=n)break;
                itemIconIds.push_back({u16(e+2),u16(e+4)});
            }
            return;
        }
    }
    std::vector<std::string> places;
    std::vector<int> mapPlaces;
    struct MapRecord {int place=-1,matrix=-1,worldX=-1,worldY=-1,encounter=-1;};
    std::vector<MapRecord> mapRecords;
    std::vector<Bytes> wildRecords;
    std::vector<std::pair<int,int>> placePoints;
    bool hgss=false;
    std::vector<std::string> trainerNames;
    std::vector<Bytes> trainerData,trainerTeams;
    static std::string Name(const std::vector<std::string>& names,int id,const char* fallback)
    {return id>=0 && id<(int)names.size()&&!names[id].empty()?names[id]:std::string(fallback)+" #"+std::to_string(id);}
    std::string Species(int id)const{return Name(species,id,"SPECIES");}
    std::string Move(int id)const{return id?Name(moves,id,"MOVE"):"NONE";}
    std::string ItemName(int id)const{return id?Name(items,id,"ITEM"):"NONE";}
    std::string PlaceName(int map)const
    {
        if(map>=0&&map<(int)mapPlaces.size())return Name(places,mapPlaces[map],"PLACE");
        return "MAP "+std::to_string(map);
    }
    struct Encounter {int species=0,low=0,high=0;std::string method;};
    std::vector<Encounter> Encounters(int map)const
    {
        std::vector<Encounter> out;
        if(map<0||map>=(int)mapRecords.size())return out;
        int set=mapRecords[map].encounter;if(set<0||set>=(int)wildRecords.size())return out;
        const auto& b=wildRecords[set];
        auto add=[&](int species,int low,int high,const std::string& method)
        {
            if(species<1||species>(gen5?649:493)||low<0||high<low||high>100)return;
            if((low==0)!=(high==0))return;
            for(auto& entry:out)if(entry.species==species&&entry.method==method)
            {
                if(entry.low==0||low==0){entry.low=entry.high=0;}
                else{entry.low=std::min(entry.low,low);entry.high=std::max(entry.high,high);}
                return;
            }
            out.push_back({species,low,high,method});
        };
        if(gen5)
        {
            if(b.size()!=232&&b.size()!=928)return out;
            static const int counts[]={12,12,12,5,5,5,5},offsets[]={8,56,104,152,172,192,212};
            static const char* methods[]={"GRASS","DARK GRASS","RUSTLING GRASS","SURF","RIPPLING WATER","FISHING","RIPPLING FISHING"};
            static const char* seasons[]={"SPRING ","SUMMER ","AUTUMN ","WINTER "};
            for(size_t block=0;block<b.size()/232;block++)for(int method=0;method<7;method++)if(b[block*232+method])
                for(int slot=0;slot<counts[method];slot++)
                {
                    size_t at=block*232+offsets[method]+slot*4;
                    add(U16(b.data()+at)&2047,b[at+2],b[at+3],std::string(b.size()==928?seasons[block]:"")+methods[method]);
                }
        }
        else if(hgss)
        {
            if(b.size()<196)return out;
            static const char* times[]={"MORNING GRASS","DAY GRASS","NIGHT GRASS"};
            if(b[0])for(int time=0;time<3;time++)for(int slot=0;slot<12;slot++)add(U16(b.data()+20+time*24+slot*2),b[8+slot],b[8+slot],times[time]);
            static const int counts[]={5,2,5,5,5};static const char* methods[]={"SURF","ROCK SMASH","OLD ROD","GOOD ROD","SUPER ROD"};
            size_t offset=100;
            for(int method=0;method<5;method++)
            {
                if(b[method+1])for(int slot=0;slot<counts[method];slot++)
                {size_t at=offset+slot*4;add(U16(b.data()+at+2),b[at],b[at+1],methods[method]);}
                offset+=counts[method]*4;
            }
            if(b[0])
            {
                for(int slot=0;slot<4;slot++)add(U16(b.data()+92+slot*2),0,0,slot<2?"HOENN RADIO":"SINNOH RADIO");
                for(int slot=0;slot<2;slot++)add(U16(b.data()+188+slot*2),0,0,"SWARM");
            }
            if(b[5])add(U16(b.data()+192),0,0,"NIGHT FISHING REPLACEMENT");
            if(b[3]||b[4]||b[5])add(U16(b.data()+194),0,0,"FISHING SWARM");
        }
        else
        {
            if(b.size()<424)return out;
            if(U32(b.data()))
            {
                for(int slot=0;slot<12;slot++)add(U32(b.data()+8+slot*8),U32(b.data()+4+slot*8),U32(b.data()+4+slot*8),"GRASS / CAVE");
                static const int slots[]={0,1,2,3,2,3,4,5,10,11,8,9,8,9,8,9,8,9,8,9};
                for(int n=0;n<20;n++)
                {int level=U32(b.data()+4+slots[n]*8);add(U32(b.data()+100+n*4+(n>=10?24:0)),level,level,n<2?"SWARM":n<6?"TIME REPLACEMENT":n<10?"POKE RADAR":"GBA SLOT");}
            }
            static const char* methods[]={"SURF","UNUSED","OLD ROD","GOOD ROD","SUPER ROD"};
            for(int method=0;method<5;method++)if(method!=1&&U32(b.data()+204+method*44))for(int slot=0;slot<5;slot++)
            {size_t at=208+method*44+slot*8;unsigned levels=U32(b.data()+at);add(U32(b.data()+at+4),(levels>>8)&255,levels&255,methods[method]);}
        }
        return out;
    }
    std::vector<std::string> EncounterLines(int map)const
    {
        std::vector<std::string> out;std::string method;
        for(const auto& entry:Encounters(map))
        {
            if(method!=entry.method){method=entry.method;out.push_back(method);}
            out.push_back(Species(entry.species)+(entry.low?" L"+std::to_string(entry.low)+(entry.high!=entry.low?"-"+std::to_string(entry.high):""):" - LEVEL VARIES"));
        }
        return out;
    }
    struct Marker {int x=0,y=0;bool ready=false,approximate=false;};
    Marker PlayerMarker(const Snapshot& snapshot)const
    {
        Marker out;
        if(!snapshot.ready||!regionMap.Ready())return out;
        if(gen5)
        {
            if(snapshot.map<0||snapshot.map>=(int)mapPlaces.size())return out;
            int place=mapPlaces[snapshot.map];
            if(place<0||place>=(int)placePoints.size()||placePoints[place].first<0)return out;
            out={placePoints[place].first,placePoints[place].second,true,true};
        }
        else
        {
            int map=snapshot.map,x=snapshot.x,y=snapshot.y;
            if(map<0||map>=(int)mapRecords.size())return out;
            const auto* record=&mapRecords[map];int bx=-1,by=-1;
            if(record->matrix==0&&x>=0&&y>=0){bx=x/32;by=y/32;}
            else if(hgss&&record->worldX>=0&&(record->worldX||record->worldY)){bx=record->worldX;by=record->worldY;out.approximate=true;}
            else if(snapshot.exitMap>=0&&snapshot.exitMap<(int)mapRecords.size()&&mapRecords[snapshot.exitMap].matrix==0&&snapshot.exitX>=0&&snapshot.exitY>=0)
            {bx=snapshot.exitX/32;by=snapshot.exitY/32;out.approximate=true;}
            if(bx<0||by<0||bx>=(hgss?47:30)||by>=(hgss?17:30))return {};
            out.x=hgss?bx*8+4:bx*7+5;out.y=hgss?(by+2)*8+4:by*7-34;out.ready=true;
        }
        if(out.x<0||out.y<0||out.x>=regionMap.width||out.y>=regionMap.height)return {};
        return out;
    }
    std::string ItemDescription(int id)const
    {
        return id>0&&id<(int)itemDescriptions.size()&&!itemDescriptions[id].empty()?itemDescriptions[id]:"NO DESCRIPTION AVAILABLE FOR THIS ITEM.";
    }

    int Type(int id,int which)const {if(id<0||id>=(int)personal.size()||personal[id].size()<8)return -1;int t=personal[id][6+which];return gen5 && t>=9?t+1:t;}
    struct Learned {int move=0,level=0;};
    std::vector<Learned> LearnedMoves(int id)const
    {
        std::vector<Learned> out;if(id<0||id>=(int)learnsets.size())return out;auto& b=learnsets[id];
        for(size_t at=0;Range(b.size(),at,gen5?4:2);at+=gen5?4:2){int value=U16(b.data()+at);if(value==65535)break;out.push_back({gen5?value:value&511,gen5?(int)U16(b.data()+at+2):value>>9});}
        return out;
    }
    std::vector<Learned> Upcoming(const Mon& mon,size_t limit=3)const
    {
        std::vector<Learned> out;
        if(mon.egg)return out;
        auto learned=LearnedMoves(mon.species);
        std::stable_sort(learned.begin(),learned.end(),[](const Learned& a,const Learned& b){return a.level<b.level;});
        for(const auto& entry:learned)
        {
            if(entry.level<=mon.level||entry.level>100||entry.move<=0)continue;
            if(std::find(mon.moves.begin(),mon.moves.end(),entry.move)!=mon.moves.end())continue;
            if(std::any_of(out.begin(),out.end(),[&](const Learned& other){return other.move==entry.move;}))continue;
            if(out.size()>=limit)break;
            out.push_back(entry);
        }
        return out;
    }
    std::string Evolution(int method,int value)const
    {
        if(gen5 && method==7)return "TRADE WITH THE REQUIRED PARTNER";
        if(gen5 && method>7)--method;
        const std::string level="LEVEL "+std::to_string(value);
        switch(method)
        {
        case 1:return "LEVEL UP WITH HIGH FRIENDSHIP";
        case 2:return "HIGH FRIENDSHIP, DAYTIME";
        case 3:return "HIGH FRIENDSHIP, NIGHTTIME";
        case 4:case 13:return level;
        case 5:return "TRADE";
        case 6:return "TRADE HOLDING "+ItemName(value);
        case 7:return "USE "+ItemName(value);
        case 8:return level+", ATTACK ABOVE DEFENCE";
        case 9:return level+", ATTACK EQUAL TO DEFENCE";
        case 10:return level+", ATTACK BELOW DEFENCE";
        case 11:case 12:return level+", PERSONALITY BRANCH";
        case 14:return level+", SPARE PARTY SLOT AND BALL";
        case 15:return "LEVEL UP WITH HIGH BEAUTY";
        case 16:return "MALE: USE "+ItemName(value);
        case 17:return "FEMALE: USE "+ItemName(value);
        case 18:return "LEVEL UP HOLDING "+ItemName(value)+", DAYTIME";
        case 19:return "LEVEL UP HOLDING "+ItemName(value)+", NIGHTTIME";
        case 20:return "LEVEL UP KNOWING "+Move(value);
        case 21:return "LEVEL UP WITH "+Species(value)+" IN PARTY";
        case 22:return level+", MALE";
        case 23:return level+", FEMALE";
        case 24:return "LEVEL UP IN A MAGNETIC AREA";
        case 25:return "LEVEL UP NEAR A MOSS ROCK";
        case 26:return "LEVEL UP NEAR AN ICE ROCK";
        default:return "SPECIAL EVOLUTION CONDITION";
        }
    }
    struct MoveInfo {bool ready=false;int type=-1,category=0,power=0,accuracy=0,pp=0;};
    MoveInfo MoveData(int id)const
    {
        if(id<=0||id>=(int)moveRecords.size())return {};
        const auto& b=moveRecords[id];if(b.size()<(gen5?6u:7u))return {};
        MoveInfo out;out.type=b[gen5?0:4];if(gen5&&out.type>=9)out.type++;
        out.category=b[2];if(out.category>2||out.type>17||out.type==9)return {};
        out.power=b[3];out.accuracy=b[gen5?4:5];out.pp=b[gen5?5:6];out.ready=true;return out;
    }
    struct TrainerMon {int species=0,level=0,item=0,form=0;std::array<int,4> moves{};bool customMoves=false;};
    std::vector<TrainerMon> TrainerTeam(int id)const
    {
        std::vector<TrainerMon> out;
        if(id<=0||id>=(int)trainerData.size()||id>=(int)trainerTeams.size())return out;
        const auto& header=trainerData[id];const auto& team=trainerTeams[id];
        if(header.size()<4||header[0]>3||header[3]<1||header[3]>6)return out;
        bool moves=header[0]&1,items=header[0]&2;
        size_t stride=8+(moves?8:0)+(items?2:0);
        if(!Range(team.size(),0,stride*header[3]))return out;
        for(int slot=0;slot<header[3];slot++)
        {
            size_t at=slot*stride;TrainerMon mon;
            mon.level=U16(team.data()+at+2);mon.species=U16(team.data()+at+4);
            mon.form=gen5?U16(team.data()+at+6):mon.species>>10;
            if(!gen5)mon.species&=511;
            if(mon.level<1||mon.level>100||mon.species<1||mon.species>(gen5?649:493))return {};
            at+=gen5?8:6;
            if(items){mon.item=U16(team.data()+at);at+=2;}
            mon.customMoves=moves;
            if(moves)for(int n=0;n<4;n++)mon.moves[n]=U16(team.data()+at+n*2);
            else
            {
                std::vector<int> learned;
                for(const auto& entry:LearnedMoves(mon.species))if(entry.level<=mon.level&&entry.move>0&&std::find(learned.begin(),learned.end(),entry.move)==learned.end())
                {if(learned.size()==4)learned.erase(learned.begin());learned.push_back(entry.move);}
                for(size_t n=0;n<learned.size();n++)mon.moves[n]=learned[n];
            }
            out.push_back(mon);
        }
        return out;
    }
    static bool ReferenceMatches(std::string name,const std::string& query)
    {
        for(char& c:name)c=(char)std::toupper((unsigned char)c);
        std::istringstream words(query);std::string word;
        while(words>>word)
        {
            for(char& c:word)c=(char)std::toupper((unsigned char)c);
            if(name.find(word)==std::string::npos)return false;
        }
        return true;
    }
    std::vector<std::pair<int,std::string>> ReferenceEntries(int kind,const std::string& query)const
    {
        const auto& names=kind==0?species:kind==1?items:kind==2?moves:kind==3?places:trainerNames;
        std::vector<std::pair<int,std::string>> out;
        if(kind<0||kind>4)return out;
        for(size_t id=kind==3?0:1;id<names.size();id++)
            if(!names[id].empty()&&ReferenceMatches(names[id],query))out.push_back({(int)id,names[id]});
        return out;
    }
    std::vector<std::string> AbilityDetails(int id)const
    {
        std::vector<std::string> out;
        if(id<=0||id>=(int)personal.size())return out;
        const auto& p=personal[id];size_t at=gen5?24:22,count=gen5?3:2;
        if(!Range(p.size(),at,count))return out;
        for(size_t i=0;i<count;i++)if(p[at+i])
        {
            std::string line=(gen5&&i==2?"HIDDEN ABILITY: ":"ABILITY: ")+Name(abilities,p[at+i],"ABILITY");
            if(std::find(out.begin(),out.end(),line)==out.end())out.push_back(line);
        }
        return out;
    }
    std::vector<std::string> EvolutionDetails(int id)const
    {
        std::vector<std::string> out;
        if(id<=0||id>=(int)evolutions.size())return out;
        const auto& records=evolutions[id];
        for(size_t at=0;Range(records.size(),at,6);at+=6)
        {
            int method=U16(records.data()+at),value=U16(records.data()+at+2),target=U16(records.data()+at+4);
            if(method&&target)out.push_back("EVOLVES TO "+Species(target)+": "+Evolution(method,value));
        }
        return out;
    }
    void Load(const Bytes& rom,const std::string& code,const Profile& profile)
    {
        if(!profile.supported)return;
        gen5=profile.gen5;
        const std::string family=code.substr(0,std::min<size_t>(3,code.size()));
        bool hg=family=="IPK"||family=="IPG";hgss=hg;
        const bool ss=family=="IPG";
        // (Black 2 and White 2: UPR ZX's gen5_offsets.ini text banks and archives, each bank checked
        // by what it holds and else looked for.)
        const bool b2=family=="IRE"||family=="IRD";
        auto msg=Narc(RomFile(rom,gen5?"a/0/0/2":hg?"a/0/2/7":"msgdata/pl_msg.narc"));
        auto bank=[&](int n){return n>=0&&n<(int)msg.size()?Text(msg[n],gen5):std::vector<std::string>{};};
        auto upper=[](std::string t){for(char& ch:t)ch=(char)std::toupper((unsigned char)ch);return t;};
        auto checked=[&](int n,int entry,const char* want)
        {
            auto b=bank(n);
            if(entry<(int)b.size()&&upper(b[entry]).find(want)!=std::string::npos)return b;
            for(size_t i=0;i<msg.size();i++){b=bank((int)i);if(entry<(int)b.size()&&b.size()>100&&upper(b[entry])==want)return b;}
            return std::vector<std::string>{};
        };
        if(b2)
        {
            species=checked(90,1,"BULBASAUR");moves=checked(403,1,"POUND");items=checked(64,1,"MASTER BALL");
            places=bank(109);
        }
        else {species=bank(gen5?70:hg?237:412);moves=bank(gen5?203:hg?750:647);items=bank(gen5?54:hg?222:392);
        // HGSS item.c LoadItemDescription: msg_0221; Platinum TEXT_BANK_ITEM_DESCRIPTIONS: 391.
        // Black (U) text bank 53 is also identified by UPR ZX's gen5_offsets.ini.
        places=bank(gen5?89:hg?279:433);}
        regionMap=ReferenceMap(Narc(RomFile(rom,gen5?"a/0/8/5":hg?"a/1/4/4":"graphic/tmap_gra.narc")),gen5,hg);
        if(!gen5){monIcons=Narc(RomFile(rom,hg?"a/0/2/0":"poketool/icongra/pl_poke_icon.narc"));if(!monIcons.empty())iconPalettes=CompanionIcons::Palettes(monIcons[0]);}
        else
        {
            monIcons=Narc(RomFile(rom,"a/0/0/7"));
            const Bytes arm=RomArm9(rom);
            if(!monIcons.empty()){iconPalettes=CompanionIcons::Palettes(monIcons[0]);FindGen5IconPalettes(arm);}
            for(size_t i=0;i<monIcons.size()&&i<16;i++)if(monIcons[i].size()>4&&!memcmp(monIcons[i].data(),"RGCN",4)){gen5IconStart=(int)i;break;}
            LoadItemIcons(rom,arm,false);
        }
        if(gen5)
        {
            auto zones=Narc(RomFile(rom,"a/0/1/2"));
            if(!zones.empty()&&zones[0].size()%48==0&&zones[0].size()/48<=4096)
                for(size_t at=0;Range(zones[0].size(),at,48);at+=48)
                {
                    // (Black 2 and White 2 keep flags in the top three bits; 0x1FFF there: none.)
                    MapRecord record;record.place=zones[0][at+0x1A];int set=U16(zones[0].data()+at+0x14);
                    if(b2)set=(set&0x1FFF)==0x1FFF?65535:set&0x1FFF;
                    record.encounter=set==65535?-1:set;
                    mapPlaces.push_back(record.place);mapRecords.push_back(record);
                }
        }
        const size_t wildRecordCount=Narc(RomFile(rom,b2?"a/1/2/7":gen5?"a/1/2/6":hg?(ss?"a/1/3/6":"a/0/3/7"):"fielddata/encountdata/pl_enc_data.narc")).size();
        if(!gen5)
        {
            auto arm=RomArm9(rom),names=RomFile(rom,"fielddata/maptable/mapname.bin");
            LoadItemIcons(rom,arm,hg);
            size_t offset=hg?0xF6BE0:0xE601C,count=names.size()/16;
            // (Another revision: the headers looked for, each place a name and each encounter table
            // one of the ROM's or none.)
            auto fits=[&](size_t at)
            {
                if(!Range(arm.size(),at,count*24))return false;
                for(size_t n=0;n<count;n++)
                {
                    const auto* h=arm.data()+at+n*24;
                    if(h[18]>=places.size())return false;
                    const int e=hg?(h[0]==255?-1:h[0]):(U16(h+14)==65535?-1:U16(h+14));
                    if(e>=(int)wildRecordCount)return false;
                }
                return true;
            };
            if(count>0&&!fits(offset))for(size_t at=0;at+count*24<=arm.size();at+=4)if(fits(at)){offset=at;break;}
            // UPR ZX gen4_offsets.ini/loadWildMapNames: fixed 24-byte headers, place at +18.
            if(names.size()%16==0&&count>0&&count<=4096&&Range(arm.size(),offset,count*24))
            {
                std::vector<MapRecord> records;std::vector<int> mapped;
                for(size_t n=0;n<count;n++)
                {
                    const auto* h=arm.data()+offset+n*24;MapRecord record;record.place=h[18];
                    if(record.place>=(int)places.size()){records.clear();break;}
                    record.matrix=U16(h+(hg?4:2));record.encounter=hg?(h[0]==255?-1:h[0]):(U16(h+14)==65535?-1:U16(h+14));
                    if(hg){unsigned coords=U16(h+2);record.worldX=(coords>>4)&63;record.worldY=coords>>10;}
                    records.push_back(record);mapped.push_back(record.place);
                }
                if(records.size()==count){mapRecords=std::move(records);mapPlaces=std::move(mapped);}
            }
        }
        else
        {
            placePoints.assign(places.size(),{-1,-1});auto towns=Narc(RomFile(rom,"a/0/8/6"));
            if(!towns.empty()&&towns[0].size()%54==0&&towns[0].size()/54<=4096)
                for(size_t at=0;Range(towns[0].size(),at,54);at+=54)
                {
                    const auto* t=towns[0].data()+at;int map=U16(t),x=U16(t+4),y=U16(t+6);
                    if(map<(int)mapPlaces.size()&&mapPlaces[map]<(int)placePoints.size()&&x<regionMap.width&&y<regionMap.height)placePoints[mapPlaces[map]]={x,y};
                }
        }
        wildRecords=Narc(RomFile(rom,b2?"a/1/2/7":gen5?"a/1/2/6":hg?(ss?"a/1/3/6":"a/0/3/7"):"fielddata/encountdata/pl_enc_data.narc"));
        itemDescriptions=bank(b2?63:gen5?53:hg?221:391);
        descriptions=bank(b2?442:gen5?235:hg?(ss?804:803):706);abilities=b2?checked(374,1,"STENCH"):bank(gen5?182:hg?720:610);
        // Trainer layouts and name banks are documented by UPR ZX Gen4/Gen5RomHandler.
        trainerData=Narc(RomFile(rom,b2?"a/0/9/1":gen5?"a/0/9/2":hg?"a/0/5/5":"poketool/trainer/trdata.narc"));
        trainerTeams=Narc(RomFile(rom,b2?"a/0/9/2":gen5?"a/0/9/3":hg?"a/0/5/6":"poketool/trainer/trpoke.narc"));
        trainerNames=bank(b2?382:gen5?190:hg?729:618);
        auto trainerClasses=bank(b2?383:gen5?191:hg?730:619);
        for(size_t id=1;id<trainerNames.size();id++)if(id<trainerData.size()&&trainerData[id].size()>=2)
        {
            int cls=trainerData[id][1];
            if(cls<(int)trainerClasses.size())trainerNames[id]=trainerClasses[cls]+" "+trainerNames[id];
        }
        personal=Narc(RomFile(rom,gen5?"a/0/1/6":hg?"a/0/0/2":"poketool/personal/pl_personal.narc"));
        evolutions=Narc(RomFile(rom,gen5?"a/0/1/9":hg?"a/0/3/4":"poketool/personal/evo.narc"));
        learnsets=Narc(RomFile(rom,gen5?"a/0/1/8":hg?"a/0/3/3":"poketool/personal/wotbl.narc"));
        moveRecords=Narc(RomFile(rom,gen5?"a/0/2/1":hg?"a/0/1/1":"poketool/waza/pl_waza_tbl.narc"));
    }
};
}
