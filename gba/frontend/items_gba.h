// The BAG's USE, GIVE and TAKE in the GBA Companion, the way the game does them, by calling its own functions between
// frames (pk_game_call): a Repel as ItemUseOutOfBattle_Repel ends (VarSet of the steps, the Repel out of the bag), medicine
// and PP items through the game's ExecuteTableBasedItemEffect after its own "It won't have any effect." check, a held item
// through SetMonData (which re-encrypts the Pokemon and fixes its checksum), the bag through AddBagItem / RemoveBagItem.
// Never loses an item: whatever fails after the bag changed puts the bag and the Pokemon back. Adapted from PokeDaisy by
// Lidor Itzhari - https://github.com/lidor30/pokedaisy (FieldItems.kt and its records; GPL 3): retail Ruby, Sapphire,
// Emerald, FireRed rev 1 and both LeafGreens, each function checked by the CRC32 of its first 32 bytes.
#pragma once
#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <mgba/core/core.h>
#include <mgba-util/crc32.h>

#include "fly_gba.h"

namespace GbaItems
{
struct Fns
{
    const char* name;
    uint32_t varGet, varSet, checkBagHasItem, removeBagItem, fieldControlsLocked;
    int repelVar;
    uint32_t items;           // gItems (44-byte records)
    uint32_t crcs[5];         // varGet, varSet, checkBagHasItem, removeBagItem, fieldControlsLocked
    uint32_t getMonData, setMonData, itemNoEffect, itemEffect, addBagItem, party, partyCount, scratch;
    int itemCount;
    std::set<int> unholdable; // pockets no Pokemon can hold (key items, TMs and HMs)
    std::vector<uint32_t> partyCrcs; // getMonData, setMonData, itemNoEffect where there is one, itemEffect, addBagItem
    std::set<uint32_t> useFuncs;     // the game's field-use routines for medicine, PP items and Repels
};
inline const std::vector<Fns>& Tables()
{
    static const std::vector<Fns> t = {
        { "Emerald", 0x0809D694, 0x0809D6B0, 0x080D6724, 0x080D6AA4, 0x08098E6C, 0x4021, 0x085839A0,
          { 0x7D0D07B6, 0x519C39C0, 0x312F6576, 0x379EF742, 0x2532A407 },
          0x0806A518, 0x0806ACAC, 0, 0x0806BD04, 0x080D6928, 0x020244EC, 0x020244E9, 0x0203FFF0, 377, { 5, 3 },
          { 0x9670EBDE, 0x8D555FF8, 0xEADAC6C9, 0x75BC5970 }, { 0x080FDEA0, 0x080FDEF4, 0x080FDF10, 0x080FE0BC } },
        { "FireRed", 0x0806E57C, 0x0806E598, 0x08099F54, 0x0809A1EC, 0x0806996C, 0x4020, 0x083DB098,
          { 0xE6DE47BA, 0x6E7CC2BE, 0xD30D656E, 0xC40BF208, 0x9C26CCA4 },
          0x0803FBFC, 0x08040390, 0x08042428, 0x080413D4, 0x0809A098, 0x02024284, 0x02024029, 0x0203FFF0, 375, { 2, 4 },
          { 0xA73A32CD, 0x71F5A020, 0x788AB547, 0xEADAC6C9, 0xC316CD20 }, { 0x080A16F4, 0x080A1710, 0x080A172C, 0x080A19AC } },
        { "LeafGreen", 0x0806E568, 0x0806E584, 0x08099F14, 0x0809A1AC, 0x08069958, 0x4020, 0x083DAE64,
          { 0xE6DE47BA, 0x6E7CC2BE, 0xD30D656E, 0xC40BF208, 0x9C26CCA4 },
          0x0803FBE8, 0x0804037C, 0x08042414, 0x080413C0, 0x0809A058, 0x02024284, 0x02024029, 0x0203FFF0, 375, { 2, 4 },
          { 0x7841F205, 0x9B11B0A6, 0x788AB547, 0xEADAC6C9, 0xC316CD20 }, { 0x080A16B4, 0x080A16D0, 0x080A16EC, 0x080A196C } },
        { "LeafGreen", 0x0806E57C, 0x0806E598, 0x08099F28, 0x0809A1C0, 0x0806996C, 0x4020, 0x083DAED4,
          { 0xE6DE47BA, 0x6E7CC2BE, 0xD30D656E, 0xC40BF208, 0x9C26CCA4 },
          0x0803FBFC, 0x08040390, 0x08042428, 0x080413D4, 0x0809A06C, 0x02024284, 0x02024029, 0x0203FFF0, 375, { 2, 4 },
          { 0xA73A32CD, 0x71F5A020, 0x788AB547, 0xEADAC6C9, 0xC316CD20 }, { 0x080A16C8, 0x080A16E4, 0x080A1700, 0x080A1980 } },
        { "Ruby", 0x08069274, 0x08069290, 0x080A92F4, 0x080A9558, 0x08065568, 0x4021, 0x083C5580,
          { 0x98EFB2B6, 0x82AA042B, 0x87E0CA93, 0xD7C65886, 0xA306873D },
          0x0803CB60, 0x0803D1FC, 0, 0x0803E18C, 0x080A9444, 0x03004360, 0x03004350, 0x0203FFF0, 349, { 5, 3 },
          { 0x7D459654, 0x59B8ECA4, 0xEADAC6C9, 0x891E35D5 }, { 0x080C9DD0, 0x080C9E5C, 0x080C9E78, 0x080CA034 } },
        { "Sapphire", 0x08069278, 0x08069294, 0x080A92F4, 0x080A9558, 0x0806556C, 0x4021, 0x083C55DC,
          { 0x98EFB2B6, 0x82AA042B, 0x87E0CA93, 0xD7C65886, 0xA306873D },
          0x0803CB60, 0x0803D1FC, 0, 0x0803E18C, 0x080A9444, 0x03004360, 0x03004350, 0x0203FFF0, 349, { 5, 3 },
          { 0x7D459654, 0x59B8ECA4, 0xEADAC6C9, 0x891E35D5 }, { 0x080C9DD0, 0x080C9E5C, 0x080C9E78, 0x080CA034 } },
    };
    return t;
}

// The TM screen (PokeDaisy's TmTeach.kt and its records): what the bag does for a TM or HM, from the field. The START
// menu's way off the field (controls locked, the fade to black, then CleanupOverworldWindowsAndTilemaps), the TM in
// gSpecialVar_ItemId, gItemUseCB = ItemUseCB_TMHM, the party cursor on the first slot, CB2_ShowPartyMenuForItemUse; its
// exit (gPartyMenu.exitCallback) pointed at CB2_ReturnToField instead of a bag that never opened. Everything after is the
// game's own: "Teach which POKeMON?", ABLE / NOT ABLE / LEARNED, forgetting a move, the TM used up or not. Not Ruby and
// Sapphire: their party menu goes back to the bag by a fixed call, with nowhere else to point it.
struct TmFns
{
    const char* name;
    uint32_t specialVarItemId, itemUseCb, itemUseCbTmhm, showPartyMenuForItemUse, partyMenu;
    int partyMenuSlotOff;
    uint32_t initPartyMenu, mainCallback2, setMainCallback2, returnToField, cleanupOverworld, fadeScreen, lockControls, unlockControls;
    uint32_t crcs[9]; // itemUseCbTmhm, showPartyMenuForItemUse, initPartyMenu, setMainCallback2, returnToField, cleanupOverworld, fadeScreen, lock, unlock
};
inline const std::vector<TmFns>& TmTables()
{
    static const std::vector<TmFns> t = {
        { "Emerald", 0x0203CE7C, 0x03006328, 0x081B6DC4, 0x081B617C, 0x0203CEC8, 9, 0x081B01E0, 0x030022C4, 0x08000540, 0x080860C8, 0x08085D34, 0x080ABCD0, 0x08098E54, 0x08098E60,
          { 0xF253CF6E, 0x34D2BE1C, 0x99B75656, 0x6228571D, 0x224FD691, 0x4A3D66F5, 0xF9959006, 0xB9FB85A7, 0xB037C8AF } },
        { "FireRed", 0x0203AD30, 0x03005E98, 0x08125BB8, 0x08124D04, 0x0203B0A0, 9, 0x0811EC48, 0x030030F4, 0x08000558, 0x080567F0, 0x08056404, 0x0807A82C, 0x08069954, 0x08069960,
          { 0xEA99DF01, 0xAA3B20FF, 0x1655AE08, 0x7FD43C20, 0x95CA9A3E, 0xCE27BF5A, 0xF9959006, 0x0111A86E, 0xBAB654BA } },
        { "FireRed", 0x0203AD30, 0x03005E98, 0x08125B40, 0x08124C8C, 0x0203B0A0, 9, 0x0811EBD0, 0x030030F4, 0x08000544, 0x080567DC, 0x080563F0, 0x0807A818, 0x08069940, 0x0806994C,
          { 0x9CA850AA, 0xAA3B20FF, 0x3099DC9B, 0x7FD43C20, 0xB892BBB7, 0xE67BC364, 0xF9959006, 0x0111A86E, 0xBAB654BA } },
        { "LeafGreen", 0x0203AD30, 0x03005E98, 0x08125B18, 0x08124C64, 0x0203B0A0, 9, 0x0811EBA8, 0x030030F4, 0x08000544, 0x080567DC, 0x080563F0, 0x0807A7EC, 0x08069940, 0x0806994C,
          { 0x679AE1F0, 0xAA3B20FF, 0x1DE165E1, 0x7FD43C20, 0xB892BBB7, 0xAD9CEAC8, 0xF9959006, 0x0111A86E, 0xBAB654BA } },
        { "LeafGreen", 0x0203AD30, 0x03005E98, 0x08125B90, 0x08124CDC, 0x0203B0A0, 9, 0x0811EC20, 0x030030F4, 0x08000558, 0x080567F0, 0x08056404, 0x0807A800, 0x08069954, 0x08069960,
          { 0xAB261E3B, 0xAA3B20FF, 0xBF379CB9, 0x7FD43C20, 0x95CA9A3E, 0xD3AF9AFE, 0xF9959006, 0x0111A86E, 0xBAB654BA } },
    };
    return t;
}
inline bool IsTmHm(int id) { return id >= 289 && id <= 346; }

inline bool IsRepel(int id) { return id == 83 || id == 84 || id == 86; }
inline bool IsMail(int id) { return id >= 121 && id <= 132; }
inline bool IsMedicine(int id)
{
    static const std::set<int> m = { 38, 44, 63, 64, 65, 66, 67, 70, 133, 134, 135, 136, 137, 139, 141, 142 };
    return (id >= 13 && id <= 33) || m.count(id);
}
inline bool IsPpOneMove(int id) { return id == 34 || id == 35 || id == 138 || id == 69 || id == 71; }
inline bool IsPpAllMoves(int id) { return id == 36 || id == 37; }

enum Result { USED, REPEL_ON, NOT_NOW, NONE_LEFT, NO_EFFECT, GIVEN, TAKEN, BAG_FULL, CHANGED, FAILED };
inline const char* Why(Result r)
{
    switch (r)
    {
    case REPEL_ON: return "A REPEL IS STILL WORKING";
    case NOT_NOW: return "NOT RIGHT NOW: TRY AGAIN ON THE FIELD";
    case NONE_LEFT: return "THERE ARE NONE LEFT";
    case NO_EFFECT: return "IT WON'T HAVE ANY EFFECT";
    case BAG_FULL: return "THE BAG IS FULL";
    case CHANGED: return "YOUR PARTY CHANGED: LOOK AGAIN";
    case FAILED: return "THAT DIDN'T WORK";
    default: return "";
    }
}
enum Verdict { OK, NOTHING, EGG, HOLDS_MAIL, HOLDS_IT };
struct Target { int slot = 0; uint32_t personality = 0; int held = 0; Verdict verdict = OK; bool moves[4] = {}; };

class Items
{
public:
    // The table whose functions (the bag's and the party's) are where it says in [clean], and what its gItems allow.
    void Find(mCore* c, const std::vector<uint8_t>& clean, const GbaFly::Fns* field)
    {
        core = c;
        fns = nullptr;
        tm = nullptr;
        fly = field;
        holdable.clear();
        usable.clear();
        auto crc = [&](uint32_t fn) -> uint32_t {
            const size_t o = (fn & ~1u) - 0x08000000u;
            return o + 32 <= clean.size() ? (uint32_t)doCrc32(clean.data() + o, 32) : 0;
        };
        for (const Fns& f : Tables())
        {
            const uint32_t fn[5] = { f.varGet, f.varSet, f.checkBagHasItem, f.removeBagItem, f.fieldControlsLocked };
            bool ok = true;
            for (int i = 0; i < 5 && ok; i++) ok = crc(fn[i]) == f.crcs[i];
            std::vector<uint32_t> pf = { f.getMonData, f.setMonData };
            if (f.itemNoEffect) pf.push_back(f.itemNoEffect);
            pf.push_back(f.itemEffect);
            pf.push_back(f.addBagItem);
            ok = ok && pf.size() == f.partyCrcs.size();
            for (size_t i = 0; i < pf.size() && ok; i++) ok = crc(pf[i]) == f.partyCrcs[i];
            if (!ok) continue;
            fns = &f;
            for (const TmFns& t : TmTables())
            {
                const uint32_t tf[9] = { t.itemUseCbTmhm, t.showPartyMenuForItemUse, t.initPartyMenu, t.setMainCallback2, t.returnToField, t.cleanupOverworld, t.fadeScreen, t.lockControls, t.unlockControls };
                bool tok = true;
                for (int i = 0; i < 9 && tok; i++) tok = crc(tf[i]) == t.crcs[i];
                if (tok) { tm = &t; break; }
            }
            // (gItems: importance at +24, pocket at +26, the field-use routine at +28.)
            for (int id = 1; id < f.itemCount; id++)
            {
                const size_t o = f.items - 0x08000000u + (size_t)id * 44;
                if (o + 44 > clean.size()) break;
                if (clean[o + 24] == 0 && !f.unholdable.count(clean[o + 26]) && !IsMail(id)) holdable.insert(id);
                const uint32_t use = (clean[o + 28] | clean[o + 29] << 8 | clean[o + 30] << 16 | (uint32_t)clean[o + 31] << 24) & ~1u;
                if ((IsRepel(id) || IsMedicine(id) || IsPpOneMove(id) || IsPpAllMoves(id)) && f.useFuncs.count(use)) usable.insert(id);
            }
            return;
        }
    }
    void Forget() { fns = nullptr; tm = nullptr; fly = nullptr; core = nullptr; }
    bool CanTeach(int id) const { return fns && tm && IsTmHm(id); }
    bool Supported() const { return fns && fly; }
    bool CanUse(int id) const { return usable.count(id) > 0; }
    bool CanGive(int id) const { return holdable.count(id) > 0; }
    bool PicksMon(int id) const { return CanUse(id) && !IsRepel(id); }
    bool PicksMove(int id) const { return IsPpOneMove(id); }

