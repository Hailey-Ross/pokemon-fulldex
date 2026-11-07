#pragma once
#include "companion_data.h"
#include "state_slots.h"
#include <fstream>
#include <map>
#include <set>

// Optional observation tracker. Its history is independent of cartridge saves and save states.
// It records observed faints/first wild encounters; it does not enforce challenge rules.
class NuzlockeTracker
{
    std::string file;
    bool enabled=false,wasBattle=false;
    std::map<uint32_t,int> deaths;
    std::map<int,int> encounters;
public:
    void Load(const std::string& path)
    {
        file=path;deaths.clear();encounters.clear();enabled=false;wasBattle=false;
        std::ifstream in(file);std::string header;int version=0,active=0;
        if(!(in>>header>>version>>active)||header!="FULLDEX_RUN"||version!=1)return;
        enabled=active!=0;char kind;unsigned long long key;int species;
        while(in>>kind>>key>>species)if(species>0&&species<=1025)
        {if(kind=='F'&&key<=0xFFFFFFFFu)deaths[(uint32_t)key]=species;else if(kind=='E'&&key<=65535)encounters[(int)key]=species;}
    }
    bool Save()const
    {
        return StateSlots::Save(file,[&](const std::string& pending){std::ofstream out(pending);out<<"FULLDEX_RUN 1 "<<enabled<<'\n';for(auto item:deaths)out<<"F "<<item.first<<' '<<item.second<<'\n';for(auto item:encounters)out<<"E "<<item.first<<' '<<item.second<<'\n';out.close();return bool(out);});
    }
    bool Enabled()const{return enabled;}
    bool SetEnabled(bool on){enabled=on;wasBattle=false;return Save();}
    bool Reset(){deaths.clear();encounters.clear();wasBattle=false;return Save();}
    bool Dead(uint32_t pid)const{return deaths.count(pid)!=0;}
    const std::map<uint32_t,int>& Deaths()const{return deaths;}
    const std::map<int,int>& Encounters()const{return encounters;}
    bool Observe(const Companion::Snapshot& snapshot)
    {
        if(!enabled||!snapshot.ready)return true;
        bool changed=false;
        for(const auto& mon:snapshot.party)if(!mon.egg&&mon.hp==0&&!deaths.count(mon.personality))
        {deaths[mon.personality]=mon.species;changed=true;}
        bool battle=snapshot.battlers.size()>1&&snapshot.battlers[1].species>0;
        if(battle&&!wasBattle&&!snapshot.trainerBattle&&!encounters.count(snapshot.map))
        {encounters[snapshot.map]=snapshot.battlers[1].species;changed=true;}
        wasBattle=battle;
        return !changed||Save();
    }
};
