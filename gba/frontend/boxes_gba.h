// BOXES in the GBA Companion: the PC's boxes read live from the game's memory, and WITHDRAW, SWAP and DEPOSIT done the
// way the game's own PC does them, by calling its functions between frames (pk_game_call): BoxMonToMon takes a Pokemon
// out (its stats worked out, full HP), BoxMonRestorePP puts one away with its PP back. Adapted from PokeDaisy by Lidor
// Itzhari - https://github.com/lidor30/pokedaisy (BoxSwap.kt; GPL 3): retail Emerald, FireRed rev 1 and both
// LeafGreens (and Ruby and Sapphire, found the same way), each function checked by the CRC32 of its first 32 bytes in the ROM as it came.
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <mgba/core/core.h>
#include <mgba-util/crc32.h>

#include "fly_gba.h"
#include "gba_companion.h"

namespace GbaBoxes
{
struct Fns
{
    const char* name;
    uint32_t boxMonToMon, restorePP, getBoxMonData, storagePtr;
    uint32_t crcs[3];
    bool staticStorage = false;   // (Ruby and Sapphire: storagePtr is gPokemonStorage itself, not a pointer to it)
};
inline const std::vector<Fns>& Tables()
{
    static const std::vector<Fns> t = {
        { "Emerald", 0x08069004, 0x0806E930, 0x0806A674, 0x03005D94, { 0xD6140AE3, 0x04CE5AB8, 0xF3E3F4DD } },
        { "FireRed", 0x0803E788, 0x080442F8, 0x0803FD58, 0x03005010, { 0xC49105FC, 0x05F9FBC2, 0xF3E3F4DD } },
        { "LeafGreen", 0x0803E788, 0x080442F8, 0x0803FD58, 0x03005010, { 0x3F859618, 0x05F9FBC2, 0xF3E3F4DD } },
        { "LeafGreen", 0x0803E774, 0x080442E4, 0x0803FD44, 0x03005010, { 0x9435DDEC, 0x05F9FBC2, 0xF3E3F4DD } },
        // (Ruby and Sapphire rev 2, English: found by Emerald's code with its calls masked, once each; GetBoxMonData as
        // GetMonData's call; gPokemonStorage from the ROM's references to it and its boxes at +4.)
        { "Ruby", 0x0803B4B4, 0x08040B48, 0x0803CBFC, 0x020300A0, { 0x00CEE9C0, 0xD3067929, 0xFBFC6EF8 }, true },
        { "Sapphire", 0x0803B4B4, 0x08040B48, 0x0803CBFC, 0x020300A0, { 0x97E898EE, 0xD3067929, 0xFBFC6EF8 }, true },
    };
    return t;
}

constexpr int Count = 14, Slots = 30, BoxMon = 80, MonSize = 100;

struct Slot { int slot = 0; GbaCompanion::Mon mon; };
struct Box { std::string name; std::vector<Slot> mons; };

enum Result { DONE, NOT_NOW, CHANGED, HOLDS_MAIL, LAST_MON, PARTY_FULL, BOX_FULL, FAILED };
inline const char* Why(Result r)
{
    switch (r)
    {
    case NOT_NOW: return "NOT RIGHT NOW: TRY AGAIN ON THE FIELD";
    case CHANGED: return "YOUR PARTY OR BOX CHANGED: LOOK AGAIN";
    case HOLDS_MAIL: return "PLEASE REMOVE THE MAIL FIRST";
    case LAST_MON: return "THAT'S YOUR LAST POKEMON THAT CAN FIGHT";
    case PARTY_FULL: return "YOUR PARTY IS FULL: SWAP INSTEAD";
    case BOX_FULL: return "THAT BOX IS FULL";
    case FAILED: return "THAT DIDN'T WORK";
    default: return "";
    }
}

class Boxes
{
public:
    // The table whose functions are where it says in [clean] (the ROM before the widescreen patch), with FLY's for the
    // party side (GetMonData, the party, the field's lock: the same build).
    void Find(mCore* c, const std::vector<uint8_t>& clean, const GbaFly::Fns* field)
    {
        core = c;
        fns = nullptr;
        party = field;
        if (!field) return;
        for (const Fns& f : Tables())
        {
            bool ok = true;
            const uint32_t fn[3] = { f.boxMonToMon, f.restorePP, f.getBoxMonData };
            for (int i = 0; i < 3 && ok; i++)
            {
                const size_t o = (fn[i] & ~1u) - 0x08000000u;
                ok = o + 32 <= clean.size() && (uint32_t)doCrc32(clean.data() + o, 32) == f.crcs[i];
            }
            if (ok) { fns = &f; return; }
        }
    }
    void Forget() { fns = nullptr; party = nullptr; core = nullptr; }
    bool Supported() const { return fns && party; }

