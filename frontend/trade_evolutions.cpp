// TRADE EVOLUTIONS WITHOUT TRADING (trade_evolutions.h).
#include "trade_evolutions.h"

#include <cstdio>
#include <cstring>

namespace TradeEvolutions
{
namespace
{
uint16_t R16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t R32(const uint8_t* p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)); }
void W16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

// ---------------------------------------------------------------------------------------------
// The DS cartridge's file system: a file by its path.

bool FindFile(const uint8_t* rom, size_t size, const char* path, uint32_t& start, uint32_t& end)
{
    if (size < 0x200) return false;
    const uint32_t fnt = R32(rom + 0x40), fntSize = R32(rom + 0x44), fat = R32(rom + 0x48), fatSize = R32(rom + 0x4C);
    if ((uint64_t)fnt + fntSize > size || (uint64_t)fat + fatSize > size || fntSize < 8) return false;
    uint16_t dir = 0xF000;
    const char* p = path;
    for (;;)
    {
        const char* slash = strchr(p, '/');
        const size_t len = slash ? (size_t)(slash - p) : strlen(p);
        const uint32_t d = (dir & 0xFFF) * 8;
        if (d + 8 > fntSize) return false;
        uint32_t at = fnt + R32(rom + fnt + d);
        uint16_t fileId = R16(rom + fnt + d + 4);
        bool found = false;
        while (at < fnt + fntSize)
        {
            const uint8_t head = rom[at++];
            if (head == 0) break;
            const size_t nameLen = head & 0x7F;
            const bool isDir = (head & 0x80) != 0;
            if (at + nameLen > fnt + fntSize) return false;
            const bool same = nameLen == len && memcmp(rom + at, p, len) == 0;
            at += nameLen;
            if (isDir)
            {
                const uint16_t sub = R16(rom + at);
                at += 2;
                if (same && slash) { dir = sub; found = true; break; }
            }
            else
            {
                if (same && !slash)
                {
                    if ((uint32_t)fileId * 8 + 8 > fatSize) return false;
                    start = R32(rom + fat + fileId * 8);
                    end = R32(rom + fat + fileId * 8 + 4);
                    return start <= end && end <= size;
                }
                fileId++;
            }
        }
        if (!found) return false;
        p = slash + 1;
    }
}

// A NARC archive's members: their place in the ROM.
struct Narc
{
    std::vector<std::pair<uint32_t, uint32_t>> members; // start, end (in the ROM)
};

bool ReadNarc(const uint8_t* rom, uint32_t start, uint32_t end, Narc& narc)
{
    if (end - start < 0x1C || memcmp(rom + start, "NARC", 4) != 0) return false;
    const uint32_t btaf = start + R16(rom + start + 0x0C);
    if (memcmp(rom + btaf, "BTAF", 4) != 0) return false;
    const uint32_t btafSize = R32(rom + btaf + 4), count = R16(rom + btaf + 8);
    const uint32_t btnf = btaf + btafSize;
    if (btnf + 8 > end || memcmp(rom + btnf, "BTNF", 4) != 0) return false;
    const uint32_t gmif = btnf + R32(rom + btnf + 4);
    if (gmif + 8 > end || memcmp(rom + gmif, "GMIF", 4) != 0) return false;
    const uint32_t data = gmif + 8;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint32_t s = data + R32(rom + btaf + 12 + i * 8), e = data + R32(rom + btaf + 16 + i * 8);
        if (s > e || e > end) return false;
        narc.members.push_back({ s, e });
    }
    return true;
}

// Evolution methods, as numbered in Gens 4 and 5 (Gen 5 put Karrablast/Shelmet at 7, moving the rest up).
struct Methods { uint16_t level, stone, trade, tradeItem, tradeSpecial, itemDay, itemNight, withOther; };
const Methods kGen4 = { 4, 7, 5, 6, 0, 18, 19, 0 };
const Methods kGen5 = { 4, 8, 5, 6, 7, 19, 20, 22 };
const uint16_t kWaterStone = 84, kSlowpoke = 79, kKarrablast = 588, kShelmet = 616;

