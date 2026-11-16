#pragma once
// COMBINED POKEDEX (the Hub's fifth row, GitHub #165): the Pokédexes of all the player's own games
// put together. A Pokémon counts as caught if any of the saves has caught it, and as seen if any
// has seen it. Its rows: the whole National Pokédex, then each region; beside the chosen one, its
// Pokémon in Pokédex order (caught in full colour, seen dimmed, the rest faint) and the counts.
// The saves are read with PKSM-Core (pokemon_hub.h); a ROM hack's is left out, as its Pokédex
// may not be the original's. Included in launcher.h after the Hub's other pages.

#ifdef SSR_POKEMON_HUB
#include <chrono>
#include <filesystem>
#include <map>

struct DexRegion { const char* name; int first, last; };
// The regions, by the National Pokédex numbers each brought in.
inline const std::vector<DexRegion>& DexRegions()
{
    static const std::vector<DexRegion> r = {
        { "National", 1, 0 }, // (last: the highest any save knows)
        { "Kanto", 1, 151 }, { "Johto", 152, 251 }, { "Hoenn", 252, 386 }, { "Sinnoh", 387, 493 },
        { "Unova", 494, 649 }, { "Kalos", 650, 721 }, { "Alola", 722, 809 },
    };
    return r;
}

struct CombinedDex
{
    std::string key;                   // the saves and their times: read again when it changes
    int maxSpecies = 0;                // the highest number any of the saves knows
    std::vector<uint8_t> seen, caught; // by National Pokédex number
    std::vector<std::string> games;    // the games counted
    std::vector<std::string> skipped;  // and those left out (a hack, or a save that can't be read)
    uint32_t checkedAt = 0;
};
inline CombinedDex& DexNow()
{
    static CombinedDex d;
    // (Looked at again at most every two seconds: each save's time, then read only if one changed.)
    // (The standard clock, not SDL's: the Switch launcher has no SDL.)
    const uint32_t now = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (d.checkedAt && now - d.checkedAt < 2000) return d;
    d.checkedAt = now ? now : 1;
    const std::vector<MoverGame> gs = MoverGames();
    std::string key;
    for (const MoverGame& g : gs)
    {
        std::error_code ec;
        const auto t = std::filesystem::last_write_time(std::filesystem::u8path(g.path), ec);
        key += g.path + "|" + (ec ? std::string("?") : std::to_string((long long)t.time_since_epoch().count())) + ";";
    }
    if (key == d.key && !d.key.empty()) return d;
    d.key = key;
    d.maxSpecies = 0;
    d.seen.assign(1024, 0);
    d.caught.assign(1024, 0);
    d.games.clear();
    d.skipped.clear();
    for (const MoverGame& g : gs)
    {
        const std::string name = g.name.substr(0, g.name.find("  \xC2\xB7  "));
        if (g.hack) { d.skipped.push_back(name + " (a ROM hack)"); continue; }
        std::string err;
        std::unique_ptr<PokemonHub::Save> s = PokemonHub::Save::Open(g.path, err, false);
        if (!s) { d.skipped.push_back(name); continue; }
        const int max = std::min(s->MaxSpecies(), (int)d.seen.size() - 1);
        int any = 0;
        for (int sp = 1; sp <= max; sp++)
        {
            if (s->DexCaught(sp)) { d.caught[sp] = 1; d.seen[sp] = 1; any++; }
            else if (s->DexSeen(sp)) { d.seen[sp] = 1; any++; }
        }
        d.maxSpecies = std::max(d.maxSpecies, max);
        d.games.push_back(name);
        fprintf(stderr, "[dex] %s: %d of %d in its Pokedex\n", name.c_str(), any, max);
    }
    return d;
}
// The rows: the regions the saves' Pokédexes reach (all of them up to the highest number known).
inline std::vector<DexRegion> DexRows()
{
    const CombinedDex& d = DexNow();
    std::vector<DexRegion> out;
    for (const DexRegion& r : DexRegions())
    {
        if (r.last == 0) { out.push_back({ r.name, 1, std::max(d.maxSpecies, 151) }); continue; }
        if (r.first <= std::max(d.maxSpecies, 151)) out.push_back(r);
    }
    return out;
}
inline int DexPageRows() { return (int)DexRows().size(); }
inline void DexCount(const CombinedDex& d, const DexRegion& r, int& seen, int& caught)
{
    seen = caught = 0;
    for (int sp = r.first; sp <= r.last && sp < (int)d.seen.size(); sp++) { seen += d.seen[sp]; caught += d.caught[sp]; }
}
// Each Pokémon's icon, as Move Pokémon shows it, kept once made (empty: none in the ROM found).
inline const std::vector<uint32_t>& DexIcon(int species)
{
    static std::map<int, std::vector<uint32_t>> cache;
    auto it = cache.find(species);
    if (it != cache.end()) return it->second;
    std::vector<uint32_t> ic;
    if (!MonIcon(species, false, ic)) ic.clear();
    return cache.emplace(species, std::move(ic)).first->second;
}
// An icon as caught (as it is), seen (darker) or neither (a faint shape).
inline void DrawDexIcon(Canvas& c, const std::vector<uint32_t>& ic, int x, int y, int size, int state)
{
    if (ic.size() < 32 * 32 || size <= 0) return;
    for (int j = 0; j < size; j++)
    {
        const int yy = y + j;
        if (yy < 0 || yy >= c.h) continue;
        for (int i = 0; i < size; i++)
        {
            const int xx = x + i;
            if (xx < 0 || xx >= c.w) continue;
            uint32_t p = ic[(size_t)(j * 32 / size) * 32 + (i * 32 / size)];
            if (!(p >> 24)) continue;
            uint32_t& dst = c.px[(size_t)yy * c.w + xx];
            if (state == 2) dst = p | 0xFF000000u;
            else if (state == 1)
                dst = 0xFF000000u | (((p >> 16) & 0xFF) * 2 / 5) << 16 | (((p >> 8) & 0xFF) * 2 / 5) << 8 | ((p & 0xFF) * 2 / 5);
            else
            {
                // (a faint silhouette: the background lightened a little)
                const uint32_t d = dst;
                dst = 0xFF000000u | (((d >> 16) & 0xFF) * 3 / 4 + 0x18) << 16 | (((d >> 8) & 0xFF) * 3 / 4 + 0x18) << 8 | ((d & 0xFF) * 3 / 4 + 0x18);
            }
        }
    }
}

