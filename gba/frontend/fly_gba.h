// FLY from the companion, in the GBA games: the way the game does it, by calling the game's own functions between
// frames (pk_game_call), not by steering its menus. The party menu's checks (the field free, the badge, a map Fly may
// leave), the Fly map's towns (those the game itself would list), then what closing the Fly map does: controls locked,
// the screen faded to black, the Pokemon that knows Fly under the party cursor (the cut-in shows it) and the warp
// handed to the game, which plays its own Fly. Adapted from PokeDaisy by Lidor Itzhari -
// https://github.com/lidor30/pokedaisy (Fly.kt, FieldItems.kt and its records; GPL 3): every address is retail's,
// from pret's decompilations built byte for byte, and each function is checked by the CRC32 of its first 32 bytes, so
// a ROM that differs simply has no FLY.
#pragma once
#include <algorithm>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include <mgba/core/core.h>
#include <mgba-util/crc32.h>

#include "pokedaisy/pk_call_lite.h"

namespace GbaFly
{
struct HealWarp
{
    uint32_t toHealLocation, toMapWarp, returnToField, saveBlock2; // (saveBlock2: the block itself where static, else its pointer)
    bool saveBlock2Static;
    int specialSec[2], specialHeal[2];                             // map section -> heal location (the League, the Frontier / Tower)
    int genderSec, maleHeal, femaleHeal;
};
struct Fns
{
    const char* name;
    // The field and the party
    uint32_t fieldControlsLocked, getMonData, party, partyCount;
    uint32_t lockedCrc, getMonDataCrc;
    uint32_t gMain, cb2Overworld;                                   // (the field's main callback: still the field when FLY finishes)
    // FLY
    uint32_t flagGet; int badgeFlag;
    uint32_t setUpFieldMoveFly, getMapsecType, fadeScreen, lockControls, unlockControls, setFlyWarpDestination;
    uint32_t partyMenuSlotId, destinations; int firstMapsec, count;
    std::vector<uint32_t> crcs;                                     // flagGet, setUp, getMapsecType, fade, lock, unlock, then the warp's
    std::set<int> flyTypes;
    std::vector<int> extraSecs;
    uint32_t mapHeader;                                             // (Ruby / Sapphire: SetUpFieldMove_Fly takes gMapHeader.mapType)
    bool heal; HealWarp hw;
};

inline const std::vector<Fns>& Tables()
{
    static const HealWarp emeraldHeal = { 0x08084CCC, 0x08084C28, 0x080B69DC, 0x03005D90, false, { 15, 58 }, { 11, 22 }, 0, 12, 13 };
    static const HealWarp rubyHeal = { 0x08053558, 0x080534B0, 0x080865DC, 0x02024EA4, true, { 15, 58 }, { 11, 21 }, 0, 12, 13 };
    static const std::vector<Fns> t = {
        { "Emerald", 0x08098E6C, 0x0806A518, 0x020244EC, 0x020244E9, 0x2532A407, 0x9670EBDE, 0x030022C0, 0x08085E5C,
          0x0809D790, 0x86C, 0x081B5884, 0x08123D58, 0x080ABCD0, 0x08098E54, 0x08098E60, 0,
          0x0203CED1, 0x085A1E3C, 0, 50,
          { 0xBCF97BF3, 0x13BE1A7A, 0x4B625BC4, 0xF9959006, 0xB9FB85A7, 0xB037C8AF, 0x74DBCE3A, 0x2F1D18CA, 0xA284286D },
          { 2, 4 }, { 58 }, 0, true, emeraldHeal },
        { "Ruby", 0x08065568, 0x0803CB60, 0x03004360, 0x03004350, 0xA306873D, 0x7D459654, 0x03001770, 0x080543C4,
          0x08069360, 0x80C, 0x08054244, 0x080FB778, 0x0807D664, 0x08065550, 0x0806555C, 0,
          0x03005CE0, 0x083E793C, 0, 50,
          { 0xAC4D17B2, 0x6F375BAA, 0x0190971B, 0xF9959006, 0xA003C21A, 0x9807B468, 0x5BF43808, 0x2F1D18CA, 0xB2A1E0F7 },
          { 2, 4 }, { 58 }, 0x0202E828, true, rubyHeal },
        { "Sapphire", 0x0806556C, 0x0803CB60, 0x03004360, 0x03004350, 0xA306873D, 0x7D459654, 0x03001770, 0x080543C8,
          0x08069364, 0x80C, 0x08054248, 0x080FB778, 0x0807D664, 0x08065554, 0x08065560, 0,
          0x03005CE0, 0x083E7998, 0, 50,
          { 0xAC4D17B2, 0x6F375BAA, 0x0190971B, 0xF9959006, 0xA003C21A, 0x9807B468, 0x5BF43808, 0x2F1D18CA, 0xB7ABAD76 },
          { 2, 4 }, { 58 }, 0x0202E828, true, rubyHeal },
        { "FireRed", 0x0806996C, 0x0803FBFC, 0x02024284, 0x02024029, 0x9C26CCA4, 0xA73A32CD, 0x030030F0, 0x080565C8,
          0x0806E6E4, 0x822, 0x08124B04, 0x080C35F0, 0x0807A82C, 0x08069954, 0x08069960, 0x080C5290,
          0x0203B0A9, 0x083F2F50, 0x58, 108,
          { 0x07BEBC3A, 0xCD8961DF, 0x348C5B9F, 0xF9959006, 0x0111A86E, 0xBAB654BA, 0x70EA1DFC },
          { 2 }, {}, 0, false, {} },
        { "LeafGreen", 0x08069958, 0x0803FBE8, 0x02024284, 0x02024029, 0x9C26CCA4, 0x7841F205, 0x030030F0, 0x080565B4,
          0x0806E6D0, 0x822, 0x08124A64, 0x080C35B0, 0x0807A7EC, 0x08069940, 0x0806994C, 0x080C5250,
          0x0203B0A9, 0x083F2D1C, 0x58, 108,
          { 0x07BEBC3A, 0x5B68C93A, 0xE4D65E08, 0xF9959006, 0x0111A86E, 0xBAB654BA, 0x059815BF },
          { 2 }, {}, 0, false, {} },
        { "LeafGreen", 0x0806996C, 0x0803FBFC, 0x02024284, 0x02024029, 0x9C26CCA4, 0xA73A32CD, 0x030030F0, 0x080565C8,
          0x0806E6E4, 0x822, 0x08124ADC, 0x080C35C4, 0x0807A800, 0x08069954, 0x08069960, 0x080C5264,
          0x0203B0A9, 0x083F2D8C, 0x58, 108,
          { 0x07BEBC3A, 0xDBE480BD, 0x50C0C136, 0xF9959006, 0x0111A86E, 0xBAB654BA, 0x059815BF },
          { 2 }, {}, 0, false, {} },
    };
    return t;
}

enum Result { OK, NOT_NOW, NO_BADGE, CANT_FLY_HERE, NO_FLYER, FAILED };
inline const char* Why(Result r)
{
    switch (r)
    {
    case NOT_NOW: return "NOT RIGHT NOW: TRY AGAIN ON THE FIELD";
    case NO_BADGE: return "YOU NEED THE BADGE THAT LETS YOU USE FLY";
    case CANT_FLY_HERE: return "YOU CAN'T FLY FROM HERE";
    case NO_FLYER: return "NO POKEMON IN YOUR PARTY KNOWS FLY";
    case FAILED: return "FLY DIDN'T WORK HERE";
    default: return "";
    }
}

struct Targets
{
    Result result = FAILED;
    std::vector<int> towns;   // map sections
    int slot = -1;
    uint32_t personality = 0;
};

class Fly
{
public:
    // The table whose functions are all where it says, or none. Once per game.
    void Find(mCore* c)
    {
        core = c;
        fns = nullptr;
        pk_game_rest_reset();
        for (const Fns& f : Tables())
            if (Matches(f)) { fns = &f; break; }
    }
    void Forget() { fns = nullptr; core = nullptr; }
    bool Supported() const { return fns != nullptr; }
    const char* Name() const { return fns ? fns->name : ""; }