const char* SpeciesLabel(int s)
{
    switch (s)
    {
        case 64: return "KADABRA"; case 67: return "MACHOKE"; case 75: return "GRAVELER"; case 93: return "HAUNTER";
        case 61: return "POLIWHIRL"; case 79: return "SLOWPOKE"; case 95: return "ONIX"; case 123: return "SCYTHER";
        case 117: return "SEADRA"; case 137: return "PORYGON"; case 233: return "PORYGON2"; case 366: return "CLAMPERL";
        case 112: return "RHYDON"; case 125: return "ELECTABUZZ"; case 126: return "MAGMAR"; case 356: return "DUSCLOPS";
        case 349: return "FEEBAS"; case 525: return "BOLDORE"; case 533: return "GURDURR"; case 588: return "KARRABLAST";
        case 616: return "SHELMET";
        default: return nullptr;
    }
}

// One species' evolutions (7 entries of method, value, target), changed in place.
int PatchRecord(uint8_t* rec, int species, const Methods& m, std::string& log)
{
    int changed = 0;
    for (int i = 0; i < 7; i++)
    {
        uint8_t* e = rec + i * 6;
        const uint16_t method = R16(e), value = R16(e + 2), target = R16(e + 4);
        if (target == 0 || method == 0) continue;
        char line[128];
        const char* name = SpeciesLabel(species);
        if (method == m.trade)
        {
            W16(e, m.level);
            W16(e + 2, 37);
            snprintf(line, sizeof line, "%s (#%d): traded -> level 37\n", name ? name : "?", species);
        }
        else if (method == m.tradeItem && species == kSlowpoke)
        {
            W16(e, m.stone);
            W16(e + 2, kWaterStone);
            snprintf(line, sizeof line, "%s (#%d): traded with item %u -> water stone\n", name ? name : "?", species, value);
        }
        else if (method == m.tradeItem)
        {
            W16(e, m.itemDay);
            // The same by night, in the first free entry (the game checks day and night apart).
            bool night = false;
            for (int j = 0; j < 7 && !night; j++)
            {
                uint8_t* f = rec + j * 6;
                if (R16(f) == 0 && R16(f + 4) == 0)
                {
                    W16(f, m.itemNight);
                    W16(f + 2, value);
                    W16(f + 4, target);
                    night = true;
                }
            }
            snprintf(line, sizeof line, "%s (#%d): traded with item %u -> level up holding it%s\n", name ? name : "?", species, value,
                     night ? "" : " (by day only: no room)");
        }
        else if (m.tradeSpecial && method == m.tradeSpecial && (species == kKarrablast || species == kShelmet))
        {
            W16(e, m.withOther);
            W16(e + 2, species == kKarrablast ? kShelmet : kKarrablast);
            snprintf(line, sizeof line, "%s (#%d): traded for %s -> level up with it in the party\n", name ? name : "?", species,
                     species == kKarrablast ? "SHELMET" : "KARRABLAST");
        }
        else continue;
        log += line;
        changed++;
    }
    return changed;
}
}

int PatchNds(uint8_t* rom, size_t size, std::string& log)
{
    if (size < 0x200) return 0;
    const char c0 = (char)rom[0x0C], c1 = (char)rom[0x0D], c2 = (char)rom[0x0E];
    const char code[4] = { c0, c1, c2, 0 };
    const char* path = nullptr;
    const Methods* m = nullptr;
    int count = 0; // species with evolution entries (the members after are forms)
    if (!strcmp(code, "ADA") || !strcmp(code, "APA") || !strcmp(code, "CPU")) { path = "poketool/personal/evo.narc"; m = &kGen4; count = 493; }
    else if (!strcmp(code, "IPK") || !strcmp(code, "IPG")) { path = "a/0/3/4"; m = &kGen4; count = 493; }
    else if (!strcmp(code, "IRB") || !strcmp(code, "IRA") || !strcmp(code, "IRE") || !strcmp(code, "IRD")) { path = "a/0/1/9"; m = &kGen5; count = 649; }
    if (!path) return 0;
    uint32_t start, end;
    Narc narc;
    if (!FindFile(rom, size, path, start, end) || !ReadNarc(rom, start, end, narc))
    {
        log += std::string("evolution data not found (") + path + ")\n";
        return 0;
    }
    int changed = 0;
    for (int s = 1; s <= count && s < (int)narc.members.size(); s++)
        if (narc.members[s].second - narc.members[s].first >= 42) changed += PatchRecord(rom + narc.members[s].first, s, *m, log);
    return changed;
}

