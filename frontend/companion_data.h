#pragma once
// Read-only DS companion data, adapted from PokeDaisy Gen4Reader/Gen5Reader (GPL-3.0).
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
// Reference: 52f959770de4085bacfda16e6d77be7e91134bc4. No ROM or save writes.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace Companion
{
using Bytes = std::vector<uint8_t>;
inline uint16_t U16(const uint8_t* p) { return p[0] | (uint16_t)p[1] << 8; }
inline uint32_t U32(const uint8_t* p) { return U16(p) | (uint32_t)U16(p+2) << 16; }
inline bool Range(size_t size, size_t at, size_t count) { return at <= size && count <= size-at; }
// Latin transliteration for the portable font. These are character encodings, not game text.
inline std::string LatinAscii(uint16_t c)
{
    if(c>=32&&c<127)return std::string(1,(char)c);
    switch(c)
    {
    case 0x00C0:case 0x00C1:case 0x00C2:case 0x00C3:case 0x00C4:case 0x00C5:case 0x0100:case 0x0102:case 0x0104:return "A";
    case 0x00C6:return "AE";
    case 0x00C7:case 0x0106:case 0x0108:case 0x010A:case 0x010C:return "C";
    case 0x00C8:case 0x00C9:case 0x00CA:case 0x00CB:case 0x0112:case 0x0114:case 0x0116:case 0x0118:case 0x011A:return "E";
    case 0x00CC:case 0x00CD:case 0x00CE:case 0x00CF:case 0x0128:case 0x012A:case 0x012C:case 0x012E:case 0x0130:return "I";
    case 0x00D0:case 0x010E:return "D";
    case 0x00D1:case 0x0143:case 0x0145:case 0x0147:return "N";
    case 0x00D2:case 0x00D3:case 0x00D4:case 0x00D5:case 0x00D6:case 0x00D8:case 0x014C:case 0x014E:case 0x0150:return "O";
    case 0x00D9:case 0x00DA:case 0x00DB:case 0x00DC:case 0x0168:case 0x016A:case 0x016C:case 0x016E:case 0x0170:case 0x0172:return "U";
    case 0x00DD:case 0x0176:case 0x0178:return "Y";
    case 0x00DE:return "TH";
    case 0x00DF:return "ss";
    case 0x00E0:case 0x00E1:case 0x00E2:case 0x00E3:case 0x00E4:case 0x00E5:case 0x0101:case 0x0103:case 0x0105:return "a";
    case 0x00E6:return "ae";
    case 0x00E7:case 0x0107:case 0x0109:case 0x010B:case 0x010D:return "c";
    case 0x00E8:case 0x00E9:case 0x00EA:case 0x00EB:case 0x0113:case 0x0115:case 0x0117:case 0x0119:case 0x011B:return "e";
    case 0x00EC:case 0x00ED:case 0x00EE:case 0x00EF:case 0x0129:case 0x012B:case 0x012D:case 0x012F:return "i";
    case 0x00F0:case 0x010F:return "d";
    case 0x00F1:case 0x0144:case 0x0146:case 0x0148:case 0x0149:return "n";
    case 0x00F2:case 0x00F3:case 0x00F4:case 0x00F5:case 0x00F6:case 0x00F8:case 0x014D:case 0x014F:case 0x0151:return "o";
    case 0x00F9:case 0x00FA:case 0x00FB:case 0x00FC:case 0x0169:case 0x016B:case 0x016D:case 0x016F:case 0x0171:case 0x0173:return "u";
    case 0x00FD:case 0x00FF:case 0x0177:return "y";
    case 0x00FE:return "th";
    case 0x011C:case 0x011E:case 0x0120:case 0x0122:return "G";
    case 0x011D:case 0x011F:case 0x0121:case 0x0123:return "g";
    case 0x0124:return "H";
    case 0x0125:return "h";
    case 0x0132:return "IJ";
    case 0x0133:return "ij";
    case 0x0134:return "J";
    case 0x0135:return "j";
    case 0x0136:return "K";
    case 0x0137:return "k";
    case 0x0139:case 0x013B:case 0x013D:case 0x013F:return "L";
    case 0x013A:case 0x013C:case 0x013E:case 0x0140:return "l";
    case 0x0152:return "OE";
    case 0x0153:return "oe";
    case 0x0154:case 0x0156:case 0x0158:return "R";
    case 0x0155:case 0x0157:case 0x0159:return "r";
    case 0x015A:case 0x015C:case 0x015E:case 0x0160:return "S";
    case 0x015B:case 0x015D:case 0x015F:case 0x0161:case 0x017F:return "s";
    case 0x0162:case 0x0164:return "T";
    case 0x0163:case 0x0165:return "t";
    case 0x0174:return "W";
    case 0x0175:return "w";
    case 0x0179:case 0x017B:case 0x017D:return "Z";
    case 0x017A:case 0x017C:case 0x017E:return "z";
    case 0x2018:case 0x2019:return "'";
    case 0x201C:case 0x201D:return "\"";
    case 0x2026:return "...";
    case 0x2013:case 0x2014:return "-";
    case 0x2640:return "F";
    case 0x2642:return "M";
    default:return " ";
    }
}
inline std::string AsciiChar(uint16_t c, bool gen5)
{
    if (gen5) return LatinAscii(c);
    if (c >= 0x121 && c <= 0x12A) return std::string(1, '0'+c-0x121);
    if (c >= 0x12B && c <= 0x144) return std::string(1, 'A'+c-0x12B);
    if (c >= 0x145 && c <= 0x15E) return std::string(1, 'a'+c-0x145);
    // Gen4's Western glyph codes follow the Nitro charmap used by tools/msgenc.
    switch(c)
    {
    case 0x015F:case 0x0160:case 0x0161:case 0x0162:case 0x0163:case 0x0164:return "A";
    case 0x0165:return "AE";
    case 0x0166:return "C";
    case 0x0167:case 0x0168:case 0x0169:case 0x016A:return "E";
    case 0x016B:case 0x016C:case 0x016D:case 0x016E:return "I";
    case 0x016F:return "D";
    case 0x0170:return "N";
    case 0x0171:case 0x0172:case 0x0173:case 0x0174:case 0x0175:case 0x0177:return "O";
    case 0x0178:case 0x0179:case 0x017A:case 0x017B:return "U";
    case 0x017C:return "Y";
    case 0x017D:return "TH";
    case 0x017E:return "ss";
    case 0x017F:case 0x0180:case 0x0181:case 0x0182:case 0x0183:case 0x0184:case 0x01A3:return "a";
    case 0x0185:return "ae";
    case 0x0186:return "c";
    case 0x0187:case 0x0188:case 0x0189:case 0x018A:return "e";
    case 0x018B:case 0x018C:case 0x018D:case 0x018E:return "i";
    case 0x018F:return "d";
    case 0x0190:return "n";
    case 0x0191:case 0x0192:case 0x0193:case 0x0194:case 0x0195:case 0x0197:case 0x01A4:return "o";
    case 0x0198:case 0x0199:case 0x019A:case 0x019B:return "u";
    case 0x019C:case 0x019E:return "y";
    case 0x019D:return "th";
    case 0x019F:return "OE";
    case 0x01A0:return "oe";
    case 0x01A1:return "S";
    case 0x01A2:return "s";
    case 0x01AB:return "!";
    case 0x01AC:return "?";
    case 0x01AD:return ",";
    case 0x01AE:return ".";
    case 0x01AF:return "...";
    case 0x01B1:return "/";
    case 0x01B2:case 0x01B3:return "'";
    case 0x01B4:case 0x01B5:return "\"";
    case 0x01B9:return "(";
    case 0x01BA:return ")";
    case 0x01BB:return "M";
    case 0x01BC:return "F";
    case 0x01BD:return "+";
    case 0x01BE:return "-";
    case 0x01C1:return "=";
    case 0x01C2:return "&";
    case 0x01C4:return ":";
    case 0x01C5:return ";";
    case 0x01D2:return "%";
    default:break;
    }
    if (c == 0x1BE) return "-";
    if (c == 0x1AE) return ".";
    return " ";
}
inline Bytes Crypt(const uint8_t* input, size_t count, uint32_t seed)
{
    Bytes output(input,input+count);
    for (size_t i=0; i+1<count; i+=2)
    {
        seed=seed*0x41C64E6Du+0x6073;
        uint16_t value=U16(input+i)^(seed>>16);
        output[i]=value;output[i+1]=value>>8;
    }
    return output;
}
struct Mon
{
    int species=0, level=0, hp=0, maxHp=0, item=0, nature=0, ability=0;
    uint32_t personality=0, status=0, experience=0;
    bool egg=false;
    std::string nickname;
    std::array<int,4> moves{},pp{};
    // HP, attack, defence, speed, special attack, special defence.
    std::array<int,6> iv{},ev{},stats{};
};
inline bool DecodeMon(const uint8_t* raw, size_t length, bool gen5, Mon& result)
{
    if (length < (gen5 ? 0xDCu : 0xECu)) return false;
    const uint32_t pid=U32(raw), flags=U16(raw+4), checksum=U16(raw+6);
    auto valid=[&](const Bytes& b) { uint16_t sum=0;for(size_t i=0;i<128;i+=2)sum+=U16(b.data()+i);return sum==checksum; };
    Bytes blocks(raw+8,raw+136);
    if (!(flags&2)) { auto decrypted=Crypt(raw+8,128,checksum);if(valid(decrypted))blocks=std::move(decrypted); }
    if(!valid(blocks))return false;
    static const char* orders[]={"ABCD","ABDC","ACBD","ACDB","ADBC","ADCB","BACD","BADC","BCAD","BCDA","BDAC","BDCA",
        "CABD","CADB","CBAD","CBDA","CDAB","CDBA","DABC","DACB","DBAC","DBCA","DCAB","DCBA"};
    const char* order=orders[((pid>>13)&31)%24];
    const uint8_t* a=blocks.data()+(strchr(order,'A')-order)*32;
    const uint8_t* b=blocks.data()+(strchr(order,'B')-order)*32;
    const uint8_t* c=blocks.data()+(strchr(order,'C')-order)*32;
    const int species=U16(a);
    if(species<1 || species>(gen5?649:493))return false;
    auto sane=[](const Bytes& p) { return p[4]>=1 && p[4]<=100 && U16(p.data()+8)>0 && U16(p.data()+8)<=999 && U16(p.data()+6)<=U16(p.data()+8); };
    Bytes party(raw+136,raw+length);
    if(!(flags&1)) { auto decrypted=Crypt(raw+136,length-136,pid);if(sane(decrypted))party=std::move(decrypted); }
    if(!sane(party))return false;
    result={};result.species=species;result.personality=pid;result.level=party[4];
    result.hp=U16(party.data()+6);result.maxHp=U16(party.data()+8);result.status=U32(party.data());
    result.item=U16(a+2);result.ability=a[13];result.experience=U32(a+8);result.nature=gen5?b[25]:pid%25;
    uint32_t iv=U32(b+16);result.egg=(iv>>30)&1;
    result.stats[0]=result.maxHp;
    for(int i=0;i<6;i++) { result.iv[i]=(iv>>(i*5))&31;result.ev[i]=a[16+i];if(i)result.stats[i]=U16(party.data()+8+i*2); }
    for(int i=0;i<4;i++){result.moves[i]=U16(b+i*2);result.pp[i]=b[8+i];}
    for(int i=0;i<11;i++){uint16_t ch=U16(c+i*2);if(ch==0xFFFF)break;result.nickname+=AsciiChar(ch,gen5);}
    while(!result.nickname.empty() && result.nickname.back()==' ')result.nickname.pop_back();
    return true;
}
struct Profile
{
    bool supported=false,gen5=false;
    uint32_t pointer=0,body=0,party=0,bag=0,dex=0,location=0,trainer=0,repel=0;
    int maxSpecies=493;
    std::vector<int> pockets;
    // (pointer 0: a version whose addresses in memory are not known (White, Black 2, White 2, other
    // revisions): its save found by looking for it (FindSave); no live position or battle.)
    bool hgss=false;
    uint32_t misc=0; // (Gen 5: the money, then the badges)
    // (Gen 5: the game's system, which leads to the live position (+0x1C, then +0x114: the map,
    // x and z) and, in Black and White, the battle. 0: not known.)
    uint32_t system=0;
    bool gen5Battle=false;
    uint32_t battleProc=93; // (the battle's process id: Black and White 93, Black 2 and White 2 0xA7)
};
inline Profile ProfileFor(const std::string& code, int revision)
{
    Profile p;
    const std::string family=code.size()>=3?code.substr(0,3):code;
    // (The US carts, whose addresses are known; then each game's other carts and revisions, the
    // same save layout found in memory by looking.)
    if((code=="IPKE" || code=="IPGE") && revision==0)
        p={true,false,0x021D2228,0x10,0x90,0x644,0x12B8,0x1234,0x64,0x6909,493,{165,50,101,12,40,64,24,30}};
    // (Other HeartGold and SoulSilver revisions tested (rev 10) keep the save and the field where
    // the US carts do: the same addresses tried first, the save looked for if they do not hold.)
    else if(family=="IPK" || family=="IPG")
        p={true,false,0x021D2228,0x10,0x90,0x644,0x12B8,0x1234,0x64,0x6909,493,{165,50,101,12,40,64,24,30}};
    else if(code=="CPUE" && revision==1)
        p={true,false,0x021C0794,0x14,0x98,0x630,0x1328,0x1280,0x68,0x8073,493,{165,50,100,12,40,64,15,30}};
    else if(family=="CPU")
        p={true,false,0x021C0794,0x14,0x98,0x630,0x1328,0x1280,0x68,0x8073,493,{165,50,100,12,40,64,15,30}};
    else if(code=="IRBO" && revision==0)
        p={true,true,0x020A7298,0,0x18E00,0x18400,0x21600,0x19500,0x19400,0x21B31,649,{310,83,109,48,64}};
    // (White: the same code as Black, its globals 0x20 on; found by the same instructions in its ARM9.)
    else if(code=="IRAO" && revision==0)
        p={true,true,0x020A72B8,0,0x18E00,0x18400,0x21600,0x19500,0x19400,0x21B31,649,{310,83,109,48,64}};
    else if(family=="IRB" || family=="IRA")
        p={true,true,0,0,0x18E00,0x18400,0x21600,0x19500,0x19400,0x21B31,649,{310,83,109,48,64}};
    // (Black 2 and White 2: PKHeX's SAV5B2W2: the Pokedex at 0x21400, the money and badges at
    // 0x21100; the Repel's steps not known, so not read.)
    else if(family=="IRE" || family=="IRD")
        p={true,true,0,0,0x18E00,0x18400,0x21400,0x19500,0x19400,0,649,{310,83,109,48,64}};
    p.hgss=family=="IPK" || family=="IPG";
    p.misc=family=="IRE" || family=="IRD" ? 0x21100 : 0x21200;
    // (Black 2's found in savestates: the player's map and position at [[0x021410F4]+0x1C]+0x114;
    // White 2's the same instructions' 0x02141134. Their battles not yet mapped.)
    if(code=="IRBO" && revision==0){p.system=0x02146248;p.gen5Battle=true;}
    else if(code=="IRAO" && revision==0){p.system=0x02146268;p.gen5Battle=true;}
    else if(code=="IREO" && revision==0){p.system=0x021410F4;p.gen5Battle=true;p.battleProc=0xA7;}
    else if(code=="IRDO" && revision==0){p.system=0x02141134;p.gen5Battle=true;p.battleProc=0xA7;}
    return p;
}
struct Item {int id=0,quantity=0,pocket=0;};
inline const char* PocketName(const Profile& profile,int pocket)
{
    static const char* gen4[]={"ITEMS","KEY ITEMS","TMS / HMS","MAIL","MEDICINE","BERRIES","BALLS","BATTLE ITEMS"};
    static const char* gen5[]={"ITEMS","KEY ITEMS","TMS / HMS","MEDICINE","BERRIES"};
    if(pocket<0)return "ALL";
    int count=profile.gen5?5:8;
    if(pocket>=count || pocket>=(int)profile.pockets.size())return "UNKNOWN POCKET";
    return profile.gen5?gen5[pocket]:gen4[pocket];
}
inline int StepPocket(const Profile& profile,int pocket,int direction)
{
    int count=(int)profile.pockets.size();
    if(!count)return -1;
    // Include ALL as the first choice and wrap in both directions.
    int index=std::clamp(pocket+1,0,count);
    return (index+(direction<0?count:1))%(count+1)-1;
}

struct Battler
{
    int species=0,level=0,hp=0,maxHp=0,type1=-1,type2=-1,ability=0,item=0;
    uint32_t status=0;bool statusKnown=false;
    std::array<int,4> moves{},pp{};
};
struct Snapshot
{
    bool supported=false,ready=false,dexReady=false,locationLive=false,trainerBattle=false;
    std::string message;
    int map=0,x=0,y=0,exitMap=-1,exitX=-1,exitY=-1,repel=0,invalidSlots=0,badges=0;
    int64_t money=-1; // (the trainer's money; -1 not read)
    std::vector<Mon> party;
    std::vector<Battler> battlers;
    std::vector<Item> items;
    std::vector<bool> seen,caught;
};
using Reader=std::function<bool(uint32_t,void*,size_t)>;
// (Main memory: 4MB, or 8MB where Fulldex gives Black 2 and White 2 more for the wider map.)
inline bool MainRange(uint32_t at,size_t size) { return at>=0x02000000 && at<0x02800000 && size<=0x02800000-at; }
// Where the save is in memory, for a cart whose addresses are not known: the party's header (its
// size, 6, then how many, 1 to 6) where the save keeps it, with the Pokedex's mark (0xBEEFCAFE) where
// the save keeps that. Looked for at most once a second (Read is called often); kept while it holds.
inline uint32_t FindSave(const Profile& p,const Reader& read)
{
    static uint32_t found=0,tries=0;
    auto at=[&](uint32_t a,size_t n){std::vector<uint8_t> v(n);return read(a,v.data(),n)?v:std::vector<uint8_t>();};
    auto holds=[&](uint32_t base)
    {
        if(!MainRange(base,p.dex+8)||!MainRange(base,p.party+8))return false;
        auto head=at(base+p.party,8),dex=at(base+p.dex,4);
        return head.size()==8&&dex.size()==4&&U32(head.data())==6&&U32(head.data()+4)>=1&&U32(head.data()+4)<=6&&U32(dex.data())==0xBEEFCAFE;
    };
    if(found&&holds(found))return found;
    found=0;
    if(tries++%2)return 0; // (Read: every half second; a look: once a second)
    const uint32_t chunk=0x40000;
    for(uint32_t start=0x02000000;start<0x02800000;start+=chunk)
    {
        auto block=at(start,std::min<uint32_t>(chunk+8,0x02800000-start));
        if(block.size()<8)continue;
        for(uint32_t o=0;o+8<=block.size();o+=4)
        {
            if(U32(block.data()+o)!=6)continue;
            const uint32_t count=U32(block.data()+o+4);
            if(count<1||count>6)continue;
            const uint32_t base=start+o-p.party; // (the party's place less its offset: where the save starts)
            if(start+o<p.party+0x02000000)continue;
            if(holds(base)){found=base;return found;}
        }
    }
    return 0;
}
inline Snapshot Read(const Profile& p,const Reader& read)
{
    Snapshot s;s.supported=p.supported;
    if(!p.supported){s.message="NO VERIFIED COMPANION PROFILE FOR THIS VERSION";return s;}
    auto bytes=[&](uint32_t at,size_t count){Bytes v;if(!count||!MainRange(at,count))return v;v.resize(count);if(!read(at,v.data(),count))v.clear();return v;};
    auto pointer=[&](uint32_t at){auto v=bytes(at,4);return v.size()==4?U32(v.data()):0u;};
    uint32_t base=0;
    if(p.pointer)
    {
        base=pointer(p.pointer);
        if(p.gen5){if(!MainRange(base,0x14))base=0;else{base=pointer(base+0x10);if(MainRange(base,0x38))base=pointer(base+0x34);else base=0;}}
        if(MainRange(base, p.gen5?0x24000:0x20000))base+=p.body;else base=0;
        // (A revision that keeps it elsewhere: the party's header not there; looked for instead.)
        if(base){auto head=bytes(base+p.party,4);if(head.size()!=4||U32(head.data())!=6)base=FindSave(p,read);}
    }
    else base=FindSave(p,read);
    if(!base){s.message="START OR LOAD A GAME TO READ ITS DATA";return s;}
    auto head=bytes(base+p.party,8);
    if(head.size()!=8||U32(head.data())!=6||U32(head.data()+4)>6){s.message="PARTY DATA IS NOT READY";return s;}
    int count=U32(head.data()+4),stride=p.gen5?0xDC:0xEC;
    auto party=bytes(base+p.party+8,count*stride);
    if(party.size()!=(size_t)count*stride){s.message="PARTY DATA IS UNAVAILABLE";return s;}
    for(int i=0;i<count;i++){Mon mon;if(DecodeMon(party.data()+i*stride,stride,p.gen5,mon))s.party.push_back(mon);else s.invalidSlots++;}
    int slots=0;for(int n:p.pockets)slots+=n;
    auto bag=bytes(base+p.bag,slots*4);
    if(bag.size()==(size_t)slots*4){int slot=0;for(size_t pocket=0;pocket<p.pockets.size();pocket++)for(int i=0;i<p.pockets[pocket];i++,slot++){int id=U16(bag.data()+slot*4),qty=U16(bag.data()+slot*4+2);if(id && qty && id<2048 && qty<=999)s.items.push_back({id,qty,(int)pocket});}}
    auto loc=bytes(base+p.location,0x1C);
    if(loc.size()==0x1C){s.map=p.gen5?U16(loc.data()+4):U32(loc.data());s.x=p.gen5?(int32_t)U32(loc.data()+16)>>16:U32(loc.data()+8);s.y=p.gen5?(int32_t)U32(loc.data()+24)>>16:U32(loc.data()+12);}
    if(!p.gen5)
    {
        auto exit=bytes(base+p.location+0x50,0x14);
        if(exit.size()==0x14){s.exitMap=U32(exit.data());s.exitX=U32(exit.data()+8);s.exitY=U32(exit.data()+12);}
    }
    if(p.gen5)
    {
        auto badge=bytes(base+p.misc+4,1);if(!badge.empty())s.badges=badge[0];
        auto cash=bytes(base+p.misc,4);if(cash.size()==4)s.money=U32(cash.data()); // (PKHeX SAV5: the trainer's money, before the badges)
        uint32_t system=p.system?pointer(p.system):0,data=MainRange(system,0x20)?pointer(system+0x1C):0;
        auto live=MainRange(data,0x124)?bytes(data+0x114,16):Bytes{};
        if(!live.empty())
        {
            // (Only when it looks like a place and a position: a game between screens leaves other things there.)
            const int map=U16(live.data()),x=(int32_t)U32(live.data()+4)>>16,y=(int32_t)U32(live.data()+12)>>16;
            if(map>0&&map<2048&&x>=0&&x<4096&&y>=0&&y<4096){s.map=map;s.x=x;s.y=y;s.locationLive=true;}
        }
    }
    else
    {
        auto trainer=bytes(base+p.trainer,0x20);if(!trainer.empty())s.badges=trainer[0x1A]|(p.hgss?trainer[0x1F]<<8:0);
        if(trainer.size()>=0x18)s.money=U32(trainer.data()+0x14); // (PKHeX SAV4: the trainer's money, at 0x14 in its block)
        const bool hg=p.pointer==0x021D2228;
        uint32_t field=p.pointer?pointer(hg?0x021D4158:0x021C07DC):0,avatar=MainRange(field,0x44)?pointer(field+(hg?0x40:0x3C)):0;
        uint32_t object=MainRange(avatar,0x34)?pointer(avatar+0x30):0;
        auto coordinates=MainRange(object,0x70)?bytes(object+0x64,12):Bytes{};
        uint32_t location=MainRange(field,0x24)?pointer(field+(hg?0x20:0x1C)):0;
        auto current=MainRange(location,0x14)?bytes(location,0x14):Bytes{};
        if(!coordinates.empty()&&!current.empty()){s.map=U32(current.data());s.x=(int32_t)U32(coordinates.data());s.y=(int32_t)U32(coordinates.data()+8);s.locationLive=true;}
    }
    auto repel=p.repel?bytes(base+p.repel,1):Bytes{};if(!repel.empty())s.repel=repel[0];
    const int setBytes=p.gen5?84:64;
    auto dex=bytes(base+p.dex,p.gen5?8+setBytes*5:0x340);
    if(!dex.empty()&&U32(dex.data())==0xBEEFCAFE){s.dexReady=true;s.seen.resize(p.maxSpecies+1);s.caught.resize(p.maxSpecies+1);auto bit=[&](int at,int id){return (dex[at+(id-1)/8]>>((id-1)%8))&1;};for(int id=1;id<=p.maxSpecies;id++){bool seen=p.gen5?false:bit(4+64,id);bool caught=bit(p.gen5?8:4,id);if(p.gen5)for(int set=1;set<=4;set++)seen=seen||bit(8+set*setBytes,id);s.seen[id]=seen||caught;s.caught[id]=caught;}}
    // Only battle layouts mapped in the source reference are enabled.
    uint32_t battle=0;
    if(!p.gen5 && p.pointer==0x021C0794)
    {
        uint32_t fs=pointer(0x021C07DC),manager=MainRange(fs,4)?pointer(fs):0;
        uint32_t child=MainRange(manager,8)?pointer(manager+4):0;
        if(MainRange(child,0x20)&&pointer(child)==pointer(0x020EA358))
        {
            uint32_t system=pointer(child+0x1C);
            if(MainRange(system,0x34)){battle=pointer(system+0x30);s.trainerBattle=(pointer(system+0x2C)&1)!=0;}
        }
        auto mons=MainRange(battle,0x2D40+4*0xC0)?bytes(battle+0x2D40,4*0xC0):Bytes{};
        if(!mons.empty())for(int i=0;i<4;i++)
        {
            const auto* b=mons.data()+i*0xC0;Battler m;m.species=U16(b);m.level=b[0x34];m.hp=U32(b+0x4C);m.maxHp=U32(b+0x50);
            m.status=U32(b+0x6C);m.statusKnown=true;
            m.type1=b[0x24];m.type2=b[0x25];m.ability=b[0x27];m.item=U16(b+0x78);
            for(int j=0;j<4;j++){m.moves[j]=U16(b+0x0C+j*2);m.pp[j]=b[0x2C+j];}
            if(m.species<1||m.species>493||m.level<1||m.level>100||m.hp<0||m.maxHp<1||m.hp>m.maxHp)m={};
            s.battlers.push_back(m);
        }
    }
    if(p.gen5 && p.system && p.gen5Battle)
    {
        uint32_t system=pointer(p.system),procSys=MainRange(system,12)?pointer(system+8):0;
        uint32_t proc=MainRange(procSys,12)?pointer(procSys+8):0;
        uint32_t main=MainRange(proc,0x18)&&pointer(proc+4)==p.battleProc?pointer(proc+0x14):0;
        uint32_t param=MainRange(proc,0x14)?pointer(proc+0x10):0;
        if(MainRange(param,4))s.trainerBattle=pointer(param)!=0;
        auto parties=MainRange(main,0xCC+0x38)?bytes(main+0xCC,0x38):Bytes{};
        if(!parties.empty())for(int client=0;client<2;client++)
        {
            uint32_t member=U32(parties.data()+client*0x1C);auto b=bytes(member,0x140);Battler m;
            if(!b.empty())
            {
                // (The maximum HP first, then the HP now: a hurt Tepig at 13 of 29 in the savestate.)
                m.species=U16(b.data()+12);m.level=b[0x18];m.maxHp=U16(b.data()+14);m.hp=U16(b.data()+16);
                m.type1=b[0xF8];m.type2=b[0xF9];if(m.type1>=9)m.type1++;if(m.type2>=9)m.type2++;
                m.ability=U16(b.data()+0x16);m.item=U16(b.data()+0x12);
                for(int j=0;j<4;j++){m.moves[j]=U16(b.data()+0x104+j*14);m.pp[j]=b[0x106+j*14];}
                if(m.species<1||m.species>649||m.level<1||m.level>100||m.maxHp<1||m.hp>m.maxHp)m={};
            }
            s.battlers.push_back(m);
        }
    }
    s.ready=true;s.message=s.invalidSlots?"SOME PARTY SLOTS COULD NOT BE VERIFIED":"READ-ONLY SNAPSHOT";
    return s;
}
inline const char* Status(uint32_t status)
{
    if(status&7)return "ASLEEP";
    if(status&0x88)return "POISONED";
    if(status&0x10)return "BURNED";
    if(status&0x20)return "FROZEN";
    if(status&0x40)return "PARALYSED";
    return "OK";
}
}
