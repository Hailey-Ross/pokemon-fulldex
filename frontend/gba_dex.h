#pragma once
// The Pokedex page's data for the GBA games (Ruby, Sapphire, Emerald, FireRed, LeafGreen and the
// hacks built on them), read from the player's own ROM at run time. The tables are found by what
// they hold, not by fixed addresses, so translations and hacks that keep their layout work too:
// base stats by Bulbasaur's and Ivysaur's, names by "BULBASAUR", the pictures by their tables of
// {picture, size 0x800, species} and {palette, tag} entries (pokeemerald's gMonFrontPicTable,
// gMonPaletteTable and gMonShinyPaletteTable). No game graphics or data are bundled or written.
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <set>
#include <vector>

namespace GbaDex
{
using Bytes = std::vector<uint8_t>;
inline uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
inline uint32_t U32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
inline bool Range(size_t size, size_t at, size_t n) { return at <= size && n <= size - at; }

// The accented letters of the European games (pokeemerald's charmap.txt), as UTF-8.
inline const char* Accented(uint8_t c)
{
    switch (c)
    {
    case 0x01: return "\xC3\x80"; case 0x02: return "\xC3\x81"; case 0x03: return "\xC3\x82"; case 0x04: return "\xC3\x87";
    case 0x05: return "\xC3\x88"; case 0x06: return "\xC3\x89"; case 0x07: return "\xC3\x8A"; case 0x08: return "\xC3\x8B";
    case 0x09: return "\xC3\x8C"; case 0x0B: return "\xC3\x8E"; case 0x0C: return "\xC3\x8F"; case 0x0D: return "\xC3\x92";
    case 0x0E: return "\xC3\x93"; case 0x0F: return "\xC3\x94"; case 0x10: return "\xC5\x92"; case 0x11: return "\xC3\x99";
    case 0x12: return "\xC3\x9A"; case 0x13: return "\xC3\x9B"; case 0x14: return "\xC3\x91"; case 0x15: return "\xC3\x9F";
    case 0x16: return "\xC3\xA0"; case 0x17: return "\xC3\xA1"; case 0x19: return "\xC3\xA7"; case 0x1A: return "\xC3\xA8";
    case 0x1B: return "\xC3\xA9"; case 0x1C: return "\xC3\xAA"; case 0x1D: return "\xC3\xAB"; case 0x1E: return "\xC3\xAC";
    case 0x20: return "\xC3\xAE"; case 0x21: return "\xC3\xAF"; case 0x22: return "\xC3\xB2"; case 0x23: return "\xC3\xB3";
    case 0x24: return "\xC3\xB4"; case 0x25: return "\xC5\x93"; case 0x26: return "\xC3\xB9"; case 0x27: return "\xC3\xBA";
    case 0x28: return "\xC3\xBB"; case 0x29: return "\xC3\xB1"; case 0x2D: return "&"; case 0x2E: return "+";
    case 0x5A: return "\xC3\x8D"; case 0x68: return "\xC3\xA2"; case 0x6F: return "\xC3\xAD";
    case 0xF1: return "\xC3\x84"; case 0xF2: return "\xC3\x96"; case 0xF3: return "\xC3\x9C";
    case 0xF4: return "\xC3\xA4"; case 0xF5: return "\xC3\xB6"; case 0xF6: return "\xC3\xBC";
    }
    return nullptr;
}
// The games' own characters (the Latin ones; pokeemerald's charmap.txt).
inline std::string Text(const uint8_t* p, size_t max)
{
    std::string out;
    for (size_t i = 0; i < max; i++)
    {
        const uint8_t c = p[i];
        if (c == 0xFF) break;
        // (A control code and its argument, as Ruby's two-line place names have: a space.)
        if (c == 0xFC) { if (out.empty() || out.back() != ' ') out += ' '; i++; continue; }
        if (c == 0x00 && !out.empty() && out.back() == ' ') continue;
        if (c >= 0xBB && c <= 0xD4) out += (char)('A' + c - 0xBB);
        else if (c >= 0xD5 && c <= 0xEE) out += (char)('a' + c - 0xD5);
        else if (c >= 0xA1 && c <= 0xAA) out += (char)('0' + c - 0xA1);
        else if (c == 0x00) out += ' ';
        else if (c == 0xAD) out += '.';
        else if (c == 0xAE) out += '-';
        else if (c == 0xB4) out += '\'';
        else if (c == 0xB5) out += "\xE2\x99\x82"; // (male sign)
        else if (c == 0xB6) out += "\xE2\x99\x80"; // (female sign)
        else if (const char* a = Accented(c)) out += a;
        else if (c == 0xB8) out += ',';
        else if (c == 0xAB) out += '!';
        else if (c == 0xAC) out += '?';
        else if (c == 0xBA) out += '/';
        else if (c == 0xB0) out += "...";
        else if (c == 0x5C) out += '(';
        else if (c == 0x5D) out += ')';
        else out += '?';
    }
    return out;
}
inline Bytes Encode(const char* s)
{
    Bytes out;
    for (; *s; s++)
    {
        const char c = *s;
        if (c >= 'A' && c <= 'Z') out.push_back((uint8_t)(0xBB + c - 'A'));
        else if (c >= 'a' && c <= 'z') out.push_back((uint8_t)(0xD5 + c - 'a'));
        else if (c >= '0' && c <= '9') out.push_back((uint8_t)(0xA1 + c - '0'));
        else if (c == ' ') out.push_back(0);
        else if (c == '-') out.push_back(0xAE);
    }
    return out;
}
inline size_t Find(const Bytes& rom, const Bytes& what, size_t from = 0, size_t align = 1)
{
    if (what.empty() || rom.size() < what.size()) return std::string::npos;
    for (auto it = std::search(rom.begin() + std::min(from, rom.size()), rom.end(), what.begin(), what.end()); it != rom.end();
         it = std::search(it + 1, rom.end(), what.begin(), what.end()))
        if ((size_t)(it - rom.begin()) % align == 0) return (size_t)(it - rom.begin());
    return std::string::npos;
}
// A ROM pointer (0x08000000 on), as an offset; npos if it is not one.
inline size_t Pointer(const Bytes& rom, size_t at)
{
    if (!Range(rom.size(), at, 4)) return std::string::npos;
    const uint32_t p = U32(rom.data() + at);
    if (p < 0x08000000u || p >= 0x08000000u + rom.size()) return std::string::npos;
    return p - 0x08000000u;
}
// The BIOS's LZ77 (type 0x10), as the pictures and palettes are packed.
inline Bytes Lz77(const Bytes& rom, size_t at, size_t limit = 0x10000)
{
    if (!Range(rom.size(), at, 4) || rom[at] != 0x10) return {};
    const size_t size = rom[at + 1] | rom[at + 2] << 8 | rom[at + 3] << 16;
    if (!size || size > limit) return {};
    Bytes out(size);
    size_t p = at + 4, o = 0;
    while (o < size)
    {
        if (p >= rom.size()) return {};
        const uint8_t flags = rom[p++];
        for (int bit = 7; bit >= 0 && o < size; bit--)
        {
            if (!(flags >> bit & 1)) { if (p >= rom.size()) return {}; out[o++] = rom[p++]; continue; }
            if (p + 1 >= rom.size()) return {};
            const unsigned v = rom[p] << 8 | rom[p + 1];
            p += 2;
            const size_t n = (v >> 12) + 3, disp = (v & 0xFFF) + 1;
            if (disp > o) return {};
            for (size_t k = 0; k < n && o < size; k++, o++) out[o] = out[o - disp];
        }
    }
    return out;
}

struct Stats
{
    bool ok = false;
    int base[6] = {}; // HP, Attack, Defense, Sp. Atk, Sp. Def, Speed
    int ev[6] = {};
    int type1 = 0, type2 = 0, catchRate = 0, baseExp = 0, gender = 0, hatchCycles = 0, friendship = 0, growth = 0;
    int eggGroup1 = 0, eggGroup2 = 0, ability1 = 0, ability2 = 0, hidden = 0;
};
struct Picture { int width = 0, height = 0; std::vector<uint32_t> pixels; bool Ready() const { return width > 0 && pixels.size() == (size_t)width * height; } };

struct Data
{
    size_t stats = std::string::npos, statsStride = 28;
    size_t names = std::string::npos, nameLength = 11;
    size_t abilities = std::string::npos, abilityLength = 13;
    size_t national = std::string::npos; // (u16 a species from species 1: its National Pokedex number)
    size_t frontPics = std::string::npos, palettes = std::string::npos, shinyPalettes = std::string::npos;
    size_t moveNames = std::string::npos, levelMoves = std::string::npos, tmMoves = std::string::npos, tmBits = std::string::npos, eggMoves = std::string::npos;
    size_t items = std::string::npos, battleMoves = std::string::npos;
    size_t wildHeaders = std::string::npos, mapGroups = std::string::npos, mapsecNames = std::string::npos;
    int mapsecFirst = 0;      // (the first region map section the names table has: 0x58 in FireRed and LeafGreen)
    bool mapsecPointers = false; // (FireRed and LeafGreen: a table of name pointers; the others: {x, y, w, h, name})
    int pictureCount = 0; // (species the picture tables cover)
    size_t picRun = 0, palRun = 0, shinyRun = 0;
    int count = 0; // species with a stats record (the table's length, as far as it could be told)
    const Bytes* rom = nullptr;