int PatchNds(std::vector<uint8_t>& rom, std::string& log) { return PatchNds(rom.data(), rom.size(), log); }

// ---------------------------------------------------------------------------------------------
// GBA: one table of 5 entries (method, value, target, 2 unused) per species, by the games' own
// species numbering. Found by Bulbasaur's and Ivysaur's entries at its start, so it is found in
// every language.

int PatchGba(uint8_t* rom, size_t size, std::string& log)
{
    if (size < 0x200) return 0;
    const char code[4] = { (char)rom[0xAC], (char)rom[0xAD], (char)rom[0xAE], 0 };
    if (strcmp(code, "AXV") && strcmp(code, "AXP") && strcmp(code, "BPE") && strcmp(code, "BPR") && strcmp(code, "BPG")) return 0;
    static const uint8_t bulbasaur[8] = { 4, 0, 16, 0, 2, 0, 0, 0 }, ivysaur[8] = { 4, 0, 32, 0, 3, 0, 0, 0 };
    size_t table = 0;
    for (size_t at = 0x28; at + 0x30 < size; at += 4)
        if (!memcmp(rom + at, bulbasaur, 8) && !memcmp(rom + at + 0x28, ivysaur, 8)) { table = at - 0x28; break; }
    if (!table) { log += "evolution table not found\n"; return 0; }
    const uint16_t kLevel = 4, kStone = 7, kTrade = 5, kTradeItem = 6, kWater = 97, kDeepSeaTooth = 192, kDeepSeaScale = 193;
    int changed = 0;
    for (int s = 1; s < 450 && table + (size_t)(s + 1) * 0x28 <= size; s++)
        for (int i = 0; i < 5; i++)
        {
            uint8_t* e = rom + table + (size_t)s * 0x28 + i * 8;
            const uint16_t method = R16(e), value = R16(e + 2);
            if (method != kTrade && method != kTradeItem) continue;
            char line[96];
            if (method == kTrade) { W16(e, kLevel); W16(e + 2, 37); snprintf(line, sizeof line, "#%d: traded -> level 37\n", s); }
            else if (s == 61) { W16(e, kLevel); W16(e + 2, 37); snprintf(line, sizeof line, "POLIWHIRL: -> level 37\n"); }
            else if (s == 79) { W16(e, kStone); W16(e + 2, kWater); snprintf(line, sizeof line, "SLOWPOKE: -> water stone\n"); }
            else if (s == 117) { W16(e, kLevel); W16(e + 2, 40); snprintf(line, sizeof line, "SEADRA: -> level 40\n"); }
            else if (value == kDeepSeaTooth) { W16(e, kLevel); W16(e + 2, 30); snprintf(line, sizeof line, "CLAMPERL (HUNTAIL): -> level 30\n"); }
            else if (value == kDeepSeaScale) { W16(e, kStone); W16(e + 2, kWater); snprintf(line, sizeof line, "CLAMPERL (GOREBYSS): -> water stone\n"); }
            else { W16(e, kLevel); W16(e + 2, 30); snprintf(line, sizeof line, "#%d: traded with item %u -> level 30\n", s, value); }
            log += line;
            changed++;
        }
    return changed;
}
}