    Targets Now()
    {
        Targets t;
        if (!fns) return t;
        if (Result g = Gate(); g != OK) { t.result = g; return t; }
        if (!Flyer(t.slot, t.personality)) { t.result = NO_FLYER; return t; }
        for (int sec : Sections())
        {
            uint32_t type = 0;
            if (!Call(fns->getMapsecType, { (uint32_t)sec }, &type)) { t.result = NOT_NOW; return t; }
            if (fns->flyTypes.count((int)type)) t.towns.push_back(sec);
        }
        t.result = OK;
        return t;
    }
    // The first step: the checks again, then controls locked and the screen fading to black. Finish ~30 frames on.
    Result Start(int sec, const Targets& t)
    {
        if (!fns) return FAILED;
        if (Result g = Gate(); g != OK) return g;
        uint32_t type = 0;
        if (!Call(fns->getMapsecType, { (uint32_t)sec }, &type) || !fns->flyTypes.count((int)type)) return FAILED;
        int slot; uint32_t pers;
        if (!Flyer(slot, pers) || slot != t.slot || pers != t.personality) return NO_FLYER;
        Call(fns->lockControls, {});
        Call(fns->fadeScreen, { 1, 0 });
        return OK;
    }
    // Still the field (the main callback): nothing took the game elsewhere during the fade.
    bool OnField() const
    {
        if (!fns) return false;
        const uint32_t cb = core->busRead32(core, fns->gMain + 4);
        return (cb & ~1u) == (fns->cb2Overworld & ~1u);
    }
    // The second step: the cut-in's Pokemon under the party cursor, then the warp; the game flies. If it can't, the
    // field comes back as it was (faded in, controls free). False when the game isn't at rest this frame: ask again.
    enum Step { DONE, BACK, LATER };
    Step Finish(int sec, const Targets& t)
    {
        if (!fns) return BACK;
        if (!pk_game_call_safe(core)) return LATER;
        if (!OnField()) return BACK;   // (something else has the screen: whatever it is brings the field back)
        core->busWrite8(core, fns->partyMenuSlotId, (uint8_t)t.slot);
        if (Warp(sec)) return DONE;
        Call(fns->fadeScreen, { 0, 0 });
        Call(fns->unlockControls, {});
        return BACK;
    }
    // The towns the game's Fly map would list (visited), as maps: {group, num} from the Fly map's table. Empty when the
    // game isn't at rest this frame.
    std::vector<std::pair<int, int>> VisitedTownMaps()
    {
        std::vector<std::pair<int, int>> out;
        if (!fns || !pk_game_call_safe(core)) return out;
        for (int sec : Sections())
        {
            uint32_t type = 0;
            if (!Call(fns->getMapsecType, { (uint32_t)sec }, &type)) return {};
            const int i = sec - fns->firstMapsec;
            if (!fns->flyTypes.count((int)type) || i < 0 || i >= fns->count) continue;
            const uint32_t a = fns->destinations + (uint32_t)i * 3;
            const int g = core->busRead8(core, a), n = core->busRead8(core, a + 1);
            if (g | n) out.push_back({ g, n });
        }
        return out;
    }
    bool AtRest() const { return core && pk_game_call_safe(core); }
    void FrameEnded() { if (core && fns) pk_game_rest_note(core); }

private:
    mCore* core = nullptr;
    const Fns* fns = nullptr;