    bool Ready() const { return rom && stats != std::string::npos; }
    void Load(const Bytes& r)
    {
        rom = &r;
        // (Base stats: Bulbasaur's 45 49 49 45 65 65, then Ivysaur's 60 62 63 60 80 80 a record on.
        // A hack may keep the old table and use a longer one elsewhere: the longest is taken.)
        const uint8_t bulba[] = { 45, 49, 49, 45, 65, 65 }, ivy[] = { 60, 62, 63, 60, 80, 80 };
        auto statsCount = [&](size_t table, size_t stride) {
            int n = 0;
            for (int i = 1; i < 2048 && Range(r.size(), table + i * stride, stride); i++)
            {
                const uint8_t* e = r.data() + table + i * stride;
                if (e[6] > 25 || e[7] > 25) break;
                n = i;
            }
            return n;
        };
        for (size_t at = Find(r, Bytes(bulba, bulba + 6)); at != std::string::npos; at = Find(r, Bytes(bulba, bulba + 6), at + 1))
            for (size_t stride : { 28 }) // (pokeemerald-expansion's records are laid out differently)
                if (at >= stride && Range(r.size(), at + stride, 6) && !memcmp(r.data() + at + stride, ivy, 6))
                {
                    const int n = statsCount(at - stride, stride);
                    if (n > count) { stats = at - stride; statsStride = stride; count = n; }
                }
        // (Names and abilities: tables of fixed-length entries, the first "??????????" for names and
        // "-------" for abilities in every language; the longest run of names that read as words.)
        auto word = [&](size_t at, size_t len) {
            int letters = 0;
            for (size_t i = 0; i < len && at + i < r.size(); i++)
            {
                const uint8_t c = r[at + i];
                if (c == 0xFF) return letters >= 2;
                if ((c >= 0xBB && c <= 0xEE) || (c >= 0xA1 && c <= 0xAA) || c == 0 || c == 0xAD || c == 0xAE || c == 0xB4 || c == 0xB5 || c == 0xB6 || c == 0x1B || c == 0xB8) { letters++; continue; }
                if (Accented(c)) { letters++; continue; }
                return false;
            }
            return letters >= 2;
        };
        auto table = [&](const Bytes& first, std::initializer_list<size_t> lengths, size_t& best, size_t& bestLen, int most) {
            int bestRun = 0;
            for (size_t at = Find(r, first); at != std::string::npos; at = Find(r, first, at + 1))
                for (size_t len : lengths)
                {
                    int run = 0;
                    while (run < most && word(at + (run + 1) * len, len)) run++;
                    if (run > bestRun) { bestRun = run; best = at; bestLen = len; }
                }
            return bestRun;
        };
        Bytes unknown(10, 0xAC); unknown.push_back(0xFF);
        Bytes dashes(7, 0xAE); dashes.push_back(0xFF);
        table(unknown, { 11, 12, 13 }, names, nameLength, 2048);
        table(dashes, { 13, 17 }, abilities, abilityLength, 1024);
        // (The National Pokedex number of each species, where the games keep their own order (Hoenn's
        // Pokemon from 277): 1 to 251 in turn, then Treecko's 252 at species 277.)
        for (size_t at = 0; Range(r.size(), at, 277 * 2) && national == std::string::npos; at += 2)
        {
            if (U16(r.data() + at) != 1 || U16(r.data() + at + 2) != 2 || U16(r.data() + at + 500) != 251) continue;
            bool ok = true;
            for (int i = 0; i < 251 && ok; i++) ok = U16(r.data() + at + i * 2) == i + 1;
            if (ok && U16(r.data() + at + 276 * 2) == 252) national = at;
        }
        // (Move names: 13 bytes each, the first "-": the longest such table that reads as words.)
        {
            const Bytes dash = { 0xAE, 0xFF };
            size_t len = 13; int best = 0;
            for (size_t found = Find(r, dash); found != std::string::npos; found = Find(r, dash, found + 1))
            {
                size_t at = found;
                while (at > 0 && r[at - 1] == 0xAE) at--; // (the first entry may be "--")
                int run = 0;
                while (run < 1024 && word(at + (run + 1) * 13, 13)) run++;
                if (run > best && run >= 300) { best = run; moveNames = at; }
            }
            (void)len;
        }
        // (Moves by level: a table of pointers, one a species, to u16 lists (move, and the level
        // times 512), 0xFFFF at the end; found by Bulbasaur's: Tackle 1, Growl 4, Leech Seed 7,
        // Vine Whip 10.)
        {
            const Bytes bulbaMoves = { 0x21, 0x02, 0x2D, 0x08, 0x49, 0x0E, 0x16, 0x14 };
            for (size_t at = Find(r, bulbaMoves, 0, 2); at != std::string::npos && levelMoves == std::string::npos; at = Find(r, bulbaMoves, at + 1, 2))
            {
                uint8_t ptr[4]; const uint32_t p = 0x08000000u + (uint32_t)at;
                memcpy(ptr, &p, 4);
                for (size_t q = Find(r, Bytes(ptr, ptr + 4), 0, 4); q != std::string::npos; q = Find(r, Bytes(ptr, ptr + 4), q + 4, 4))
                    if (q >= 4 && Pointer(r, q - 4) != std::string::npos && Pointer(r, q + 4) != std::string::npos) { levelMoves = q - 4; break; }
            }
        }
        // (TMs and HMs: their 58 moves (Focus Punch, Dragon Claw, Water Pulse, Calm Mind first), and
        // 8 bytes of bits a species, found by none for species 0 and Bulbasaur's.)
        {
            const Bytes first = { 0x08, 0x01, 0x51, 0x01, 0x60, 0x01, 0x5B, 0x01 };
            tmMoves = Find(r, first, 0, 2);
            const Bytes bits = { 0, 0, 0, 0, 0, 0, 0, 0, 32, 7, 53, 132, 8, 30 }; // (Bulbasaur: Toxic, Bullet Seed ... Cut, Strength)
            tmBits = Find(r, bits, 0, 4);
        }
        // (Egg moves: one list, each species' after 20000 plus its number; Bulbasaur's first.)
        {
            const Bytes marker = { 0x21, 0x4E };
            for (size_t at = Find(r, marker, 0, 2); at != std::string::npos; at = Find(r, marker, at + 2, 2))
            {
                size_t end = at + 2;
                while (Range(r.size(), end, 2) && U16(r.data() + end) > 0 && U16(r.data() + end) < 1000) end += 2;
                if (Range(r.size(), end, 2) && U16(r.data() + end) == 20004 && end - at > 4) { eggMoves = at; break; }
            }
        }
        // (A hack with more Pokemon than these games (Radical Red, Unbound) may keep the old move
        // tables beside its own: the moves only where the table covers all its Pokemon, the
        // machines and egg moves not at all, rather than show another game's.)
        if (count > 420)
        {
            int n = 0;
            if (levelMoves != std::string::npos)
                while (n <= count && Pointer(r, levelMoves + n * 4) != std::string::npos) n++;
            if (n <= count) levelMoves = std::string::npos;
            tmMoves = tmBits = eggMoves = std::string::npos;
        }
        FindWild(r);
        // (Items: 44 bytes each, the name first and the item's own number at 14; found by items 1
        // and 2 numbering themselves.)
        for (size_t at = 0; Range(r.size(), at, 44 * 4) && items == std::string::npos; at += 4)
            if (U16(r.data() + at + 44 + 14) == 1 && U16(r.data() + at + 88 + 14) == 2 && U16(r.data() + at + 132 + 14) == 3 && U16(r.data() + at + 14) == 0 &&
                word(at + 44, 14) && word(at + 88, 14))
                items = at;
        // (Moves' data: 12 bytes each; Pound (power 40, Normal, 100%, 35 PP) and Karate Chop (50,
        // Fighting, 100%, 25 PP) first.)
        {
            const Bytes pound = { 0, 40, 0, 100, 35 };
            for (size_t at = Find(r, pound); at != std::string::npos && battleMoves == std::string::npos; at = Find(r, pound, at + 1))
                if (at >= 12 && Range(r.size(), at + 12, 5) && r[at + 13] == 50 && r[at + 14] == 1 && r[at + 15] == 100 && r[at + 16] == 25) battleMoves = at - 12;
        }
        // (Pictures: tables of {pointer, 0x800, species} from species 0; palettes {pointer, tag, 0},
        // the normal ones tagged with the species, the shiny ones with it plus a fixed amount. Hacks
        // keep the old tables and add longer ones: the longest of each is taken.)
        picRun = palRun = shinyRun = 0;
        std::vector<std::pair<size_t, size_t>> shinyTables;
        for (size_t at = 0; Range(r.size(), at, 24); at += 4)
        {
            const uint8_t* e = r.data() + at;
            if (Pointer(r, at) == std::string::npos || Pointer(r, at + 8) == std::string::npos) continue;
            if (U16(e + 4) == 0x800 && U16(e + 6) == 0 && U16(e + 12) == 0x800 && U16(e + 14) == 1)
            {
                size_t n = 0;
                while (Range(r.size(), at + n * 8, 8) && U16(e + n * 8 + 4) == 0x800 && U16(e + n * 8 + 6) == n && Pointer(r, at + n * 8) != std::string::npos) n++;
                if (n > picRun) { picRun = n; frontPics = at; }
                at += n * 8 - 4;
                continue;
            }
            if (U16(e + 6) || U16(e + 14) || U16(e + 12) != U16(e + 4) + 1) continue;
            const size_t p0 = Pointer(r, at);
            if (r[p0] != 0x10) continue;
            const unsigned t0 = U16(e + 4);
            size_t n = 0;
            while (Range(r.size(), at + n * 8, 8) && U16(e + n * 8 + 4) == t0 + n && !U16(e + n * 8 + 6) && Pointer(r, at + n * 8) != std::string::npos) n++;
            if (n < 100) continue;
            if (t0 == 0) { if (n > palRun) { palRun = n; palettes = at; } }
            else shinyTables.push_back({ at, n });
            at += n * 8 - 4;
        }
        // (The shiny palettes' tags start past the normal ones' (500 in these games).)
        for (const auto& t : shinyTables)
            if (t.second > shinyRun && t.second + 8 >= palRun && U16(r.data() + t.first + 4) >= palRun) { shinyRun = t.second; shinyPalettes = t.first; }
        // (The pictures only when their tables agree: a hack that added Pokemon has longer tables of
        // its own (Radical Red, Unbound), and a picture with another's palette is worse than none.)
        if (picRun != palRun) { frontPics = palettes = shinyPalettes = std::string::npos; picRun = palRun = shinyRun = 0; }
        pictureCount = (int)std::min(picRun, palRun);
    }
    static bool Readable(const std::string& t) { return t.size() >= 2 && t.find('?') == std::string::npos; }
    // Species from a National Pokedex number, and back (the games' own order from Treecko on).
    int Species(int dex) const
    {
        if (national == std::string::npos) return dex;
        for (int sp = 1; sp <= count; sp++) if (Range(rom->size(), national + (sp - 1) * 2, 2) && U16(rom->data() + national + (sp - 1) * 2) == dex) return sp;
        return 0;
    }
    int Dex(int species) const
    {
        if (national == std::string::npos || species <= 0) return species;
        return Range(rom->size(), national + (species - 1) * 2, 2) ? U16(rom->data() + national + (species - 1) * 2) : 0;
    }
    std::string Name(int species) const
    {
        if (names == std::string::npos || species < 0 || !Range(rom->size(), names + species * nameLength, nameLength)) return {};
        return Text(rom->data() + names + species * nameLength, nameLength);
    }
    // Where to find: gWildMonHeaders (pokeemerald): {map group, map number, 2 bytes, then pointers
    // to the land, water, rock smash and fishing encounters (or none)}, 0xFF 0xFF at the end; each
    // encounters {rate, 3 bytes, pointer to {min level, max level, species} slots}. The maps' place
    // names: gMapGroups (pointers to each group's pointers to map headers, the region map section at
    // 0x14) and the sections' names (Ruby, Sapphire and Emerald: {x, y, w, h, name pointer} a
    // section; FireRed and LeafGreen: a name pointer a section from KANTO's, 0x58).
    bool Info(const Bytes& r, size_t info, int slots) const
    {
        if (info == std::string::npos || !Range(r.size(), info, 8) || r[info] > 100) return false;
        const size_t mons = Pointer(r, info + 4);
        if (mons == std::string::npos || !Range(r.size(), mons, slots * 4)) return false;
        for (int i = 0; i < slots; i++)
        {
            const uint8_t* m = r.data() + mons + i * 4;
            if (!m[0] || m[0] > 100 || m[1] < m[0] || m[1] > 100 || U16(m + 2) > 2000) return false;
        }
        return true;
    }
    bool WildEntry(const Bytes& r, size_t at) const
    {
        if (!Range(r.size(), at, 20) || r[at] == 0xFF) return false;
        bool any = false;
        static const int slots[4] = { 12, 5, 5, 10 };
        for (int k = 0; k < 4; k++)
        {
            const uint32_t p = U32(r.data() + at + 4 + k * 4);
            if (!p) continue;
            if (!Info(r, Pointer(r, at + 4 + k * 4), slots[k])) return false;
            any = true;
        }
        return any;
    }
    static bool Header(const Bytes& r, size_t h)
    {
        return h != std::string::npos && Range(r.size(), h, 0x1C) && Pointer(r, h) != std::string::npos && Pointer(r, h + 4) != std::string::npos && r[h + 0x14] < 0xD5;
    }
    void FindWild(const Bytes& r)
    {
        int best = 0;
        for (size_t at = 0; Range(r.size(), at, 20 * 20); at += 4)
        {
            if (!WildEntry(r, at)) continue;
            int n = 0;
            while (WildEntry(r, at + n * 20)) n++;
            if (n > best && Range(r.size(), at + n * 20, 2) && r[at + n * 20] == 0xFF) { best = n; wildHeaders = at; }
            at += n * 20 - 4;
        }
        if (best < 20) { wildHeaders = std::string::npos; return; }
        // (Map groups: pointers to groups whose first map's header looks like one.)
        for (size_t at = 0; Range(r.size(), at, 4 * 20) && mapGroups == std::string::npos; at += 4)
        {
            int n = 0;
            for (; n < 64; n++)
            {
                const size_t group = Pointer(r, at + n * 4);
                if (group == std::string::npos || !Header(r, Pointer(r, group))) break;
            }
            if (n >= 20) mapGroups = at;
        }
        size_t bestAt = std::string::npos;
        // (Section names: Ruby, Sapphire and Emerald's {x, y, w, h, name}; FireRed and LeafGreen's
        // pointers, by its game code.)
        const bool frlg = r.size() > 0xB0 && r[0xAC] == 'B' && r[0xAD] == 'P' && (r[0xAE] == 'R' || r[0xAE] == 'G');
        auto readable = [&](size_t p) {
            if (p == std::string::npos || !Range(r.size(), p, 2)) return false;
            int n = 0;
            while (n < 24 && Range(r.size(), p + n, 1) && r[p + n] != 0xFF) { const uint8_t c = r[p + n]; if (!((c >= 0xBB && c <= 0xEE) || c == 0 || (c >= 0xA1 && c <= 0xAA) || c == 0xAD || c == 0xAE || c == 0xB4 || c == 0xFC || Accented(c))) return false; n++; }
            return n >= 3 && n < 24;
        };
        // (Of the tables of names, the one that names the most of the wild encounters' maps after
        // routes: "ROUTE 101", "ROUTE 1", "PERCORSO 1"; credits and item lists name none.)
        mapsecPointers = frlg;
        mapsecFirst = frlg ? 0x58 : 0;
        int bestScore = 0;
        for (size_t at = 0; Range(r.size(), at, 8 * 40); at += 4)
        {
            int n = 0;
            if (frlg) { while (n < 200 && readable(Pointer(r, at + n * 4))) n++; }
            else { while (n < 300 && Range(r.size(), at + n * 8, 8) && r[at + n * 8] < 40 && r[at + n * 8 + 1] < 40 && readable(Pointer(r, at + n * 8 + 4))) n++; }
            if (n < 40) continue;
            mapsecNames = at;
            std::set<std::string> routes;
            for (size_t w = wildHeaders; WildEntry(r, w); w += 20)
            {
                const std::string place = PlaceOf(r[w], r[w + 1]);
                if (place.find_first_of("0123456789") != std::string::npos) routes.insert(place);
            }
            if ((int)routes.size() > bestScore) { bestScore = (int)routes.size(); bestAt = at; }
            at += (frlg ? n * 4 : n * 8) - 4;
        }
        mapsecNames = bestScore >= 3 ? bestAt : std::string::npos;
        mapsecPointers = frlg;
        mapsecFirst = frlg ? 0x58 : 0;
    }
    std::string PlaceOf(int group, int num) const
    {
        const Bytes& r = *rom;
        if (mapGroups == std::string::npos || mapsecNames == std::string::npos) return {};
        const size_t g = Pointer(r, mapGroups + group * 4);
        if (g == std::string::npos) return {};
        const size_t h = Pointer(r, g + num * 4);
        if (!Header(r, h)) return {};
        const int sec = r[h + 0x14] - mapsecFirst;
        if (sec < 0) return {};
        const size_t name = mapsecPointers ? Pointer(r, mapsecNames + sec * 4) : Pointer(r, mapsecNames + sec * 8 + 4);
        if (name == std::string::npos) return {};
        return Text(r.data() + name, 32);
    }
    // A map of the region drawn from the game's own table of its places (Ruby, Sapphire and
    // Emerald: each place's square on the region map's grid of 8-pixel tiles): towns, routes and the
    // rest in their own colours, where you are marked. Not the game's map picture: none is copied.
    // FireRed and LeafGreen keep their squares elsewhere: no map (ok false).
    struct RegionPicture { bool ok = false; int w = 0, h = 0; std::vector<uint32_t> px; float markerX = -1, markerY = -1; };
    int MapsecOf(int group, int num) const
    {
        const Bytes& r = *rom;
        if (mapGroups == std::string::npos) return -1;
        const size_t g = Pointer(r, mapGroups + group * 4);
        if (g == std::string::npos) return -1;
        const size_t h = Pointer(r, g + num * 4);
        return Header(r, h) ? r[h + 0x14] : -1;
    }
    RegionPicture Region(int group, int num) const
    {
        RegionPicture m;
        const Bytes& r = *rom;
        if (mapsecPointers || mapsecNames == std::string::npos) return m;
        struct Sec { int x, y, w, h; bool route, town; };
        std::vector<Sec> secs;
        int gw = 0, gh = 0;
        for (int i = 0; i < 256 && Range(r.size(), mapsecNames + i * 8, 8); i++)
        {
            const uint8_t* e = r.data() + mapsecNames + i * 8;
            const size_t name = Pointer(r, mapsecNames + i * 8 + 4);
            if (name == std::string::npos || e[0] >= 40 || e[1] >= 40) break;
            const std::string t = Text(r.data() + name, 24);
            const bool route = t.find_first_of("0123456789") != std::string::npos;
            const bool town = !route && (t.find("TOWN") != std::string::npos || t.find("CITY") != std::string::npos || t.find("STADT") != std::string::npos ||
                                         t.find("VILLE") != std::string::npos || t.find("CIUDAD") != std::string::npos || t.find("CITT") != std::string::npos ||
                                         t.find("PUEBLO") != std::string::npos || t.find("BORGO") != std::string::npos || t.find("DORF") != std::string::npos ||
                                         t.find("BOURG") != std::string::npos || t.find("CITE") != std::string::npos);
            secs.push_back({ e[0], e[1], std::max(1, (int)e[2]), std::max(1, (int)e[3]), route, town });
            // (The region's grid: the places with a square on it; the later ones (Kanto's, the
            // secret bases') sit at 0,0.)
            if (e[0] || e[1]) { gw = std::max(gw, e[0] + std::max(1, (int)e[2])); gh = std::max(gh, e[1] + std::max(1, (int)e[3])); }
        }
        if (secs.size() < 40 || gw < 10 || gh < 5) return m;
        const int cell = 8;
        m.w = (gw + 2) * cell; m.h = (gh + 2) * cell;
        m.px.assign((size_t)m.w * m.h, 0xFF1E3A5Fu); // (the sea)
        auto fill = [&](int x, int y, int w, int h, uint32_t c) {
            for (int yy = y; yy < y + h && yy < m.h; yy++)
                for (int xx = x; xx < x + w && xx < m.w; xx++) if (xx >= 0 && yy >= 0) m.px[(size_t)yy * m.w + xx] = c;
        };
        for (int pass = 0; pass < 3; pass++)
            for (const Sec& sec : secs)
            {
                if (!sec.x && !sec.y) continue;
                if ((pass == 0) != sec.route || (pass == 2) != sec.town) continue;
                const uint32_t col = sec.town ? 0xFFE0614Bu : sec.route ? 0xFF7CC08Bu : 0xFFC9B27Au;
                fill((sec.x + 1) * cell + 1, (sec.y + 1) * cell + 1, sec.w * cell - 2, sec.h * cell - 2, col);
            }
        const int here = MapsecOf(group, num);
        if (here >= 0 && here < (int)secs.size() && (secs[here].x || secs[here].y))
        {
            const Sec& sec = secs[here];
            m.markerX = ((sec.x + 1) + sec.w * 0.5f) * cell / m.w;
            m.markerY = ((sec.y + 1) + sec.h * 0.5f) * cell / m.h;
        }
        m.ok = true;
        return m;
    }
    struct Found { std::string place, method; int low = 0, high = 0; };
    std::vector<Found> WhereToFind(int species) const
    {
        std::vector<Found> out;
        if (wildHeaders == std::string::npos || species <= 0) return out;
        const Bytes& r = *rom;
        static const int slots[4] = { 12, 5, 5, 10 };
        static const char* const methods[4] = { "GRASS", "SURF", "ROCK SMASH", "FISHING" };
        for (size_t at = wildHeaders; WildEntry(r, at); at += 20)
        {
            std::string place = PlaceOf(r[at], r[at + 1]);
            if (place.empty()) { char b[24]; snprintf(b, sizeof b, "MAP %d.%d", r[at], r[at + 1]); place = b; }
            for (int k = 0; k < 4; k++)
            {
                const size_t info = Pointer(r, at + 4 + k * 4);
                if (info == std::string::npos) continue;
                const size_t mons = Pointer(r, info + 4);
                for (int i = 0; i < slots[k]; i++)
                {
                    const uint8_t* m = r.data() + mons + i * 4;
                    if (U16(m + 2) != species) continue;
                    // (Fishing: the old rod's first 2 slots, the good rod's next 3, the super rod's 5.)
                    const char* how = k == 3 ? (i < 2 ? "OLD ROD" : i < 5 ? "GOOD ROD" : "SUPER ROD") : methods[k];
                    bool merged = false;
                    for (auto& f : out)
                        if (f.place == place && f.method == how) { f.low = std::min(f.low, (int)m[0]); f.high = std::max(f.high, (int)m[1]); merged = true; break; }
                    if (!merged) out.push_back({ place, how, m[0], m[1] });
                }
            }
        }
        return out;
    }
    std::string ItemName(int id) const
    {
        if (items == std::string::npos || id <= 0 || !Range(rom->size(), items + id * 44, 44)) return {};
        return Text(rom->data() + items + id * 44, 14);
    }
    // A move's type (Generation 3's numbers) and its PP, or -1.
    int MoveType(int id) const { return battleMoves != std::string::npos && id > 0 && Range(rom->size(), battleMoves + id * 12, 12) ? rom->data()[battleMoves + id * 12 + 2] : -1; }
    int MovePp(int id) const { return battleMoves != std::string::npos && id > 0 && Range(rom->size(), battleMoves + id * 12, 12) ? rom->data()[battleMoves + id * 12 + 4] : 0; }
    // The wild Pokemon of one map: species, how, levels.
    struct Wild { int species = 0; std::string method; int low = 0, high = 0; };
    std::vector<Wild> WildAt(int group, int num) const
    {
        std::vector<Wild> out;
        if (wildHeaders == std::string::npos) return out;
        const Bytes& r = *rom;
        static const int slots[4] = { 12, 5, 5, 10 };
        static const char* const methods[4] = { "GRASS", "SURF", "ROCK SMASH", "FISHING" };
        for (size_t at = wildHeaders; WildEntry(r, at); at += 20)
        {
            if (r[at] != group || r[at + 1] != num) continue;
            for (int k = 0; k < 4; k++)
            {
                const size_t info = Pointer(r, at + 4 + k * 4);
                if (info == std::string::npos) continue;
                const size_t mons = Pointer(r, info + 4);
                for (int i = 0; i < slots[k]; i++)
                {
                    const uint8_t* m = r.data() + mons + i * 4;
                    const char* how = k == 3 ? (i < 2 ? "OLD ROD" : i < 5 ? "GOOD ROD" : "SUPER ROD") : methods[k];
                    bool merged = false;
                    for (auto& w : out)
                        if (w.species == U16(m + 2) && w.method == how) { w.low = std::min(w.low, (int)m[0]); w.high = std::max(w.high, (int)m[1]); merged = true; break; }
                    if (!merged) out.push_back({ U16(m + 2), how, m[0], m[1] });
                }
            }
        }
        return out;
    }
    std::string Move(int id) const
    {
        if (moveNames == std::string::npos || id <= 0 || !Range(rom->size(), moveNames + id * 13, 13)) return {};
        return Text(rom->data() + moveNames + id * 13, 13);
    }
    struct Learned { int move = 0, level = 0; };
    std::vector<Learned> LevelMoves(int species) const
    {
        std::vector<Learned> out;
        if (levelMoves == std::string::npos || species <= 0) return out;
        const size_t at = Pointer(*rom, levelMoves + species * 4);
        if (at == std::string::npos) return out;
        for (size_t i = 0; i < 64 && Range(rom->size(), at + i * 2, 2); i++)
        {
            const unsigned v = U16(rom->data() + at + i * 2);
            if (v == 0xFFFF) break;
            out.push_back({ (int)(v & 511), (int)(v >> 9) });
        }
        return out;
    }
    // TM01-50 then HM01-08: the machine moves this species can use, label and move.
    std::vector<std::pair<std::string, int>> Machines(int species) const
    {
        std::vector<std::pair<std::string, int>> out;
        if (tmMoves == std::string::npos || tmBits == std::string::npos || species <= 0 || !Range(rom->size(), tmBits + species * 8, 8)) return out;
        const uint8_t* b = rom->data() + tmBits + species * 8;
        for (int i = 0; i < 58; i++)
            if (b[i / 8] >> (i % 8) & 1)
            {
                char label[8];
                snprintf(label, sizeof label, i < 50 ? "TM%02d" : "HM%02d", i < 50 ? i + 1 : i - 49);
                out.push_back({ label, U16(rom->data() + tmMoves + i * 2) });
            }
        return out;
    }
    std::vector<int> EggMoves(int species) const
    {
        std::vector<int> out;
        if (eggMoves == std::string::npos || species <= 0) return out;
        bool mine = false;
        for (size_t at = eggMoves; Range(rom->size(), at, 2) && at < eggMoves + 0x4000; at += 2)
        {
            const unsigned v = U16(rom->data() + at);
            if (v == 0xFFFF) break;
            if (v > 20000) { if (mine) break; mine = (int)v - 20000 == species; continue; }
            if (mine) out.push_back((int)v);
        }
        return out;
    }
    std::string Ability(int id) const
    {
        if (abilities == std::string::npos || id <= 0 || !Range(rom->size(), abilities + id * abilityLength, abilityLength)) return {};
        return Text(rom->data() + abilities + id * abilityLength, abilityLength);
    }
    Stats Get(int species) const
    {
        Stats s;
        if (!Ready() || species <= 0 || species > count) return s;
        const uint8_t* p = rom->data() + stats + species * statsStride;
        const int order[6] = { 0, 1, 2, 4, 5, 3 }; // (kept as HP, Atk, Def, Speed, SpA, SpD)
        const unsigned evs = U16(p + 10);
        for (int i = 0; i < 6; i++) { s.base[i] = p[order[i]]; s.ev[i] = evs >> (order[i] * 2) & 3; }
        s.type1 = p[6]; s.type2 = p[7]; s.catchRate = p[8]; s.baseExp = p[9];
        s.gender = p[16]; s.hatchCycles = p[17]; s.friendship = p[18]; s.growth = p[19];
        s.eggGroup1 = p[20]; s.eggGroup2 = p[21]; s.ability1 = p[22]; s.ability2 = p[23];
        // (The Complete FireRed Upgrade, Radical Red's and Unbound's engine: the hidden ability at 26.)
        if (p[26] && p[26] != s.ability1 && p[26] != s.ability2 && Readable(Ability(p[26]))) s.hidden = p[26];
        s.ok = true;
        return s;
    }
    // The battle picture, 64 x 64: 4bpp tiles, 8 to a row; colour 0 clear.
    Picture Front(int species, bool shiny) const
    {
        Picture img;
        if (frontPics == std::string::npos || species <= 0 || species >= pictureCount) return img;
        const size_t pal = shiny ? shinyPalettes : palettes;
        if (pal == std::string::npos || (shiny && species >= (int)shinyRun)) return img;
        const size_t picAt = Pointer(*rom, frontPics + species * 8), palAt = Pointer(*rom, pal + species * 8);
        if (picAt == std::string::npos || palAt == std::string::npos) return img;
        const Bytes tiles = Lz77(*rom, picAt), colours = Lz77(*rom, palAt);
        if (tiles.size() < 0x800 || colours.size() < 32) return img;
        img.width = img.height = 64;
        img.pixels.assign(64 * 64, 0);
        for (int t = 0; t < 64; t++)
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++)
                {
                    int v = tiles[t * 32 + y * 4 + x / 2];
                    v = x & 1 ? v >> 4 : v & 15;
                    if (!v) continue;
                    const unsigned c = U16(colours.data() + v * 2);
                    img.pixels[((t / 8) * 8 + y) * 64 + (t % 8) * 8 + x] =
                        0xFF000000u | ((c & 31) * 255 / 31) << 16 | (((c >> 5) & 31) * 255 / 31) << 8 | ((c >> 10) & 31) * 255 / 31;
                }
        // (A picture that fills its edges is not one of these 64 x 64 battle pictures (a hack that
        // lays them out otherwise): none rather than a wrong one.)
        int edge = 0, opaque = 0;
        for (int i = 0; i < 64; i++)
            for (int k : { 0, 1, 62, 63 })
            {
                edge += 2;
                opaque += (img.pixels[k * 64 + i] >> 24 ? 1 : 0) + (img.pixels[i * 64 + k] >> 24 ? 1 : 0);
            }
        if (opaque * 100 > edge * 30) return Picture();
        return img;
    }
};
}