    // A Repel, on the field: the steps (its holdEffectParam) into the Repel counter, one out of the bag.
    Result UseRepel(int id, int& steps)
    {
        steps = 0;
        if (!IsRepel(id) || !CanUse(id)) return FAILED;
        if (Result g = Gate(); g != USED) return g;
        uint32_t r = 0;
        if (!Call(fns->varGet, { (uint32_t)fns->repelVar }, &r)) return NOT_NOW;
        if (r & 0xFFFF) return REPEL_ON;
        if (!Call(fns->checkBagHasItem, { (uint32_t)id, 1 }, &r) || (r & 0xFF) != 1) return NONE_LEFT;
        steps = core->busRead8(core, fns->items + (uint32_t)id * 44 + 19);
        if (!steps) return FAILED;
        if (!Call(fns->varSet, { (uint32_t)fns->repelVar, (uint32_t)steps }) || !Call(fns->removeBagItem, { (uint32_t)id, 1 }, &r) || (r & 0xFF) != 1) return FAILED;
        return USED;
    }
    // Who [id] could go to: each party slot with its verdict (USE: the game's no-effect check, per move for a one-move
    // PP item; GIVE: eggs and Mail holders out). Empty and [why] set when it can't go to anyone now.
    std::vector<Target> Targets(int id, bool give, Result& why)
    {
        std::vector<Target> out;
        why = Gate();
        if (why != USED) return out;
        if (!(give ? CanGive(id) : PicksMon(id))) { why = FAILED; return out; }
        uint32_t r = 0;
        if (!Call(fns->checkBagHasItem, { (uint32_t)id, 1 }, &r) || (r & 0xFF) != 1) { why = NONE_LEFT; return out; }
        const int count = PartyCount();
        bool ok = true;
        for (int s = 0; s < count && ok; s++)
        {
            Target t;
            t.slot = s;
            t.personality = Get(s, 0, ok);
            t.held = (int)Get(s, 12, ok);
            if (Get(s, 45, ok)) t.verdict = EGG;
            else if (give && IsMail(t.held)) t.verdict = HOLDS_MAIL;
            else if (give && t.held == id) t.verdict = HOLDS_IT;
            else if (!give && IsPpOneMove(id))
            {
                bool any = false;
                for (int m = 0; m < 4; m++) { t.moves[m] = Get(s, 13 + m, ok) != 0 && Helps(id, s, m, ok); any = any || t.moves[m]; }
                t.verdict = any ? OK : NOTHING;
            }
            else if (!give) t.verdict = Helps(id, s, 0, ok) ? OK : NOTHING;
            out.push_back(t);
        }
        if (!ok) { why = FAILED; out.clear(); }
        return out;
    }
    // USE medicine / a PP item on [t]'s Pokemon (move [move] for a one-move item): the check again, the effect, one out of the bag.
    Result UseOn(int id, const Target& t, int move)
    {
        if (Result g = Gate(); g != USED) return g;
        if (!PicksMon(id)) return FAILED;
        bool ok = true;
        const int m = IsPpOneMove(id) ? std::min(3, std::max(0, move)) : 0;
        if (!StillThere(t, ok)) return ok ? CHANGED : FAILED;
        uint32_t r = 0;
        if (!Call(fns->checkBagHasItem, { (uint32_t)id, 1 }, &r) || (r & 0xFF) != 1) return NONE_LEFT;
        if (!Helps(id, t.slot, m, ok) || !ok) return ok ? NO_EFFECT : FAILED;
        if (!Call(fns->itemEffect, { Mon(t.slot), (uint32_t)id, (uint32_t)t.slot, (uint32_t)m }, &r) || (r & 0xFF)) return NO_EFFECT;
        if (!Call(fns->removeBagItem, { (uint32_t)id, 1 }, &r) || (r & 0xFF) != 1) return FAILED;
        return USED;
    }
    // GIVE [id] to [t]'s Pokemon: out of the bag, what it held into the bag (no room: nothing changes), then held.
    Result Give(int id, const Target& t, int& was)
    {
        was = 0;
        if (Result g = Gate(); g != USED) return g;
        if (!CanGive(id)) return FAILED;
        bool ok = true;
        if (!StillThere(t, ok)) return ok ? CHANGED : FAILED;
        const int old = (int)Get(t.slot, 12, ok);
        if (!ok || IsMail(old)) return FAILED;
        if (old == id) return NO_EFFECT;
        uint32_t r = 0;
        if (!Call(fns->removeBagItem, { (uint32_t)id, 1 }, &r) || (r & 0xFF) != 1) return NONE_LEFT;
        if (old && (!Call(fns->addBagItem, { (uint32_t)old, 1 }, &r) || (r & 0xFF) != 1)) { Call(fns->addBagItem, { (uint32_t)id, 1 }); return BAG_FULL; }
        if (SetHeld(t.slot, id)) { was = old; return GIVEN; }
        if (old && Call(fns->removeBagItem, { (uint32_t)old, 1 }, &r) && (r & 0xFF) == 1) SetHeld(t.slot, old);
        Call(fns->addBagItem, { (uint32_t)id, 1 });
        return FAILED;
    }
    // TAKE what [t]'s Pokemon holds into the bag (no room: it keeps it).
    Result Take(const Target& t, int& item)
    {
        item = 0;
        if (Result g = Gate(); g != USED) return g;
        bool ok = true;
        if (!StillThere(t, ok)) return ok ? CHANGED : FAILED;
        const int held = (int)Get(t.slot, 12, ok);
        if (!ok || IsMail(held)) return FAILED;
        if (!held) return NO_EFFECT;
        uint32_t r = 0;
        if (!Call(fns->addBagItem, { (uint32_t)held, 1 }, &r) || (r & 0xFF) != 1) return BAG_FULL;
        if (SetHeld(t.slot, 0)) { item = held; return TAKEN; }
        Call(fns->removeBagItem, { (uint32_t)held, 1 });
        return FAILED;
    }
    // The party slot's Pokemon as TAKE needs it (no calls: read from the snapshot's personality).
    // A TM or HM, first step: the bag's checks, then the controls locked and the fade to black. TmFinish follows once
    // it's black (about 30 frames, the game running).
    Result TmStart(int id)
    {
        if (!CanTeach(id)) return FAILED;
        if (Result g = Gate(); g != USED) return g;
        uint32_t r = 0;
        if (!Call(fns->checkBagHasItem, { (uint32_t)id, 1 }, &r) || (r & 0xFF) != 1) return NONE_LEFT;
        if (!PartyCount()) return FAILED;
        if (!Call(tm->lockControls, {}) || !Call(tm->fadeScreen, { 1, 0 })) return FAILED;
        return USED;
    }
    enum TmStep { TM_OPEN, TM_LATER, TM_GONE, TM_BACK };
    // The second step, the screen black: the game's TM party menu, going back to the field when it closes. TM_GONE:
    // something else took the game off the field during the fade (a script, a battle), so nothing. TM_BACK: it couldn't
    // start, the field is back as it was.
    TmStep TmFinish(int id)
    {
        if (!tm || !core) return TM_GONE;
        if (!pk_game_call_safe(core)) return TM_LATER;
        if ((core->busRead32(core, fly->gMain + 4) & ~1u) != (fly->cb2Overworld & ~1u)) return TM_GONE;
        bool ok = Call(tm->cleanupOverworld, {});
        if (ok)
        {
            core->busWrite16(core, tm->specialVarItemId, (uint16_t)id);
            core->busWrite32(core, tm->itemUseCb, tm->itemUseCbTmhm | 1u);
            core->busWrite8(core, tm->partyMenu + (uint32_t)tm->partyMenuSlotOff, 0);
            ok = Call(tm->showPartyMenuForItemUse, {});
        }
        if (ok)
        {
            if ((core->busRead32(core, tm->mainCallback2) & ~1u) == (tm->initPartyMenu & ~1u))
                core->busWrite32(core, tm->partyMenu, tm->returnToField | 1u);   // (back to the field, not the bag)
            else
                ok = Call(tm->setMainCallback2, { tm->returnToField | 1u });     // (no room for the menu: the field)
            if (ok) return TM_OPEN;
        }
        Call(tm->fadeScreen, { 0, 0 });
        Call(tm->unlockControls, {});
        return TM_BACK;
    }
    Target Slot(int slot, uint32_t personality, int held) const { Target t; t.slot = slot; t.personality = personality; t.held = held; return t; }

private:
    mCore* core = nullptr;
    const Fns* fns = nullptr;
    const TmFns* tm = nullptr;
    const GbaFly::Fns* fly = nullptr;
    std::set<int> holdable, usable;

