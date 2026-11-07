// The party icons of HeartGold, SoulSilver and Platinum, read from the player's own ROM (nothing
// is bundled): the same way PokeDaisy does it (lidor30/pokedaisy, NdsArt.kt, GPL-3.0, by Lidor
// Itzhari). The icon archive (HGSS a/0/2/0, Platinum poketool/icongra/pl_poke_icon.narc): member 0
// the palettes (an NCLR, three 16-colour palettes), member 7 + species the icon (an NCGR: 4bpp,
// 8x8 tiles four across, 32 x 64, the party menu's two frames; 494 the egg's). Which of the three
// palettes each takes is the game's own table (sPokemonIconPaletteIndex in pret's decompilations,
// the same in the three games), below as PokeDaisy has it.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace CompanionIcons
{
inline const uint8_t Gen4Palette[495] = {
    0,1,1,1,0,0,0,0,2,2,1,1,0,1,2,2,0,0,0,2,1,0,0,2,2,2,0,2,2,2,2,2,2,2,2,0,0,2,1,0,
    0,2,2,1,0,0,0,0,0,2,2,2,1,1,1,2,1,2,0,0,0,0,0,2,2,2,0,2,0,1,1,1,2,2,1,1,1,0,0,0,
    0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,1,2,2,0,0,0,1,1,1,2,2,1,2,2,1,1,0,0,1,0,0,0,0,
    2,2,0,1,2,1,0,2,2,0,0,2,2,2,0,0,0,0,0,0,2,2,0,1,0,0,0,0,0,2,2,0,1,1,1,1,1,1,2,2,
    2,2,2,2,2,0,0,1,0,2,2,0,0,0,1,2,2,0,0,2,0,0,1,2,2,1,1,1,1,2,2,1,1,1,0,0,2,2,2,0,
    0,0,0,1,0,2,2,2,0,0,2,0,0,1,2,0,0,2,0,0,2,2,0,0,0,0,2,0,0,0,0,0,0,0,2,1,2,2,1,1,
    1,1,1,0,2,0,1,0,1,0,1,1,1,0,1,0,0,0,0,0,0,2,2,2,2,0,2,0,2,1,1,1,1,1,1,0,2,2,0,0,
    1,1,1,2,0,1,1,2,2,1,1,1,1,0,2,2,2,1,2,0,0,2,2,2,2,2,2,0,0,1,0,0,0,0,2,0,1,2,0,0,
    2,0,1,0,1,0,2,1,1,1,1,1,1,0,0,0,2,1,0,0,0,0,0,1,0,2,0,0,0,2,0,0,1,0,0,0,0,1,0,0,
    0,2,0,2,2,0,0,0,0,1,0,2,2,0,0,0,0,2,2,0,0,2,2,0,1,0,0,1,1,1,1,0,0,2,2,0,0,0,0,2,
    2,0,0,0,0,0,1,0,0,0,1,1,1,1,0,0,0,0,0,0,1,0,0,0,2,2,2,2,2,0,1,0,2,0,2,2,0,0,1,0,
    0,0,0,0,0,0,2,2,2,1,1,0,2,0,0,1,0,0,0,1,1,0,0,1,0,2,1,0,2,1,1,0,2,2,0,1,0,2,2,0,
    0,0,0,2,2,0,0,0,0,0,0,0,1,0,1,
};
constexpr int Gen4Egg = 494;

inline int Find(const std::vector<uint8_t>& b, const char* magic)
{
    const size_t n = strlen(magic);
    for (size_t i = 0; i + n <= b.size(); i++)
        if (!memcmp(b.data() + i, magic, n)) return (int)i;
    return -1;
}

// An NCLR's palettes: its TTLP block's 16-colour BGR555 rows, as ARGB.
inline std::vector<std::vector<uint32_t>> Palettes(const std::vector<uint8_t>& nclr)
{
    std::vector<std::vector<uint32_t>> out;
    const int o = Find(nclr, "TTLP");
    if (o < 0) return out;
    const size_t data = (size_t)o + 0x18;
    for (size_t p = 0; data + (p + 1) * 32 <= nclr.size(); p++)
    {
        std::vector<uint32_t> pal(16);
        for (int c = 0; c < 16; c++)
        {
            const unsigned v = nclr[data + (p * 16 + c) * 2] | nclr[data + (p * 16 + c) * 2 + 1] << 8;
            const unsigned r = (v & 31) * 255 / 31, g = (v >> 5 & 31) * 255 / 31, b = (v >> 10 & 31) * 255 / 31;
            pal[c] = 0xFF000000u | r << 16 | g << 8 | b;
        }
        out.push_back(pal);
    }
    return out;
}

// The icon's first frame (the top 32 x 32 of the NCGR's 32 x 64), colour 0 clear.
inline bool Icon(const std::vector<uint8_t>& ncgr, const std::vector<uint32_t>& pal, std::vector<uint32_t>& out)
{
    const int o = Find(ncgr, "RAHC");
    if (o < 0 || pal.size() < 16) return false;
    const size_t px = (size_t)o + 0x20;
    if (ncgr.size() < px + 32 * 32 / 2) return false;
    out.assign(32 * 32, 0);
    for (int i = 0; i < 32 * 32; i++)
    {
        const uint8_t b = ncgr[px + i / 2];
        const int v = (i & 1) ? b >> 4 : b & 15;
        if (!v) continue;
        const int tile = i / 64, within = i % 64;
        out[(size_t)((tile / 4) * 8 + within / 8) * 32 + (tile % 4) * 8 + within % 8] = pal[v];
    }
    return true;
}
} // namespace CompanionIcons