    // The storage (gPokemonStoragePtr), or 0 off the field.
    uint32_t Storage() const
    {
        if (!fns) return 0;
        if (fns->staticStorage) return fns->storagePtr;
        const uint32_t s = core->busRead32(core, fns->storagePtr);
        return s >= 0x02000000u && s < 0x02040000u ? s : 0;
    }
    int CurrentBox() const { const uint32_t s = Storage(); return s ? core->busRead8(core, s) % Count : 0; }
    // Every box: its name and its Pokemon (each decoded from its 80 bytes, the party's fields left out).
    std::vector<Box> Read() const
    {
        std::vector<Box> out;
        const uint32_t s = Storage();
        if (!s) return out;
        const uint32_t names = s + 4 + Count * Slots * BoxMon;
        for (int b = 0; b < Count; b++)
        {
            Box box;
            uint8_t n[9];
            for (int i = 0; i < 9; i++) n[i] = core->busRead8(core, names + (uint32_t)b * 9 + (uint32_t)i);
            box.name.assign((const char*)n, 9);
            for (int k = 0; k < Slots; k++)
            {
                uint8_t m[MonSize] = {};
                const uint32_t a = s + 4 + (uint32_t)(b * Slots + k) * BoxMon;
                for (int i = 0; i < BoxMon; i++) m[i] = core->busRead8(core, a + (uint32_t)i);
                Slot sl;
                sl.slot = k;
                if (GbaCompanion::Decode(m, sl.mon) && sl.mon.species) box.mons.push_back(sl);
            }
            out.push_back(box);
        }
        return out;
    }

    // SWAP: box [box] slot [slot] (holding [boxPers]) and party slot [ps] (holding [partyPers]) trade places.
    Result Swap(int box, int slot, uint32_t boxPers, int ps, uint32_t partyPers)
    {
        if (Result g = Gate(); g != DONE) return g;
        const uint32_t bm = BoxAddr(box, slot);
        if (!bm) return FAILED;
        const int count = PartyCount();
        bool ok = true;
        if (ps < 0 || ps >= count || Mon(ps, 0, ok) != partyPers || Boxed(bm, 11, ok) == 0 || Boxed(bm, 0, ok) != boxPers || !ok) return ok ? CHANGED : FAILED;
        if (HoldsMail(ps)) return HOLDS_MAIL;
        if (Boxed(bm, 45, ok) && !FighterLeft(ps)) return LAST_MON;
        std::vector<uint8_t> partyBefore = Bytes(PartyAddr(ps), MonSize), boxBefore = Bytes(bm, BoxMon);
        auto back = [&]() { Put(PartyAddr(ps), partyBefore); Put(bm, boxBefore); return FAILED; };
        if (!Call(fns->boxMonToMon, { bm, PartyAddr(ps) })) return back();
        Put(bm, std::vector<uint8_t>(partyBefore.begin(), partyBefore.begin() + BoxMon));
        if (!Call(fns->restorePP, { bm })) return back();
        if (Mon(ps, 0, ok) != boxPers || Boxed(bm, 0, ok) != partyPers || !ok) return back();
        return DONE;
    }
    // WITHDRAW: box [box] slot [slot] into the party's first free slot.
    Result Withdraw(int box, int slot, uint32_t boxPers)
    {
        if (Result g = Gate(); g != DONE) return g;
        const uint32_t bm = BoxAddr(box, slot);
        if (!bm) return FAILED;
        const int count = PartyCount();
        bool ok = true;
        if (Boxed(bm, 11, ok) == 0 || Boxed(bm, 0, ok) != boxPers || !ok) return ok ? CHANGED : FAILED;
        if (count >= 6) return PARTY_FULL;
        std::vector<uint8_t> partyBefore = Bytes(PartyAddr(count), MonSize), boxBefore = Bytes(bm, BoxMon);
        auto back = [&]() { Put(PartyAddr(count), partyBefore); Put(bm, boxBefore); core->busWrite8(core, party->partyCount, (uint8_t)count); return FAILED; };
        if (!Call(fns->boxMonToMon, { bm, PartyAddr(count) })) return back();
        if (Mon(count, 0, ok) != boxPers || !ok) return back();
        Put(bm, std::vector<uint8_t>(BoxMon, 0));
        core->busWrite8(core, party->partyCount, (uint8_t)(count + 1));
        if (PartyCount() != count + 1 || Boxed(bm, 11, ok) != 0 || !ok) return back();
        return DONE;
    }
    // DEPOSIT: party slot [ps] (holding [partyPers]) into box [box]'s first free slot, the party moved up.
    Result Deposit(int ps, uint32_t partyPers, int box)
    {
        if (Result g = Gate(); g != DONE) return g;
        const int count = PartyCount();
        bool ok = true;
        if (ps < 0 || ps >= count || Mon(ps, 0, ok) != partyPers || !ok) return ok ? CHANGED : FAILED;
        if (count <= 1 || !FighterLeft(ps)) return LAST_MON;
        if (HoldsMail(ps)) return HOLDS_MAIL;
        int slot = -1;
        for (int k = 0; k < Slots && slot < 0; k++) { const uint32_t a = BoxAddr(box, k); if (a && Boxed(a, 11, ok) == 0 && ok) slot = k; }
        if (!ok) return FAILED;
        if (slot < 0) return BOX_FULL;
        const uint32_t bm = BoxAddr(box, slot);
        std::vector<uint8_t> partyBefore = Bytes(party->party, (uint32_t)count * MonSize), boxBefore = Bytes(bm, BoxMon);
        auto back = [&]() { Put(party->party, partyBefore); Put(bm, boxBefore); core->busWrite8(core, party->partyCount, (uint8_t)count); return FAILED; };
        std::vector<uint8_t> leaving(partyBefore.begin() + ps * MonSize, partyBefore.begin() + ps * MonSize + BoxMon);
        std::vector<uint8_t> compact(partyBefore.begin(), partyBefore.begin() + ps * MonSize);
        compact.insert(compact.end(), partyBefore.begin() + (ps + 1) * MonSize, partyBefore.end());
        compact.resize((size_t)count * MonSize, 0);
        Put(bm, leaving);
        if (!Call(fns->restorePP, { bm }) || Boxed(bm, 0, ok) != partyPers || !ok) return back();
        Put(party->party, compact);
        core->busWrite8(core, party->partyCount, (uint8_t)(count - 1));
        if (PartyCount() != count - 1) return back();
        return DONE;
    }

private:
    mCore* core = nullptr;
    const Fns* fns = nullptr;
    const GbaFly::Fns* party = nullptr;

