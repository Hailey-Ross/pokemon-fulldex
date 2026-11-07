#pragma once
#include "state_slots.h"
#include <fstream>
#include <iomanip>
#include <map>
#include <vector>
#include <algorithm>
#include <ctime>

// Local library metadata is separate from ROMs, saves and the launcher's existing settings.
class LibraryHistory
{
    struct Record{bool favourite=false;long long played=0;};
    std::map<std::string,Record> records;
    std::string file;
public:
    void Load(const std::string& path)
    {
        file=path;records.clear();std::ifstream in(file);std::string rom;int favourite;long long played;
        while(in>>std::quoted(rom)>>favourite>>played)if(rom.size()<32768 && played>=0)records[rom]={favourite!=0,played};
    }
    bool Save()const
    {
        return StateSlots::Save(file,[&](const std::string& pending){std::ofstream out(pending);for(const auto& item:records)out<<std::quoted(item.first)<<' '<<item.second.favourite<<' '<<item.second.played<<'\n';out.close();return bool(out);});
    }
    bool Favourite(const std::string& rom)const{auto found=records.find(rom);return found!=records.end()&&found->second.favourite;}
    long long PlayedAt(const std::string& rom)const{auto found=records.find(rom);return found==records.end()?0:found->second.played;}
    bool Toggle(const std::string& rom){records[rom].favourite=!Favourite(rom);return Save();}
    bool Played(const std::string& rom){records[rom].played=(long long)std::time(nullptr);return Save();}
    template<class Game>void Apply(std::vector<Game>& games,const std::string& sort)const
    {
        for(auto& game:games){game.favourite=Favourite(game.rom);game.lastPlayed=PlayedAt(game.rom);}
        if(sort!="favourites"&&sort!="recent")return;
        std::stable_sort(games.begin(),games.end(),[&](const Game& a,const Game& b){if(a.platform!=b.platform)return a.platform<b.platform;if(sort=="favourites")return a.favourite>b.favourite;return a.lastPlayed>b.lastPlayed;});
    }
};
