#pragma once
// FLY from the companion, done as PokeDaisy does it (PokeDaisyActivity.kt's flyAction, companion/data/
// Gen4Reader.kt and Gen5Field.kt): the game's own FLY check (pokedaisy/pk_nds_call.cpp's FLY_CHECK, the
// party menu's), then the towns the game would offer, then the game's own flight (FLY). Platinum asks the
// game which towns are open; HeartGold / SoulSilver and Gen 5 read the save's flags (and HeartGold /
// SoulSilver offer only the region the player is in, as the Pokegear does).
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy (GNU GPL version 3).
#include <algorithm>
#include <string>
#include <vector>

#include "companion_data.h"
#include "pokedaisy/pk_fly_tables_gen.h"

namespace PokeFly
{
// MOVE_FLY: the same id in Gen 4 and Gen 5.
inline constexpr int kMoveFly = 19;

enum class Result { OK, CANT_FLY_HERE, NO_BADGE, PARTNER, NOT_NOW, NO_FLYER, NO_TOWNS, FAILED };

struct Game
{
    const Spot* spots = nullptr;
    int count = 0;
    bool gen5 = false, hgss = false;
    int flagsOff = -1;   // (from the save's body; -1: the game is asked which towns are open)
    int numFlags = 0;
    bool Supported() const { return count > 0; }
};

inline Game For(const std::string& code)
{
    const std::string f = code.size() >= 3 ? code.substr(0, 3) : code;
    auto make = [](const Spot* s, int n, bool gen5, bool hgss, int off, int flags) { Game g; g.spots = s; g.count = n; g.gen5 = gen5; g.hgss = hgss; g.flagsOff = off; g.numFlags = flags; return g; };
    // (HeartGold / SoulSilver: SAVE_FLAGS' chunk 0xDE4, after its 0x170 vars. Gen 5: the event work's
    // vars, then its flags: Black / White 0x20100, 0x13E vars, 0xB60 flags; Black 2 / White 2 0x1FF00,
    // 0x1AF vars, 0xBF8 flags.)
    if (f == "CPU") return make(kPlatinum, (int)std::size(kPlatinum), false, false, -1, 0);
    if (f == "IPK" || f == "IPG") return make(kHeartGold, (int)std::size(kHeartGold), false, true, 0xDE4 + 0x170 * 2, 0x200 * 8);
    if (f == "IRB") return make(kBlack, (int)std::size(kBlack), true, false, 0x20100 + 0x13E * 2, 0xB60);
    if (f == "IRA") return make(kWhite, (int)std::size(kWhite), true, false, 0x20100 + 0x13E * 2, 0xB60);
    if (f == "IRE") return make(kBlack2, (int)std::size(kBlack2), true, false, 0x1FF00 + 0x1AF * 2, 0xBF8);
    if (f == "IRD") return make(kWhite2, (int)std::size(kWhite2), true, false, 0x1FF00 + 0x1AF * 2, 0xBF8);
    return {};
}

// The first party Pokemon (not an egg) that knows FLY, or nullptr.
inline const Companion::Mon* Flyer(const Companion::Snapshot& s)
{
    for (const auto& m : s.party)
        if (!m.egg && m.slot >= 0 && std::find(m.moves.begin(), m.moves.end(), kMoveFly) != m.moves.end()) return &m;
    return nullptr;
}

// FLY_CHECK's result 1 as FLY's answer. Gen 4: FieldMoves_CheckFly's FIELD_MOVE_ERROR_* (1 indoors, a cave,
// a link room, the Safari Zone; 2 no badge; 3 a partner with you). Gen 5: the skill's own test (1 not here).
inline Result CheckResult(const Game& g, uint32_t error)
{
    if (error == 0) return Result::OK;
    if (error == 1) return Result::CANT_FLY_HERE;
    if (!g.gen5 && error == 2) return Result::NO_BADGE;
    if (!g.gen5 && error == 3) return Result::PARTNER;
    return Result::NOT_NOW;
}

// Pokegear_RegionFromCoords: 0 Johto, 1 Kanto, 2 Indigo Plateau / Victory Road (either).
inline int PokegearRegion(int x, int y)
{
    if (x <= 21) return 0;
    if (x == 25 && y == 8) return 0; // (Mt. Silver)
    if ((x == 28 && y == 6) || (x == 28 && y >= 9 && y <= 12)) return 2;
    return 1;
}

// HeartGold / SoulSilver: the region the player is in, on the Pokegear map's blocks: on the main map
// matrix their own block (their tile x / z, 32 tiles a block); elsewhere the map's world-map block, else
// the save's special spawn's (where they last went in). 2 (either) when it is not known.
inline int HgRegion(const Companion::Snapshot& s)
{
    if (std::find(std::begin(kHgMainMatrix), std::end(kHgMainMatrix), s.map) != std::end(kHgMainMatrix))
        return PokegearRegion(s.x / 32, s.y / 32);
    for (const auto& w : kHgWorldMap)
        if (w[0] == s.map) return PokegearRegion(w[1], w[2]);
    if (s.exitMap >= 0 && s.exitX >= 0 && s.exitY >= 0) return PokegearRegion(s.exitX / 32, s.exitY / 32);
    return 2;
}

// The towns FLY offers now (indices into the game's spots), in the game's own order. [results]: FLY_CHECK's
// (Platinum's open towns are its results 2 ..), else the save's flags are read through [read].
inline std::vector<int> OpenSpots(const Game& g, const Companion::Snapshot& s, const Companion::Reader& read, const uint32_t* results)
{
    std::vector<int> open;
    if (g.flagsOff < 0)
    {
        for (int i = 0; i < g.count && 2 + i < 32; i++)
            if (results[2 + i]) open.push_back(i);
        return open;
    }
    if (!s.saveBase) return open;
    std::vector<uint8_t> flags(g.numFlags / 8);
    if (!read(s.saveBase + g.flagsOff, flags.data(), flags.size())) return open;
    const int here = g.hgss ? HgRegion(s) : 2;
    for (int i = 0; i < g.count; i++)
    {
        const Spot& p = g.spots[i];
        if (p.flag < 0 || p.flag >= g.numFlags || !((flags[p.flag / 8] >> (p.flag % 8)) & 1)) continue;
        if (g.hgss && p.region != 2 && here != 2 && p.region != here) continue;
        open.push_back(i);
    }
    return open;
}

// FLY's job arguments for spot [i]: Gen 4 the party slot, the town's map header and the tile flown to;
// Gen 5 the slot and the town's zone.
inline void Args(const Game& g, int i, int slot, uint32_t out[4])
{
    const Spot& p = g.spots[i];
    out[0] = (uint32_t)slot;
    out[1] = (uint32_t)p.header;
    out[2] = g.gen5 ? 0u : (uint32_t)p.x;
    out[3] = g.gen5 ? 0u : (uint32_t)p.z;
}

// The map a spot is named by (Gen 4: its own name map; Gen 5: its zone).
inline int NameMap(const Game& g, int i) { return g.gen5 ? g.spots[i].header : g.spots[i].nameMap; }

inline const char* Message(Result r)
{
    switch (r)
    {
    case Result::OK: return "FLYING";
    case Result::CANT_FLY_HERE: return "YOU CAN'T FLY FROM HERE";
    case Result::NO_BADGE: return "YOU NEED THE BADGE THAT ALLOWS FLY";
    case Result::PARTNER: return "YOU CAN'T FLY WHILE SOMEONE IS WITH YOU";
    case Result::NO_FLYER: return "NO POKEMON IN YOUR PARTY KNOWS FLY";
    case Result::NO_TOWNS: return "NO TOWNS TO FLY TO YET";
    case Result::NOT_NOW: return "FLY UNAVAILABLE - CLOSE MENUS OR LEAVE THIS SCENE";
    default: return "FLY FAILED";
    }
}
}