    bool Matches(const Fns& f) const
    {
        auto crc = [&](uint32_t fn) {
            uint8_t b[32];
            for (int i = 0; i < 32; i++) b[i] = (uint8_t)core->rawRead8(core, (fn & ~1u) + i, -1);
            return (uint32_t)doCrc32(b, 32);
        };
        if (crc(f.fieldControlsLocked) != f.lockedCrc || crc(f.getMonData) != f.getMonDataCrc) return false;
        std::vector<uint32_t> fn = { f.flagGet, f.setUpFieldMoveFly, f.getMapsecType, f.fadeScreen, f.lockControls, f.unlockControls };
        if (f.heal) { fn.push_back(f.hw.toHealLocation); fn.push_back(f.hw.toMapWarp); fn.push_back(f.hw.returnToField); }
        else fn.push_back(f.setFlyWarpDestination);
        if (fn.size() != f.crcs.size()) return false;
        for (size_t i = 0; i < fn.size(); i++)
            if (crc(fn[i]) != f.crcs[i]) return false;
        return true;
    }
    bool Call(uint32_t fn, std::vector<uint32_t> a, uint32_t* r0 = nullptr)
    {
        uint32_t args[4] = {};
        for (size_t i = 0; i < a.size() && i < 4; i++) args[i] = a[i];
        return pk_game_call(core, fn | 1u, args, r0) != 0;
    }
    uint32_t CallR(uint32_t fn, std::vector<uint32_t> a, bool& ok)
    {
        uint32_t r = 0;
        ok = Call(fn, a, &r) && ok;
        return r;
    }
    Result Gate()
    {
        if (!pk_game_call_safe(core)) return NOT_NOW;
        bool ok = true;
        if (CallR(fns->fieldControlsLocked, {}, ok) & 0xFF) return NOT_NOW;
        if (!OnField()) return NOT_NOW;
        if ((CallR(fns->flagGet, { (uint32_t)fns->badgeFlag }, ok) & 0xFF) != 1) return ok ? NO_BADGE : NOT_NOW;
        uint32_t may = fns->mapHeader ? CallR(fns->setUpFieldMoveFly, { core->busRead8(core, fns->mapHeader + 0x17) }, ok)
                                      : CallR(fns->setUpFieldMoveFly, {}, ok);
        if (!ok) return NOT_NOW;
        if ((may & 0xFF) != 1) return CANT_FLY_HERE;
        return OK;
    }
    // The first party Pokemon (not an egg) that knows Fly: its slot and personality.
    bool Flyer(int& slot, uint32_t& personality)
    {
        const int count = std::min(6, (int)core->busRead8(core, fns->partyCount));
        for (int s = 0; s < count; s++)
        {
            const uint32_t mon = fns->party + (uint32_t)s * 100;
            bool ok = true;
            if (CallR(fns->getMonData, { mon, 45 }, ok) || !ok) continue;   // MON_DATA_IS_EGG
            for (int k = 0; k < 4; k++)
                if (CallR(fns->getMonData, { mon, (uint32_t)(13 + k) }, ok) == 19 && ok)   // MON_DATA_MOVE1.., MOVE_FLY
                {
                    slot = s;
                    personality = CallR(fns->getMonData, { mon, 0 }, ok);
                    return ok;
                }
        }
        return false;
    }
    std::vector<int> Sections() const
    {
        std::vector<int> s;
        for (int i = 0; i < fns->count; i++)
        {
            const uint32_t a = fns->destinations + (uint32_t)i * 3;
            if (core->busRead8(core, a) | core->busRead8(core, a + 1) | core->busRead8(core, a + 2)) s.push_back(fns->firstMapsec + i);
        }
        for (int e : fns->extraSecs) s.push_back(e);
        return s;
    }
    bool Warp(int sec)
    {
        if (!fns->heal) return Call(fns->setFlyWarpDestination, { (uint32_t)sec });
        const HealWarp& h = fns->hw;
        const int i = sec - fns->firstMapsec;
        int group = 0, num = 0, heal = 0;
        if (i >= 0 && i < fns->count)
        {
            const uint32_t a = fns->destinations + (uint32_t)i * 3;
            group = core->busRead8(core, a); num = core->busRead8(core, a + 1); heal = core->busRead8(core, a + 2);
        }
        for (int k = 0; k < 2; k++) if (sec == h.specialSec[k]) heal = h.specialHeal[k];
        if (sec == h.genderSec)
        {
            const uint32_t block = h.saveBlock2Static ? h.saveBlock2 : core->busRead32(core, h.saveBlock2);
            heal = core->busRead8(core, block + 8) == 0 ? h.maleHeal : h.femaleHeal;
        }
        bool ok;
        if (heal) ok = Call(h.toHealLocation, { (uint32_t)heal });
        else ok = (group | num) && Call(h.toMapWarp, { (uint32_t)group, (uint32_t)num, 0xFFFFFFFFu });
        return ok && Call(h.returnToField, {});
    }
};
} // namespace GbaFly
