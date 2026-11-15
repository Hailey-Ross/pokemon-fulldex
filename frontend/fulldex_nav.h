#pragma once
// FULLDEX NAV: the quick menu, a watch with nine apps (Fly, Free Flight, Open PC, Repel, the encounter
// counter, Reset, Map, Pokedex, Bag), opened with L3 (Tab on a keyboard). The game keeps running behind
// it. Drawn here; what each app does is the game frontend's (main.cpp for the DS games).
#include <cstdint>
#include <ctime>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace FdNav
{
enum { FLY, FREE, PC, REPEL, COUNT, RESET, MAP, DEX, BAG, APPS };

struct App { const char* label; uint32_t col, dark; const char* rows[16]; };

// 16 x 16 pictures: W white, D the tile's darker shade, Y yellow, R red, K near-black.
inline const App& Get(int i)
{
    static const App apps[APPS] = {
        { "FLY", 0xFF428CE2, 0xFF285CA0, {
            "................", "................", "..WW............", ".WWWW...........", "..WWWW......WW..", "...WWWW....WWWW.",
            "....WWWWW.WWWW..", ".....WWWWWWWW...", "....WWWWWWWWYY..", "...WWWWWWWWW....", "..WWW..WWWW.....", "........WW......",
            "........W.......", "................", "................", "................" } },
        { "FREE FLIGHT", 0xFF28B6C4, 0xFF187884, {
            "................", "................", "......WWWW......", "....WWWWWWWW....", "...WWWWWWWWWW...", ".WWWWWWWWWWWWW..",
            "WWWWWWWWWWWWWWW.", "WWWWWWWWWWWWWWW.", ".WWWWWWWWWWWWW..", "................", "..WWWWWWW.......", "....WWWWWWWWW...",
            ".WWWWWW.........", "................", "................", "................" } },
        { "OPEN PC", 0xFF5C6C84, 0xFF2C3646, {
            "................", ".WWWWWWWWWWWWWW.", ".WDDDDDDDDDDDDW.", ".WDDDDRRRRDDDDW.", ".WDDDRRRRRRDDDW.", ".WDDDKKWWKKDDDW.",
            ".WDDDWWWWWWDDDW.", ".WDDDDWWWWDDDDW.", ".WDDDDDDDDDDDDW.", ".WWWWWWWWWWWWWW.", "......WWWW......", "....WWWWWWWW....",
            "................", "................", "................", "................" } },
        { "REPEL", 0xFF9656D2, 0xFF60348C, {
            "................", ".......WWW......", "......WWWWW..W..", ".......WWW..W.W.", "......WWWWW..W..", ".....WWWWWWW....",
            ".....WDDDDDW....", ".....WDYYYDW....", ".....WDYYYDW....", ".....WDDDDDW....", ".....WWWWWWW....", ".....WWWWWWW....",
            "......WWWWW.....", "................", "................", "................" } },
        { "COUNTER", 0xFFEC8828, 0xFFAA5A14, {
            "................", "..WWWWWWWWWWWW..", "..W..........W..", "..W.W.W.W.W..W..", "..W.W.W.W.W..W..", "..W.W.W.W.W..W..",
            "..W.WWWWWWWW.W..", "..W.W.W.W.W..W..", "..W..........W..", "..WWWWWWWWWWWW..", "................", "................",
            "................", "................", "................", "................" } },
        { "RESET", 0xFFDE3A3A, 0xFF961E1E, {
            "................", ".....WWWWWW.....", "...WWW....WWW...", "..WW........WW..", ".WW..........W..", ".W...........WW.",
            ".W.........WWWWW", ".W..........WWW.", ".WW..........W..", "..WW............", "...WWW....WW....", ".....WWWWWW.....",
            "................", "................", "................", "................" } },
        { "MAP", 0xFF4CA854, 0xFF286E32, {
            "................", ".WWWW.WWWW.WWWW.", ".WDDW.WDDW.WDDW.", ".WDDWWWDRRWWDDW.", ".WDDDDDRRRRDDDW.", ".WDDDDDRRRRDDDW.",
            ".WDDDDDDRRDDDDW.", ".WDDDDDDRDDDDDW.", ".WDDWWWDDDWWDDW.", ".WDDW.WDDW.WDDW.", ".WWWW.WWWW.WWWW.", "................",
            "................", "................", "................", "................" } },
        { "POK\xC3\x89" "DEX", 0xFFF4F4F4, 0xFFC8C8C8, {
            "................", ".....RRRRRR.....", "...RRRRRRRRRR...", "..RRRRRRRRRRRR..", "..RRRRRRRRRRRR..", ".RRRRRKKKKRRRRR.",
            ".KKKKKKWWKKKKKK.", ".WWWWWKKKKWWWWW.", "..WWWWWWWWWWWW..", "..WWWWWWWWWWWW..", "...WWWWWWWWWW...", ".....WWWWWW.....",
            "................", "................", "................", "................" } },
        { "BAG", 0xFFC48C4C, 0xFF825628, {
            "................", "......WWWW......", ".....W....W.....", "...WWWWWWWWWW...", "..WWWWWWWWWWWW..", "..WWDDDDDDDDWW..",
            "..WWDYYYYYYDWW..", "..WWDDDDDDDDWW..", "..WWWWWWWWWWWW..", "..WWWWWWWWWWWW..", "...WWWWWWWWWW...", "................",
            "................", "................", "................", "................" } },
    };
    return apps[std::clamp(i, 0, APPS - 1)];
}

struct State
{
    bool open = false;
    int sel = 0;
    uint32_t resetAsk = 0; // (RESET: the frame of the first A; a second A within 3 seconds resets)
    bool counterShown = false; // (the counter on the picture while you play)
    int count = 0;
    std::string countFile; // (beside the save: <save>.counter; empty: not kept)
    void LoadCount()
    {
        count = 0; counterShown = false;
        if (countFile.empty()) return;
        if (FILE* f = fopen(countFile.c_str(), "r")) { int c = 0, s = 0; if (fscanf(f, "%d %d", &c, &s) >= 1) { count = std::max(0, c); counterShown = s != 0; } fclose(f); }
    }
    void SaveCount() const
    {
        if (countFile.empty()) return;
        const std::string tmp = countFile + ".tmp";
        if (FILE* f = fopen(tmp.c_str(), "w")) { fprintf(f, "%d %d\n", count, counterShown ? 1 : 0); fclose(f); std::remove(countFile.c_str()); std::rename(tmp.c_str(), countFile.c_str()); }
    }
};

struct Geo { int x = 0, y = 0, w = 0, h = 0; float k = 1; int sx0 = 0, sy0 = 0, sx1 = 0, sy1 = 0; int tile[APPS][4] = {}; };

// Where it goes: bottom right; centred (centre) when a phone's buttons are at the sides.
inline Geo Layout(int outW, int outH, bool centre)
{
    Geo g;
    g.k = std::max(0.5f, std::min(outW / 1280.0f, outH / 720.0f));
    const float k = g.k;
    g.w = (int)std::lround(380 * k); g.h = (int)std::lround(400 * k);
    g.x = centre ? (outW - g.w) / 2 : outW - g.w - (int)std::lround(56 * k);
    g.y = centre ? (outH - g.h) / 2 : outH - g.h - (int)std::lround(44 * k);
    g.sx0 = g.x + (int)std::lround(26 * k); g.sy0 = g.y + (int)std::lround(36 * k);
    g.sx1 = g.x + g.w - (int)std::lround(26 * k); g.sy1 = g.y + g.h - (int)std::lround(26 * k);
    const float gw = (g.sx1 - g.sx0 - 16 * k) / 3.0f, gh = 98 * k, ts = 60 * k;
    for (int i = 0; i < APPS; i++)
    {
        const float cx = g.sx0 + 8 * k + (i % 3) * gw, cy = g.sy0 + 34 * k + (i / 3) * gh;
        g.tile[i][0] = (int)std::lround(cx + gw / 2 - ts / 2); g.tile[i][1] = (int)std::lround(cy + 4 * k);
        g.tile[i][2] = (int)std::lround(ts); g.tile[i][3] = (int)std::lround(ts);
    }
    return g;
}

// The app under a point (a tap or a click), -1 none; -2 on the watch but not an app (keeps it open).
inline int Hit(const Geo& g, int x, int y)
{
    for (int i = 0; i < APPS; i++)
    {
        const int* t = g.tile[i];
        const int m = (int)std::lround(12 * g.k);
        if (x >= t[0] - m && x < t[0] + t[2] + m && y >= t[1] - m && y < t[1] + t[3] + (int)std::lround(26 * g.k)) return i;
    }
    return x >= g.x && x < g.x + g.w && y >= g.y && y < g.y + g.h ? -2 : -1;
}

// dim: an app this game cannot do (drawn faded). label: what an app's name says now (the counter's
// count, RESET's question), empty for its own. hint: the line under the watch.
inline void Draw(Canvas& c, const Geo& g, const State& s, const bool dim[APPS], const std::string label[APPS], const std::string& hint)
{
    using namespace SettingsArt;
    const float k = g.k;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    // (Its shadow, the watch's body and sheen, the button on its side.)
    Glass::Round(c, g.x + K(6), g.y + K(10), g.w, g.h, 50 * k, 0x60000000);
    Glass::Round(c, g.x, g.y, g.w, g.h, 50 * k, 0xFFC82C2E, 0xFF701218, 4 * k);
    Glass::Round(c, g.x + K(14), g.y + K(8), g.w - K(28), K(22), 12 * k, 0xFFE45C54);
    Glass::Round(c, g.x + g.w - K(4), g.y + K(150), K(18), K(70), 6 * k, 0xFF961C1E, 0xFF701218, 3 * k);
    // (The screen.)
    Glass::Round(c, g.sx0 - K(8), g.sy0 - K(8), g.sx1 - g.sx0 + K(16), g.sy1 - g.sy0 + K(16), 20 * k, 0xFF221C20);
    Glass::Round(c, g.sx0, g.sy0, g.sx1 - g.sx0, g.sy1 - g.sy0, 10 * k, 0xFFE2ECD6);
    for (int y = g.sy0 + K(3); y < g.sy1 - K(3); y += std::max(2, K(4))) c.Fill(g.sx0 + K(2), y, g.sx1 - g.sx0 - K(4), 1, 0xFFDAE5CD);
    const uint32_t ink = 0xFF344038;
    char clock[8] = "--:--";
    const time_t now = time(nullptr);
    if (const tm* t = localtime(&now)) snprintf(clock, sizeof(clock), "%02d:%02d", t->tm_hour, t->tm_min);
    const int fs = std::max(9, K(15));
    T(c, g.sx0 + K(10), g.sy0 + K(22), "FULLDEX NAV", fs, ink, true, false);
    T(c, g.sx1 - K(10) - W(clock, fs, true, false), g.sy0 + K(22), clock, fs, ink, true, false);
    c.Fill(g.sx0 + K(8), g.sy0 + K(28), g.sx1 - g.sx0 - K(16), std::max(1, K(2)), 0xFFA0AC9C);
    const int ls = std::max(8, K(12));
    for (int i = 0; i < APPS; i++)
    {
        const App& a = Get(i);
        const int* t = g.tile[i];
        const int x = t[0], y = t[1], sz = t[2];
        const bool faded = dim && dim[i];
        // (Faded into the screen's colour, opaque: an app this game cannot do yet.)
        auto fade = [&](uint32_t col) {
            if (!faded) return col;
            auto mix = [](uint32_t a, uint32_t b, int sh) { return (((a >> sh) & 0xFF) * 35 + ((b >> sh) & 0xFF) * 65) / 100; };
            const uint32_t bg = 0xFFE2ECD6;
            return 0xFF000000u | (mix(col, bg, 16) << 16) | (mix(col, bg, 8) << 8) | mix(col, bg, 0);
        };
        if (i == s.sel) Glass::Round(c, x - K(6), y - K(6), sz + K(12), sz + K(12), 18 * k, 0, 0xFF1E2228, 3 * k);
        Glass::Round(c, x, y + K(3), sz, sz, 14 * k, fade(a.dark));
        Glass::Round(c, x, y, sz, sz, 14 * k, fade(a.col));
        const int px = std::max(1, (sz - K(12)) / 16), ox = x + (sz - px * 16) / 2, oy = y + (sz - px * 16) / 2;
        for (int r = 0; r < 16; r++)
            for (int q = 0; q < 16; q++)
            {
                const char ch = a.rows[r][q];
                if (ch == '.') continue;
                uint32_t col = ch == 'W' ? 0xFFFFFFFF : ch == 'Y' ? 0xFFFFD640 : ch == 'R' ? 0xFFE23834 : ch == 'K' ? 0xFF1E1E24 : a.dark;
                if (i == DEX && ch == 'W') col = 0xFFFAFAFA;
                if (i == DEX && ch == 'D') col = 0xFFC8C8C8;
                c.Fill(ox + q * px, oy + r * px, px, px, fade(col));
            }
        const std::string name = label && !label[i].empty() ? label[i] : std::string(a.label);
        const int tw = W(name, ls, true, false);
        T(c, x + sz / 2 - tw / 2, y + sz + K(20), name, ls, faded ? 0x80344038 : ink, true, false);
    }
    if (!hint.empty())
    {
        const int hs = std::max(9, K(14)), hw = W(hint, hs, true, false) + K(28);
        const int hx = g.x + g.w / 2 - hw / 2, hy = std::min(c.h - K(34), g.y + g.h + K(10));
        Glass::Round(c, hx, hy, hw, K(26), 13 * k, 0xD8101418);
        T(c, hx + K(14), hy + K(18), hint, hs, 0xFFEBF0F5, true, false);
    }
}

// The counter on the picture while you play (COUNTER, A: on): a small pill in the top right.
inline void DrawCount(Canvas& c, int outW, int outH, int count)
{
    using namespace SettingsArt;
    const float k = std::max(0.5f, std::min(outW / 1280.0f, outH / 720.0f));
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const std::string t = std::to_string(count) + " MET";
    const int fs = std::max(9, K(16)), w = W(t, fs, true, false) + K(40);
    const int x = outW - w - K(16), y = K(14);
    Glass::Round(c, x, y, w, K(30), 15 * k, 0xD8101418);
    Glass::Round(c, x + K(6), y + K(6), K(18), K(18), 5 * k, 0xFFEC8828);
    T(c, x + K(30), y + K(21), t, fs, 0xFFFFFFFF, true, false);
}
} // namespace FdNav