    uint32_t Mon(int slot) const { return fns->party + (uint32_t)slot * 100; }
    int PartyCount() const { return std::min(6, (int)core->busRead8(core, fns->partyCount)); }
    bool Call(uint32_t fn, std::vector<uint32_t> a, uint32_t* r0 = nullptr)
    {
        uint32_t args[4] = {};
        for (size_t i = 0; i < a.size() && i < 4; i++) args[i] = a[i];
        return pk_game_call(core, fn | 1u, args, r0) != 0;
    }
    uint32_t Get(int slot, int field, bool& ok) { uint32_t r = 0; ok = Call(fns->getMonData, { Mon(slot), (uint32_t)field }, &r) && ok; return r; }
    bool StillThere(const Target& t, bool& ok)
    {
        return t.slot >= 0 && t.slot < PartyCount() && Get(t.slot, 0, ok) == t.personality && !Get(t.slot, 45, ok) && ok;
    }
    // The game's own "It won't have any effect." check: PokemonItemUseNoEffect where the game has it (FireRed and
    // LeafGreen), else the effect itself on the Pokemon and its 100 bytes put back (Ruby, Sapphire and Emerald's way).
    bool Helps(int id, int slot, int move, bool& ok)
    {
        if (id == 63 && Get(slot, 11, ok) == 303) return false;   // (HP Up on Shedinja)
        uint32_t r = 0;
        if (fns->itemNoEffect) { ok = Call(fns->itemNoEffect, { Mon(slot), (uint32_t)id, (uint32_t)slot, (uint32_t)move }, &r) && ok; return (r & 0xFF) == 0; }
        uint8_t saved[100];
        for (int i = 0; i < 100; i++) saved[i] = (uint8_t)core->busRead8(core, Mon(slot) + (uint32_t)i);
        ok = Call(fns->itemEffect, { Mon(slot), (uint32_t)id, (uint32_t)slot, (uint32_t)move }, &r) && ok;
        for (int i = 0; i < 100; i++) core->busWrite8(core, Mon(slot) + (uint32_t)i, saved[i]);
        return (r & 0xFF) == 0;
    }
    // SetMonData(mon, MON_DATA_HELD_ITEM, &item) through two scratch bytes, then read back.
    bool SetHeld(int slot, int item)
    {
        const uint8_t b0 = (uint8_t)core->busRead8(core, fns->scratch), b1 = (uint8_t)core->busRead8(core, fns->scratch + 1);
        core->busWrite8(core, fns->scratch, (uint8_t)item);
        core->busWrite8(core, fns->scratch + 1, (uint8_t)(item >> 8));
        const bool called = Call(fns->setMonData, { Mon(slot), 12, fns->scratch });
        core->busWrite8(core, fns->scratch, b0);
        core->busWrite8(core, fns->scratch + 1, b1);
        bool ok = true;
        return called && (int)Get(slot, 12, ok) == item && ok;
    }
    // (On the field with the controls free, nothing of the game's own in progress.)
    Result Gate()
    {
        if (!Supported() || !pk_game_call_safe(core)) return NOT_NOW;
        const uint32_t cb = core->busRead32(core, fly->gMain + 4);
        if ((cb & ~1u) != (fly->cb2Overworld & ~1u)) return NOT_NOW;
        uint32_t locked = 0;
        if (!Call(fns->fieldControlsLocked, {}, &locked)) return NOT_NOW;
        return (locked & 0xFF) ? NOT_NOW : USED;
    }
};
} // namespace GbaItems
