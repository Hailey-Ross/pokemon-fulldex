#pragma once
#include "companion_rom.h"
#include "companion_types.h"

// A static matchup reference, not a battle simulator. Only explicit, tested exceptions
// are applied; contextual move types are marked unknown instead of guessed.
namespace CompanionBattle
{
struct Matchup {bool known=false;int percent=100;std::string note;};
inline std::string Upper(std::string text){for(char& c:text)c=(char)std::toupper((unsigned char)c);return text;}
inline bool ItemIs(const Companion::Metadata& data,int item,const char* name){return item>0&&Upper(data.ItemName(item))==name;}
inline Matchup Evaluate(int id,const Companion::Metadata& data,const Companion::Battler& attacker,const Companion::Battler& defender)
{
    auto move=data.MoveData(id);
    if(!move.ready||!CompanionTypes::Valid(defender.type1))return {false,100,"DATA UNAVAILABLE"};
    if(move.category==2)return {false,100,"STATUS MOVE"};
    switch(id)
    {
    case 165:return {false,100,"STRUGGLE: SPECIAL TYPE RULES"};
    case 237:return {false,100,"HIDDEN POWER: IV-DEPENDENT TYPE"};
    case 311:return {false,100,"WEATHER BALL: WEATHER-DEPENDENT"};
    case 363:return {false,100,"NATURAL GIFT: BERRY-DEPENDENT"};
    case 449:return {false,100,"JUDGMENT: PLATE-DEPENDENT TYPE"};
    case 546:return {false,100,"TECHNO BLAST: DRIVE-DEPENDENT TYPE"};
    case 49:case 69:case 82:case 101:case 162:case 283:case 68:case 243:case 368:case 117:
    case 12:case 32:case 90:case 329:return {false,100,"SPECIAL DAMAGE / CONDITIONS"};
    }
    int type=attacker.ability==96?0:move.type;
    bool bypass=attacker.ability==104||(data.gen5&&(attacker.ability==163||attacker.ability==164));
    bool iron=ItemIs(data,defender.item,"IRON BALL"),ring=data.gen5&&ItemIs(data,defender.item,"RING TARGET");
    auto single=[&](int defence)
    {
        int value=CompanionTypes::Single(type,defence);
        if(value==0&&(ring||(type==4&&defence==2&&iron)||((type==0||type==1)&&defence==7&&attacker.ability==113)))return 100;
        return value;
    };
    int percent=single(defender.type1);
    if(CompanionTypes::Valid(defender.type2)&&defender.type1!=defender.type2)percent=percent*single(defender.type2)/100;
    auto immune=[&](const char* why){return Matchup{true,0,why};};
    if(data.gen5&&type==4&&ItemIs(data,defender.item,"AIR BALLOON"))return immune("AIR BALLOON");
    if(!bypass)
    {
        int ability=defender.ability;
        if(type==4&&ability==26&&!iron)return immune("LEVITATE");
        if(type==13&&(ability==10||ability==78||(data.gen5&&ability==31)))return immune("ELECTRIC-ABSORBING ABILITY");
        if(type==11&&(ability==11||ability==87||(data.gen5&&ability==114)))return immune("WATER-ABSORBING ABILITY");
        if(data.gen5&&type==12&&ability==157)return immune("SAP SIPPER");
        if(type==10&&ability==18)
        {
            if(!defender.statusKnown)return {false,percent,"FLASH FIRE: STATUS NOT MAPPED"};
            if(!(defender.status&32))return immune("FLASH FIRE");
        }
        if(ability==25&&percent<=100&&move.power)return immune("WONDER GUARD");
    }
    return {true,percent,bypass?"ABILITY BYPASS":attacker.ability==96?"NORMALIZE":""};
}
inline std::string Label(const Matchup& matchup)
{
    std::string text=matchup.known?CompanionTypes::Label(matchup.percent):"";
    if(!matchup.note.empty())text+=(text.empty()?"":" - ")+matchup.note;
    return text;
}
inline std::vector<std::string> PartyCoverage(const Companion::Snapshot& snapshot,const Companion::Metadata& data)
{
    std::vector<std::string> out;
    if(snapshot.battlers.size()<2||!snapshot.battlers[1].species)return out;
    const auto& foe=snapshot.battlers[1];
    for(const auto& mon:snapshot.party)
    {
        if(mon.egg||mon.hp<=0)continue;
        Companion::Battler candidate;candidate.species=mon.species;candidate.type1=data.Type(mon.species,0);candidate.type2=data.Type(mon.species,1);
        candidate.ability=mon.ability;candidate.item=mon.item;candidate.status=mon.status;candidate.statusKnown=true;
        int best=-1,worst=-1;
        for(int slot=0;slot<4;slot++)if(mon.moves[slot]&&mon.pp[slot]>0)
        {auto matchup=Evaluate(mon.moves[slot],data,candidate,foe);if(matchup.known)best=std::max(best,matchup.percent);}
        for(int move:foe.moves)if(move)
        {auto matchup=Evaluate(move,data,foe,candidate);if(matchup.known)worst=std::max(worst,matchup.percent);}
        out.push_back(data.Species(mon.species)+": OUT "+(best<0?std::string("?"):CompanionTypes::Label(best))+" / IN "+(worst<0?std::string("?"):CompanionTypes::Label(worst)));
    }
    return out;
}
inline std::vector<std::string> Lines(const Companion::Snapshot& snapshot,const Companion::Metadata& data)
{
    std::vector<std::string> out;
    if(snapshot.battlers.size()<2||!snapshot.battlers[1].species)return out;
    const auto& own=snapshot.battlers[0];const auto& foe=snapshot.battlers[1];
    out.push_back("ABILITY: "+Companion::Metadata::Name(data.abilities,foe.ability,"ABILITY"));
    out.push_back("ITEM: "+data.ItemName(foe.item));
    for(int slot=0;slot<4;slot++)if(own.moves[slot])
        out.push_back(data.Move(own.moves[slot])+" (PP "+std::to_string(own.pp[slot])+"): "+(own.pp[slot]?Label(Evaluate(own.moves[slot],data,own,foe)):"NO PP"));
    out.push_back("PARTY MATCHUPS: BEST OUT / WORST KNOWN IN");
    for(const auto& line:PartyCoverage(snapshot,data))out.push_back(line);
    out.push_back("STATIC MATCHUPS. WEATHER, VOLATILE EFFECTS, ABILITY SUPPRESSION AND DAMAGE ARE NOT SIMULATED.");
    return out;
}
}
