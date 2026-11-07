#pragma once
#include "pokedaisy/pk_nds_call.h"
#include "NDS.h"

// All calls run between emulated frames. Imported recipes validate the exact game/version,
// the current field process and its idle state before installing their temporary hook.
class PokeDsActions
{
    melonDS::NDS* nds;
    const PkNdsFieldGame* game;
    static inline thread_local melonDS::NDS* reading=nullptr;
    static uint32_t Read32(uint32_t address)
    {
        return reading && address>=0x02000000 && address<0x02400000-3 ? reading->ARM9Read32(address) : 0;
    }
public:
    PokeDsActions(melonDS::NDS* core,const std::string& code,int revision):nds(core),game(pk_nds_field_game(code.c_str(),revision)){}
    bool Supported()const{return game!=nullptr;}
    int Flags(){reading=nds;return pk_nds_field_state(game,Read32,nullptr,0);}
    int OpenPc(){uint32_t args[4]={};return pk_nds_call_action(nds,game,PK_NDS_ACTION_PC,args);}
    void Frame(){if(game)pk_nds_call_frame(nds,game);}
};