    uint32_t BoxAddr(int box, int slot) const
    {
        const uint32_t s = Storage();
        if (!s || box < 0 || box >= Count || slot < 0 || slot >= Slots) return 0;
        return s + 4 + (uint32_t)(box * Slots + slot) * BoxMon;
    }
    uint32_t PartyAddr(int i) const { return party->party + (uint32_t)i * MonSize; }
    int PartyCount() const { return std::min(6, (int)core->busRead8(core, party->partyCount)); }
    std::vector<uint8_t> Bytes(uint32_t a, uint32_t n) const
    {
        std::vector<uint8_t> b(n);
        for (uint32_t i = 0; i < n; i++) b[i] = (uint8_t)core->busRead8(core, a + i);
        return b;
    }
    void Put(uint32_t a, const std::vector<uint8_t>& b) { for (size_t i = 0; i < b.size(); i++) core->busWrite8(core, a + (uint32_t)i, b[i]); }
    bool Call(uint32_t fn, std::vector<uint32_t> a, uint32_t* r0 = nullptr)
    {
        uint32_t args[4] = {};
        for (size_t i = 0; i < a.size() && i < 4; i++) args[i] = a[i];
        return pk_game_call(core, fn | 1u, args, r0) != 0;
    }
    uint32_t Mon(int i, int field, bool& ok) { uint32_t r = 0; ok = Call(party->getMonData, { PartyAddr(i), (uint32_t)field }, &r) && ok; return r; }
    uint32_t Boxed(uint32_t a, int field, bool& ok) { uint32_t r = 0; ok = Call(fns->getBoxMonData, { a, (uint32_t)field }, &r) && ok; return r; }
    bool HoldsMail(int i) { bool ok = true; const uint32_t item = Mon(i, 12, ok); return item >= 121 && item <= 132; }
    // Whether the party keeps a Pokemon that can fight without slot [without] (the PC's "That's your last POKEMON!").
    bool FighterLeft(int without)
    {
        const int count = PartyCount();
        for (int i = 0; i < count; i++)
        {
            if (i == without) continue;
            bool ok = true;
            const uint32_t hp = core->busRead16(core, PartyAddr(i) + 0x56);
            if (Mon(i, 11, ok) && !Mon(i, 45, ok) && hp > 0 && ok) return true;
        }
        return false;
    }
    // (The PC's own rule: on the field with the controls free, nothing of the game's own in progress.)
    Result Gate()
    {
        if (!Supported() || !pk_game_call_safe(core)) return NOT_NOW;
        uint32_t locked = 0;
        const uint32_t cb = core->busRead32(core, party->gMain + 4);
        if ((cb & ~1u) != (party->cb2Overworld & ~1u)) return NOT_NOW;   // (a battle, a menu screen)
        if (!Call(party->fieldControlsLocked, {}, &locked)) return NOT_NOW;
        return (locked & 0xFF) ? NOT_NOW : DONE;
    }
};
} // namespace GbaBoxes