inline void DrawDexPage(Canvas& c, bool solid, float u, int L, int R, int top, int bottom)
{
    GlassFocus& F = Focus();
    const uint32_t text = Menu::GlassText, hint = Menu::GlassHint, green = 0xFF8FE3A2u, blue = 0xFF8CC8FFu;
    const CombinedDex& d = DexNow();
    const std::vector<DexRegion> rows = DexRows();
    const int n = (int)rows.size();
    F.prow = std::max(0, std::min(F.prow, n - 1));
    F.pact = -1;
    const bool stk = PhoneLook() && c.h > c.w;
    const int gap = (int)(18 * u), pad = (int)(14 * u);
    const int rowH = (int)((stk ? 40 : 50) * u), rg = (int)((stk ? 5 : 7) * u);
    const int lw = stk ? R - L : std::max((int)(280 * u), (int)((R - L - gap) * 0.32f));
    const int lh = stk ? std::min(bottom - top, 2 * pad + n * (rowH + rg) - rg) : bottom - top;
    Menu::FrostBox(c, solid, L, top, lw, lh, 18 * u);
    F.pX = L + pad; F.pY = top + pad; F.pW = lw - 2 * pad; F.pRow = rowH + rg; F.rVis = n; F.rScroll = 0; F.aW = 0;
    for (int i = 0; i < n; i++)
    {
        const int rx = L + pad, ry = top + pad + i * (rowH + rg), rww = lw - 2 * pad, mid = ry + rowH / 2;
        if (i == F.prow)
        {
            if (F.side < 0) Glass::Glow(c, rx, ry, rww, rowH, 14 * u, u);
            Glass::Round(c, rx, ry, rww, rowH, 14 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
        }
        else Glass::Round(c, rx, ry, rww, rowH, 14 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
        int seen, caught;
        DexCount(d, rows[i], seen, caught);
        const int total = rows[i].last - rows[i].first + 1;
        GT(c, rx + (int)(16 * u), mid, rows[i].name, 16 * u, text, true);
        const std::string st = std::to_string(caught) + " / " + std::to_string(total);
        const int chipW = GW(st, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(14 * u) - chipW;
        Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
        GT(c, chipX + (int)(10 * u), mid, st, 12 * u, caught == total ? green : text, true);
    }
    // The chosen region: its Pokémon, and how far along it is.
    const int dx = stk ? L : L + lw + gap, dTop = stk ? top + lh + gap : top, dw = stk ? R - L : R - dx, dh = bottom - dTop;
    if (dh < (int)(80 * u)) return;
    Menu::FrostBox(c, solid, dx, dTop, dw, dh, 18 * u);
    const DexRegion& r = rows[F.prow];
    int seen, caught;
    DexCount(d, r, seen, caught);
    const int total = r.last - r.first + 1, ix = dx + (int)(22 * u), iw = dw - (int)(44 * u);
    int y = dTop + (int)(22 * u);
    GT(c, ix, y + 12 * u, std::string(r.name) + " Pok\xC3\xA9" "dex", 22 * u, text, true);
    const std::string counts = std::to_string(caught) + " caught   \xC2\xB7   " + std::to_string(seen) + " seen   \xC2\xB7   " + std::to_string(total) + " in all";
    GTFit(c, ix, y + 42 * u, counts, 14 * u, iw, caught == total ? green : blue, true);
    y += (int)(62 * u);
    // (Who is counted, on one line under the grid.)
    std::string from = d.games.empty() ? std::string("No saves with a Pok\xC3\xA9" "dex yet: play a game and save it.") : "From: ";
    for (size_t k = 0; k < d.games.size(); k++) from += (k ? ", " : "") + d.games[k];
    if (!d.skipped.empty())
    {
        from += ".  Left out: ";
        for (size_t k = 0; k < d.skipped.size(); k++) from += (k ? ", " : "") + d.skipped[k];
    }
    const int footH = (int)(46 * u), gridTop = y, gridH = dTop + dh - footH - gridTop;
    // The largest icon that fits them all.
    int size = (int)(48 * u);
    while (size > 8)
    {
        const int cols = std::max(1, iw / size), rowsNeeded = (total + cols - 1) / cols;
        if (rowsNeeded * size <= gridH) break;
        size--;
    }
    const int cols = std::max(1, iw / size), x0 = ix + (iw - cols * size) / 2;
    for (int k = 0; k < total; k++)
    {
        const int sp = r.first + k;
        const int state = sp < (int)d.caught.size() && d.caught[sp] ? 2 : sp < (int)d.seen.size() && d.seen[sp] ? 1 : 0;
        const int x = x0 + (k % cols) * size, yy = gridTop + (k / cols) * size;
        const std::vector<uint32_t>& ic = DexIcon(sp);
        if (!ic.empty()) DrawDexIcon(c, ic, x, yy, size, state);
        else Glass::Round(c, x + size / 3, yy + size / 3, size / 3, size / 3, size / 6.0f, state == 2 ? 0xC0FFFFFFu : state == 1 ? 0x60FFFFFFu : 0x20FFFFFFu);
    }
    const std::vector<std::string> lines = WrapLines(from, 12 * u, iw, false);
    int fy = dTop + dh - footH + (int)(10 * u);
    for (size_t k = 0; k < lines.size() && k < 2; k++, fy += (int)(17 * u)) GT(c, ix, fy, lines[k], 12 * u, hint, false);
}
#endif
