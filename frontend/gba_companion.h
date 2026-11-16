#pragma once
// COMPANION for the GBA games: what the game holds now (the party, the bag, where you are, the
// Pokedex's seen and caught), read from the running game's memory while it is paused. The
// addresses for each version and language are PokeDaisy's (NativeConfigsGen.kt, from the pret
// decompilations' maps); the party structures' decoding is Gen3Mon.kt's (GPL-3.0).
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace GbaCompanion
{
struct Config
{
    const char* code; int revision;
    uint32_t party, partyCount, saveBlock1, saveBlock2;
    bool staticSaveBlocks;  // (Ruby and Sapphire: the addresses are the blocks, not pointers to them)
    uint32_t bagPockets; int pocketCount;
    int keyOff;             // (SaveBlock2's encryption key; -1: none, Ruby and Sapphire)
    char pocketOrder;       // (E: Items, Poke Balls, TMs and HMs, Berries, Key Items; F: Items, Key Items, Poke Balls, TM Case, Berry Pouch)
    uint32_t battleMons, battlerPositions, gMain, inBattleOff; // (gBattleMons, which battler is where, gMain and its inBattle byte)
};
inline const std::vector<Config>& Configs()
{
    static const std::vector<Config> c = {
    { "AXPD", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083CD3DC, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPD", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083CD3DC, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPE", 0, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1670, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPE", 1, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1690, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPE", 2, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1690, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPF", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C8A6C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPF", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C8A6C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPI", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C1D40, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPI", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C1D40, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPJ", 0, 0x03004290, 0x03004280, 0x02025494, 0x02024C04, true, 0x08397A50, 5, -1, 'E', 0x020247E0, 0x020247D2, 0x030016E0, 0x439 },
    { "AXPJ", 1, 0x03004290, 0x03004280, 0x02025494, 0x02024C04, true, 0x08397A50, 5, -1, 'E', 0x020247E0, 0x020247D2, 0x030016E0, 0x439 },
    { "AXPS", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C4D48, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPS", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C4D48, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVD", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083CD470, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVD", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083CD470, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVE", 0, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1618, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVE", 1, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1634, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVE", 2, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1634, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVF", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C8F3C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVF", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C8F3C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVI", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C209C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVI", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C209C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVJ", 0, 0x03004290, 0x03004280, 0x02025494, 0x02024C04, true, 0x08397A6C, 5, -1, 'E', 0x020247E0, 0x020247D2, 0x030016E0, 0x439 },
    { "AXVJ", 1, 0x03004290, 0x03004280, 0x02025494, 0x02024C04, true, 0x08397A6C, 5, -1, 'E', 0x020247E0, 0x020247D2, 0x030016E0, 0x439 },
    { "AXVS", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C500C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVS", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C500C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "BPED", 0, 0x020244EC, 0x020244E9, 0x03005D8C, 0x03005D90, false, 0x02039DD8, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030022C0, 0x439 },
    { "BPEE", 0, 0x020244EC, 0x020244E9, 0x03005D8C, 0x03005D90, false, 0x02039DD8, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030022C0, 0x439 },
    { "BPEF", 0, 0x020244EC, 0x020244E9, 0x03005D8C, 0x03005D90, false, 0x02039DD8, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030022C0, 0x439 },
    { "BPEI", 0, 0x020244EC, 0x020244E9, 0x03005D8C, 0x03005D90, false, 0x02039DD8, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030022C0, 0x439 },
    { "BPEJ", 0, 0x02024190, 0x0202418D, 0x03005AEC, 0x03005AF0, false, 0x02039A78, 5, 0xAC, 'E', 0x02023D28, 0x02023D1A, 0x03002360, 0x439 },
    { "BPES", 0, 0x020244EC, 0x020244E9, 0x03005D8C, 0x03005D90, false, 0x02039DD8, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030022C0, 0x439 },
    { "BPGD", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPGE", 0, 0x02024284, 0x02024029, 0x03005008, 0x0300500C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x030030F0, 0x439 },
    { "BPGE", 1, 0x02024284, 0x02024029, 0x03005008, 0x0300500C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x030030F0, 0x439 },
    { "BPGF", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPGI", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPGJ", 0, 0x020241E4, 0x02023F89, 0x03005048, 0x0300504C, false, 0x020397D8, 5, 0xF20, 'F', 0x02023B44, 0x02023B36, 0x03003130, 0x439 },
    { "BPGJ", 1, 0x020241E4, 0x02023F89, 0x03004FA8, 0x03004FAC, false, 0x020397D8, 5, 0xF20, 'F', 0x02023B44, 0x02023B36, 0x03003090, 0x439 },
    { "BPGS", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPRD", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPRE", 0, 0x02024284, 0x02024029, 0x03005008, 0x0300500C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x030030F0, 0x439 },
    { "BPRE", 1, 0x02024284, 0x02024029, 0x03005008, 0x0300500C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x030030F0, 0x439 },
    { "BPRF", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPRI", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPRJ", 0, 0x020241E4, 0x02023F89, 0x03005048, 0x0300504C, false, 0x020397D8, 5, 0xF20, 'F', 0x02023B44, 0x02023B36, 0x03003130, 0x439 },
    { "BPRJ", 1, 0x020241E4, 0x02023F89, 0x03004FA8, 0x03004FAC, false, 0x020397D8, 5, 0xF20, 'F', 0x02023B44, 0x02023B36, 0x03003090, 0x439 },
    { "BPRS", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    };
    return c;
}
// The game's own version first, then any revision of its code (a hack keeps its base game's).
inline const Config* ConfigFor(const char* code, int revision)
{
    const Config* any = nullptr;
    for (const Config& c : Configs())
        if (!strncmp(c.code, code, 4)) { if (c.revision == revision) return &c; if (!any) any = &c; }
    return any;
}

struct Mon
{
    int species = 0, item = 0, level = 0, hp = 0, maxHp = 0, friendship = 0;
    uint32_t personality = 0, otId = 0, status = 0, exp = 0;
    bool egg = false, abilityBit = false;
    std::string nickname;
    int moves[4] = {}, pp[4] = {}, ppBonuses = 0;
    int iv[6] = {}, ev[6] = {}, stats[6] = {}; // (HP, Attack, Defence, Speed, Sp. Atk, Sp. Def)
};
struct Item { int id = 0, quantity = 0; };
// One battler as the battle keeps it (struct BattlePokemon, 0x58 bytes).
struct Battler
{
    int species = 0, level = 0, hp = 0, maxHp = 0, type1 = 0, type2 = 0, ability = 0, item = 0;
    uint32_t status = 0, personality = 0;
    int moves[4] = {}, pp[4] = {}, stats[5] = {}; // (Attack, Defence, Speed, Sp. Atk, Sp. Def)
    int iv[6] = {}; // (HP, Attack, Defence, Speed, Sp. Atk, Sp. Def, as the party's)
    std::string nickname;
};
struct Snapshot
{
    bool ok = false;
    std::vector<Mon> party;
    int mapGroup = -1, mapNum = -1;
    long long money = -1;
    std::vector<std::pair<const char*, std::vector<Item>>> pockets;
    std::vector<uint8_t> seen, caught; // (by National Pokedex number)
    bool inBattle = false;
    Battler mine, foe; // (in a battle: your Pokemon out and the foe's, the left ones in a double)
    std::vector<Mon> foeParty; // (in a battle: gEnemyParty in order, the trainer's whole team)
};

inline uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
inline uint32_t U32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

// One party Pokemon (100 bytes): its 48 bytes of growth, attacks, EVs and misc, each 12, XORed
// with personality ^ OT ID and in the order personality % 24 gives; checked by their checksum.
inline bool Decode(const uint8_t* m, Mon& out)
{
    static const uint8_t order[24][4] = {
        { 0, 1, 2, 3 }, { 0, 1, 3, 2 }, { 0, 2, 1, 3 }, { 0, 2, 3, 1 }, { 0, 3, 1, 2 }, { 0, 3, 2, 1 },
        { 1, 0, 2, 3 }, { 1, 0, 3, 2 }, { 1, 2, 0, 3 }, { 1, 2, 3, 0 }, { 1, 3, 0, 2 }, { 1, 3, 2, 0 },
        { 2, 0, 1, 3 }, { 2, 0, 3, 1 }, { 2, 1, 0, 3 }, { 2, 1, 3, 0 }, { 2, 3, 0, 1 }, { 2, 3, 1, 0 },
        { 3, 0, 1, 2 }, { 3, 0, 2, 1 }, { 3, 1, 0, 2 }, { 3, 1, 2, 0 }, { 3, 2, 0, 1 }, { 3, 2, 1, 0 } };
    const uint32_t pid = U32(m), ot = U32(m + 4), key = pid ^ ot;
    if (!pid && !ot) return false;
    uint8_t d[48];
    for (int i = 0; i < 12; i++)
    {
        const uint32_t v = U32(m + 32 + i * 4) ^ key;
        memcpy(d + i * 4, &v, 4);
    }
    uint16_t sum = 0;
    for (int i = 0; i < 24; i++) sum = (uint16_t)(sum + U16(d + i * 2));
    if (sum != U16(m + 28)) return false;
    const uint8_t* sub[4];
    for (int slot = 0; slot < 4; slot++) sub[order[pid % 24][slot]] = d + slot * 12;
    const uint8_t *g = sub[0], *a = sub[1], *e = sub[2], *x = sub[3];
    out = Mon();
    out.personality = pid; out.otId = ot;
    out.species = U16(g); out.item = U16(g + 2); out.exp = U32(g + 4); out.ppBonuses = g[8]; out.friendship = g[9];
    for (int i = 0; i < 4; i++) { out.moves[i] = U16(a + i * 2); out.pp[i] = a[8 + i]; }
    for (int i = 0; i < 6; i++) out.ev[i] = e[i];
    const uint32_t ivs = U32(x + 4);
    for (int i = 0; i < 6; i++) out.iv[i] = ivs >> (i * 5) & 31;
    out.egg = ivs >> 30 & 1; out.abilityBit = ivs >> 31 & 1;
    out.status = U32(m + 80); out.level = m[84]; out.hp = U16(m + 86); out.maxHp = U16(m + 88);
    out.stats[0] = out.maxHp;
    for (int i = 1; i < 6; i++) out.stats[i] = U16(m + 88 + i * 2);
    out.nickname.assign((const char*)m + 8, 10);
    return out.species > 0 && out.level <= 100;
}

// The game as it is now. read(address, n, buffer): its memory, false where it can't.
template <class Read>
Snapshot Take(const Config& c, Read read)
{
    Snapshot s;
    uint8_t b[600];
    auto u32At = [&](uint32_t at) { uint8_t v[4] = {}; return read(at, 4, v) ? U32(v) : 0u; };
    uint8_t count = 0;
    if (!read(c.partyCount, 1, &count) || count > 6) count = 0;
    if (count && read(c.party, count * 100u, b))
        for (int i = 0; i < count; i++) { Mon m; if (Decode(b + i * 100, m)) s.party.push_back(m); }
    const uint32_t sb1 = c.staticSaveBlocks ? c.saveBlock1 : u32At(c.saveBlock1);
    const uint32_t sb2 = c.staticSaveBlocks ? c.saveBlock2 : u32At(c.saveBlock2);
    const bool blocks = sb1 >= 0x02000000u && sb1 < 0x02040000u && sb2 >= 0x02000000u && sb2 < 0x02040000u;
    if (blocks)
    {
        uint8_t loc[8];
        if (read(sb1, 8, loc)) { s.mapGroup = loc[4]; s.mapNum = loc[5]; }
        const uint32_t key = c.keyOff >= 0 ? u32At(sb2 + c.keyOff) : 0;
        // (Money: Ruby and Sapphire's at SaveBlock1 + 0x490, Emerald's too; FireRed and LeafGreen's at 0x290.)
        s.money = (long long)(u32At(sb1 + (c.pocketOrder == 'F' ? 0x290 : 0x490)) ^ key);
        // (The Pokedex in SaveBlock2 + 0x18: owned at + 0x10, seen at + 0x44, 52 bytes each.)
        uint8_t dex[104];
        if (read(sb2 + 0x28, 104, dex))
        {
            s.caught.assign(413, 0); s.seen.assign(413, 0);
            for (int n = 1; n <= 412; n++)
            {
                s.caught[n] = dex[(n - 1) / 8] >> ((n - 1) % 8) & 1;
                s.seen[n] = dex[52 + (n - 1) / 8] >> ((n - 1) % 8) & 1;
            }
        }
        // (The bag: gBagPockets, each {its slots, how many}: slots {item, quantity ^ key's low half}.)
        static const char* const emerald[5] = { "ITEMS", "POKE BALLS", "TMS AND HMS", "BERRIES", "KEY ITEMS" };
        static const char* const firered[5] = { "ITEMS", "KEY ITEMS", "POKE BALLS", "TM CASE", "BERRY POUCH" };
        uint8_t pockets[5 * 8];
        if (c.pocketCount == 5 && read(c.bagPockets, sizeof pockets, pockets))
            for (int p = 0; p < 5; p++)
            {
                const uint32_t slots = U32(pockets + p * 8);
                const int capacity = std::min<int>(pockets[p * 8 + 4], 64);
                std::vector<Item> items;
                uint8_t raw[64 * 4];
                if (slots >= 0x02000000u && slots < 0x02040000u && capacity && read(slots, capacity * 4u, raw))
                    for (int i = 0; i < capacity; i++)
                        if (U16(raw + i * 4)) items.push_back({ U16(raw + i * 4), U16(raw + i * 4 + 2) ^ (int)(key & 0xFFFF) });
                s.pockets.push_back({ c.pocketOrder == 'F' ? firered[p] : emerald[p], items });
            }
    }
    // (A battle: gMain's inBattle bit; the battlers by position, 0 yours and 1 the foe's on the left.)
    uint8_t flag = 0;
    if (c.gMain && read(c.gMain + c.inBattleOff, 1, &flag) && (flag & 2))
    {
        uint8_t pos[4] = {}, mons[4 * 0x58];
        if (read(c.battlerPositions, 4, pos) && read(c.battleMons, sizeof mons, mons))
            for (int b = 0; b < 4; b++)
            {
                if (pos[b] > 1) continue;
                const uint8_t* m = mons + b * 0x58;
                Battler& t = pos[b] == 0 ? s.mine : s.foe;
                t.species = U16(m); t.level = m[0x2A]; t.hp = U16(m + 0x28); t.maxHp = U16(m + 0x2C);
                t.type1 = m[0x21]; t.type2 = m[0x22]; t.ability = m[0x20]; t.item = U16(m + 0x2E);
                t.status = U32(m + 0x4C); t.personality = U32(m + 0x48);
                for (int i = 0; i < 4; i++) { t.moves[i] = U16(m + 0x0C + i * 2); t.pp[i] = m[0x24 + i]; }
                for (int i = 0; i < 5; i++) t.stats[i] = U16(m + 2 + i * 2);
                const uint32_t ivs = U32(m + 0x14);
                for (int i = 0; i < 6; i++) t.iv[i] = ivs >> (i * 5) & 31;
                t.nickname.assign((const char*)m + 0x30, 10);
            }
        s.inBattle = s.foe.species > 0;
        // (gEnemyParty: beside gPlayerParty, after it in Ruby, Sapphire and Emerald and before it in FireRed and
        // LeafGreen, 600 or 608 bytes away; the block holding the foe out is the one.)
        if (s.inBattle)
            for (int64_t d : { 600, -600, 608 })
            {
                const uint32_t at = (uint32_t)((int64_t)c.party + d);
                uint8_t e[600];
                if (!read(at, 600, e)) continue;
                std::vector<Mon> team;
                bool hasFoe = false;
                for (int i = 0; i < 6; i++)
                {
                    Mon m;
                    if (!Decode(e + i * 100, m) || !m.species) break;
                    hasFoe = hasFoe || m.personality == s.foe.personality;
                    team.push_back(m);
                }
                if (hasFoe) { s.foeParty = team; break; }
            }
    }
    s.ok = !s.party.empty() || blocks;
    return s;
}
}
