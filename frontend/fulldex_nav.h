#pragma once
// FULLDEX NAV: the quick menu, a watch (a graphite case, a dark glass face) with nine apps (Fly, Party, Open PC, Repel, the encounter
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
enum { FLY, PARTY, PC, REPEL, COUNT, RESET, MAP, DEX, BAG, APPS };

struct App { const char* label; uint32_t col, dark; const char* rows[16]; };

// 16 x 16 pictures: W white, L the tile's light tint, D its darker shade, Y yellow, R red, K near-black, G grey.
inline const App& Get(int i)
{
    static const App apps[APPS] = {
        { "Fly", 0xFF428CE2, 0xFF285CA0, {
            "................", "................", "WD...........DW.", "WWD.........DWW.", ".WWD.......DWW..", ".WWWD.....DWWW..", "..WWWD.KK.DWWW..", "..WWWWDWWDWWW...", "...WWWWWWWWWW...", "....WWWWWWWW....", ".....WWWWWW.....", "......WYYW......", "......WWWW......", ".....WW..WW.....", "................", "................" } },
        { "Party", 0xFFE0559A, 0xFF9C2E68, {
            "................", "......KKKK......", ".....KRRRRK.....", ".....KKWWKK.....", ".....KWWWWK.....", ".....KWWWWK.....", "......KKKK......", "................", "..KKKK....KKKK..", ".KRRRRK..KRRRRK.", ".KKWWKK..KKWWKK.", ".KWWWWK..KWWWWK.", ".KWWWWK..KWWWWK.", "..KKKK....KKKK..", "................", "................" } },
        { "Open PC", 0xFF5C6C84, 0xFF2C3646, {
            "................", ".KKKKKKKKKKKKKK.", ".KWWWWWWWWWWWWK.", ".KWDDDDDDDDDDWK.", ".KWDDDRRRRDDDWK.", ".KWDDRRRRRRDDWK.", ".KWDDKKWWKKDDWK.", ".KWDDWWWWWWDDWK.", ".KWDDDWWWWDDDWK.", ".KWDDDDDDDDDDWK.", ".KWWWWWWWWWWWWK.", ".KKKKKKKKKKKKKK.", "......KWWK......", "....KKWWWWKK....", "....KKKKKKKK....", "................" } },
        { "Repel", 0xFF9656D2, 0xFF60348C, {
            "................", "........W..W....", ".......KKK..W.W.", "......KWWK.W....", "......KKKK...W..", ".....KKKKKK.....", "....KWWWWWWK....", "....KWWWWWWK....", "....KDDDDDDK....", "....KDYYYYDK....", "....KDYDDYDK....", "....KDYYYYDK....", "....KDDDDDDK....", "....KWWWWWWK....", ".....KKKKKK.....", "................" } },
        { "Encounters", 0xFFEC8828, 0xFFAA5A14, {
            "................", "......KKKK......", "......KWWK......", "..KKKKKKKKKKKK..", ".KWWWWWWWWWWWWK.", ".KWKKKKKKKKKKWK.", ".KWKWWWKKWWWKWK.", ".KWKKKWKKKKWKWK.", ".KWKKWWKKKWKKWK.", ".KWKKKWKKKWKKWK.", ".KWKWWWKKKWKKWK.", ".KWKKKKKKKKKKWK.", ".KWWWWWWWWWWWWK.", "..KKKKKKKKKKKK..", "................", "................" } },
        { "Reset", 0xFFDE3A3A, 0xFF961E1E, {
            "................", "......WWWW..W...", "....WWWWWWWWWW..", "...WWW....WWWW..", "..WWW......WWW..", "..WW......WWWW..", ".WWW............", ".WW.............", ".WW.........WW..", ".WWW.......WWW..", "..WW.......WW...", "..WWW.....WWW...", "...WWWW.WWWW....", "....WWWWWWW.....", "......WWW.......", "................" } },
        { "Map", 0xFF4CA854, 0xFF286E32, {
            "................", "...........RR...", "..........RRRR..", ".KKKK.KKKRRWWRR.", ".KLLKKKLLRRWWRR.", ".KLLLKLLLKRRRRK.", ".KLDLKLLLKLRRLK.", ".KLDDKLDLKLLRLK.", ".KLLDKLDDKDLLLK.", ".KLLLKLLDKDDLLK.", ".KLLLKLLLKLDLLK.", ".KLLLKKKLKKLLLK.", ".KKKK..KKK.KKKK.", "................", "................", "................" } },
        { "Pok\xC3\xA9" "dex", 0xFFF4F4F4, 0xFFC8C8C8, {
            "................", ".....KKKKKK.....", "...KKRRRRRRKK...", "..KRRRRRRRRWRK..", "..KRRRRRRRRRWK..", ".KRRRRRRRRRRRRK.", ".KRRRRKKKKRRRRK.", ".KKKKKKWWKKKKKK.", ".KWWWWKKKKWWWWK.", ".KWWWWWWWWWWWWK.", "..KWWWWWWWWWWK..", "..KGWWWWWWWWGK..", "...KKGGGGGGKK...", ".....KKKKKK.....", "................", "................" } },
        { "Bag", 0xFFC48C4C, 0xFF825628, {
            "................", "......KKKK......", ".....KK..KK.....", "....KK....KK....", "..KKKKKKKKKKKK..", ".KWWWWWWWWWWWWK.", ".KWLLLLLLLLLLWK.", ".KKKKKKYYKKKKKK.", ".KDDDDKYYKDDDDK.", ".KDDDDDKKDDDDDK.", ".KDDDDDDDDDDDDK.", ".KDDDDDDDDDDDDK.", ".KDDDDDDDDDDDDK.", "..KKKKKKKKKKKK..", "................", "................" } },
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

// Where it goes: bottom right; centred (centre) when a phone's buttons are at the sides. style: NAV STYLE,
// 0 MODERN, the watch (graphite case, glass face); 1 CLASSIC, the red watch; 2 SIMPLE, a glass card; 3 WHEEL,
// the tiles alone in a ring.
inline Geo Layout(int outW, int outH, bool centre, int style = 0)
{
    Geo g;
    if (style == 3)
    {
        // WHEEL: the apps' tiles in a ring round the middle of the picture, the first at the top.
        g.k = std::max(0.5f, std::min(outW / 1280.0f, outH / 720.0f));
        const float k = g.k, R = 200 * k, ts = 66 * k;
        const int cx = outW / 2, cy = outH / 2;
        g.w = g.h = (int)std::lround(2 * R + ts + 20 * k); g.x = cx - g.w / 2; g.y = cy - g.h / 2;
        g.sx0 = cx; g.sy0 = cy; g.sx1 = g.sy1 = (int)std::lround(98 * k); // (the middle and its radius)
        for (int i = 0; i < APPS; i++)
        {
            const float a = -1.5707963f + i * 6.2831853f / APPS;
            g.tile[i][0] = (int)std::lround(cx + R * std::cos(a) - ts / 2); g.tile[i][1] = (int)std::lround(cy + R * std::sin(a) - ts / 2);
            g.tile[i][2] = g.tile[i][3] = (int)std::lround(ts);
        }
        return g;
    }
    if (style == 2)
    {
        g.k = std::max(0.5f, std::min(outW / 1280.0f, outH / 720.0f));
        const float k = g.k;
        g.w = (int)std::lround(360 * k); g.h = (int)std::lround(392 * k);
        g.x = centre ? (outW - g.w) / 2 : outW - g.w - (int)std::lround(56 * k);
        g.y = centre ? (outH - g.h) / 2 : outH - g.h - (int)std::lround(44 * k);
        g.sx0 = g.x + (int)std::lround(26 * k); g.sy0 = g.y + (int)std::lround(36 * k);
        g.sx1 = g.x + g.w - (int)std::lround(26 * k); g.sy1 = g.y + g.h - (int)std::lround(26 * k);
        const float gw = (g.w - 40 * k) / 3.0f, gh = 104 * k, ts = 60 * k;
        for (int i = 0; i < APPS; i++)
        {
            const float cx = g.x + 20 * k + (i % 3) * gw, cy = g.y + 62 * k + (i / 3) * gh;
            g.tile[i][0] = (int)std::lround(cx + gw / 2 - ts / 2); g.tile[i][1] = (int)std::lround(cy + 4 * k);
            g.tile[i][2] = (int)std::lround(ts); g.tile[i][3] = (int)std::lround(ts);
        }
        return g;
    }
    if (style == 1)
    {
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
    g.k = std::max(0.5f, std::min(outW / 1280.0f, outH / 720.0f));
    const float k = g.k;
    g.w = (int)std::lround(372 * k); g.h = (int)std::lround(404 * k);
    g.x = centre ? (outW - g.w) / 2 : outW - g.w - (int)std::lround(50 * k);
    g.y = centre ? (outH - g.h) / 2 : outH - g.h - (int)std::lround(70 * k);
    // (sx/sy: the glass face, set into the case.)
    g.sx0 = g.x + (int)std::lround(16 * k); g.sy0 = g.y + (int)std::lround(16 * k);
    g.sx1 = g.x + g.w - (int)std::lround(16 * k); g.sy1 = g.y + g.h - (int)std::lround(16 * k);
    const float gw = (g.sx1 - g.sx0 - 40 * k) / 3.0f, gh = 100 * k, ts = 60 * k;
    for (int i = 0; i < APPS; i++)
    {
        const float cx = g.sx0 + 20 * k + (i % 3) * gw, cy = g.sy0 + 58 * k + (i / 3) * gh;
        g.tile[i][0] = (int)std::lround(cx + gw / 2 - ts / 2); g.tile[i][1] = (int)std::lround(cy);
        g.tile[i][2] = (int)std::lround(ts); g.tile[i][3] = (int)std::lround(ts);
    }
    return g;
}

// WHEEL: the app a direction points at (x right, y down), -1 when it points nowhere much.
inline int WheelPick(float x, float y)
{
    if (x * x + y * y < 0.45f * 0.45f) return -1;
    float a = std::atan2(y, x) + 1.5707963f; // (0 at the top, clockwise)
    while (a < 0) a += 6.2831853f;
    return (int)std::lround(a / (6.2831853f / APPS)) % APPS;
}

// ENCOUNTERS: the count itself on the counter's little screen (its picture's rows 5 to 11), so the icon shows
// how many, not a made-up number.
inline void CountOnIcon(Canvas& c, int ox, int oy, int px, int count)
{
    using namespace SettingsArt;
    const int x = ox + 3 * px, y = oy + 5 * px, w = 10 * px, h = 7 * px;
    c.Fill(x, y, w, h, 0xFF1E1E24);
    const std::string t = count > 9999 ? std::string("9999+") : std::to_string(std::max(0, count));
    int fs = std::max(6, (int)std::lround(px * 6.2f));
    while (fs > 6 && W(t, fs, true, false) > w - px) fs--;
    T(c, x + (w - W(t, fs, true, false)) / 2, y + h / 2 + (int)std::lround(fs * 0.36f), t, fs, 0xFFFFFFFF, true, false);
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
// NAV STYLE CLASSIC: the first Nav, a red watch with a light screen, its words in capitals.
inline std::string Upper(std::string t)
{
    for (char& ch : t) if (ch >= 'a' && ch <= 'z') ch -= 32;
    for (size_t p; (p = t.find("\xC3\xA9")) != std::string::npos;) t.replace(p, 2, "\xC3\x89");
    return t;
}
inline void DrawClassic(Canvas& c, const Geo& g, const State& s, const bool dim[APPS], const std::string label[APPS], const std::string& hint)
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
                const uint32_t lite = 0xFF000000u | ((((a.col >> 16) & 0xFF) * 45 / 100 + 140) << 16) | ((((a.col >> 8) & 0xFF) * 45 / 100 + 140) << 8) | ((a.col & 0xFF) * 45 / 100 + 140);
                uint32_t col = ch == 'W' ? 0xFFFFFFFF : ch == 'Y' ? 0xFFFFD640 : ch == 'R' ? 0xFFE23834 : ch == 'K' ? 0xFF1E1E24 : ch == 'G' ? 0xFFB0B0B8 : ch == 'L' ? lite : a.dark;
                c.Fill(ox + q * px, oy + r * px, px, px, fade(col));
            }
        if (i == COUNT && !faded) CountOnIcon(c, ox, oy, px, s.count);
        const std::string name = Upper(label && !label[i].empty() ? label[i] : std::string(a.label));
        const int tw = W(name, ls, true, false);
        T(c, x + sz / 2 - tw / 2, y + sz + K(20), name, ls, faded ? 0x80344038 : ink, true, false);
    }
    if (!hint.empty())
    {
        const int hs = std::max(9, K(14)), hw = W(Upper(hint), hs, true, false) + K(28);
        const int hx = g.x + g.w / 2 - hw / 2, hy = std::min(c.h - K(34), g.y + g.h + K(10));
        Glass::Round(c, hx, hy, hw, K(26), 13 * k, 0xD8101418);
        T(c, hx + K(14), hy + K(18), Upper(hint), hs, 0xFFEBF0F5, true, false);
    }
}

// NAV STYLE SIMPLE: a plain glass card, as the companion window's, no case.
inline void DrawSimple(Canvas& c, const Geo& g, const State& s, const bool dim[APPS], const std::string label[APPS], const std::string& hint)
{
    using namespace SettingsArt;
    const float k = g.k;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    // (The watch: dark glass as the companion window's, its light rim, the crown on its side.)
    Glass::Round(c, g.x + K(4), g.y + K(10), g.w, g.h, 44 * k, 0x50000000);
    Glass::Round(c, g.x, g.y, g.w, g.h, 44 * k, 0xE0101826);
    Glass::Rim(c, g.x, g.y, g.w, g.h, 44 * k, std::max(1.2f, 1.8f * k));
    Glass::Round(c, g.x + g.w - K(3), g.y + K(150), K(12), K(64), 6 * k, 0x60FFFFFF);
    char clock[8] = "--:--";
    const time_t now = time(nullptr);
    if (const tm* t = localtime(&now)) snprintf(clock, sizeof(clock), "%02d:%02d", t->tm_hour, t->tm_min);
    const int fs = std::max(9, K(16));
    T(c, g.x + K(26), g.y + K(36), "FullDex Nav", fs, Menu::GlassText, true, false);
    T(c, g.x + g.w - K(26) - W(clock, fs, true, false), g.y + K(36), clock, fs, Menu::GlassHint, true, false);
    c.Fill(g.x + K(22), g.y + K(48), g.w - K(44), 1, 0x32FFFFFF);
    const uint32_t ink = Menu::GlassText;
    const int ls = std::max(8, K(13));
    for (int i = 0; i < APPS; i++)
    {
        const App& a = Get(i);
        const int* t = g.tile[i];
        const int x = t[0], y = t[1], sz = t[2];
        const bool faded = dim && dim[i];
        // (Faded: an app this game cannot do yet, the glass showing through.)
        auto fade = [&](uint32_t col) {
            if (!faded) return col;
            auto mix = [](uint32_t a, uint32_t b, int sh) { return (((a >> sh) & 0xFF) * 38 + ((b >> sh) & 0xFF) * 62) / 100; };
            const uint32_t bg = 0xFF161E2A; // (the glass's own colour: opaque, so no hole to the game)
            return 0xFF000000u | (mix(col, bg, 16) << 16) | (mix(col, bg, 8) << 8) | mix(col, bg, 0);
        };
        if (i == s.sel) Glass::Round(c, x - K(6), y - K(6), sz + K(12), sz + K(12), 20 * k, 0, 0xE6FFFFFF, 3 * k);
        Glass::Round(c, x, y + K(3), sz, sz, 14 * k, fade(a.dark));
        Glass::Round(c, x, y, sz, sz, 14 * k, fade(a.col));
        const int px = std::max(1, (sz - K(12)) / 16), ox = x + (sz - px * 16) / 2, oy = y + (sz - px * 16) / 2;
        for (int r = 0; r < 16; r++)
            for (int q = 0; q < 16; q++)
            {
                const char ch = a.rows[r][q];
                if (ch == '.') continue;
                const uint32_t lite = 0xFF000000u | ((((a.col >> 16) & 0xFF) * 45 / 100 + 140) << 16) | ((((a.col >> 8) & 0xFF) * 45 / 100 + 140) << 8) | ((a.col & 0xFF) * 45 / 100 + 140);
                uint32_t col = ch == 'W' ? 0xFFFFFFFF : ch == 'Y' ? 0xFFFFD640 : ch == 'R' ? 0xFFE23834 : ch == 'K' ? 0xFF1E1E24 : ch == 'G' ? 0xFFB0B0B8 : ch == 'L' ? lite : a.dark;
                c.Fill(ox + q * px, oy + r * px, px, px, fade(col));
            }
        if (i == COUNT && !faded) CountOnIcon(c, ox, oy, px, s.count);
        const std::string name = label && !label[i].empty() ? label[i] : std::string(a.label);
        const int tw = W(name, ls, true, false);
        T(c, x + sz / 2 - tw / 2, y + sz + K(22), name, ls, faded ? 0x70F2F5F9 : ink, true, false);
    }
    if (!hint.empty())
    {
        const int hs = std::max(9, K(14)), hw = W(hint, hs, true, true) + K(28);
        const int hx = g.x + g.w / 2 - hw / 2, hy = std::min(c.h - K(34), g.y + g.h + K(10));
        Glass::Round(c, hx, hy, hw, K(26), 13 * k, 0xD8101418);
        T(c, hx + K(14), hy + K(18), hint, hs, 0xFFEBF0F5, true, true);
    }
}

// NAV STYLE WHEEL: only the apps' tiles, in a ring; the chosen one's name in the middle.
inline void DrawWheel(Canvas& c, const Geo& g, const State& s, const bool dim[APPS], const std::string label[APPS], const std::string& hint)
{
    using namespace SettingsArt;
    const float k = g.k;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const int cx = g.sx0, cy = g.sy0, r = g.sx1;
    Glass::Round(c, cx - r, cy - r, 2 * r, 2 * r, (float)r, 0xD8101826, 0x46FFFFFF, 2 * k);
    for (int i = 0; i < APPS; i++)
    {
        const App& a = Get(i);
        const int* t = g.tile[i];
        const int x = t[0], y = t[1], sz = t[2];
        const bool faded = dim && dim[i];
        auto fade = [&](uint32_t col) {
            if (!faded) return col;
            auto mix = [](uint32_t a, uint32_t b, int sh) { return (((a >> sh) & 0xFF) * 38 + ((b >> sh) & 0xFF) * 62) / 100; };
            const uint32_t bg = 0xFF121A24;
            return 0xFF000000u | (mix(col, bg, 16) << 16) | (mix(col, bg, 8) << 8) | mix(col, bg, 0);
        };
        const int grow = i == s.sel ? K(8) : 0;
        const int X = x - grow / 2, Y = y - grow / 2, S = sz + grow;
        Glass::Round(c, X + K(2), Y + K(8), S, S, 18 * k, 0x60000000);
        if (i == s.sel) Glass::Round(c, X - K(6), Y - K(6), S + K(12), S + K(12), 22 * k, 0, 0xF0FFFFFF, 3 * k);
        Glass::Round(c, X, Y + K(3), S, S, 16 * k, fade(a.dark));
        Glass::Round(c, X, Y, S, S, 16 * k, fade(a.col));
        const int px = std::max(1, (S - K(12)) / 16), ox = X + (S - px * 16) / 2, oy = Y + (S - px * 16) / 2;
        for (int rr = 0; rr < 16; rr++)
            for (int q = 0; q < 16; q++)
            {
                const char ch = a.rows[rr][q];
                if (ch == '.') continue;
                const uint32_t lite = 0xFF000000u | ((((a.col >> 16) & 0xFF) * 45 / 100 + 140) << 16) | ((((a.col >> 8) & 0xFF) * 45 / 100 + 140) << 8) | ((a.col & 0xFF) * 45 / 100 + 140);
                const uint32_t col = ch == 'W' ? 0xFFFFFFFF : ch == 'Y' ? 0xFFFFD640 : ch == 'R' ? 0xFFE23834 : ch == 'K' ? 0xFF1E1E24 : ch == 'G' ? 0xFFB0B0B8 : ch == 'L' ? lite : a.dark;
                c.Fill(ox + q * px, oy + rr * px, px, px, fade(col));
            }
        if (i == COUNT && !faded) CountOnIcon(c, ox, oy, px, s.count);
    }
    // (The middle: the chosen app's name, and the clock.)
    const int sel = std::clamp(s.sel, 0, APPS - 1);
    const std::string name = label && !label[sel].empty() ? label[sel] : std::string(Get(sel).label);
    const int fs = std::max(10, K(20)), cs = std::max(8, K(13));
    T(c, cx - W(name, fs, true, false) / 2, cy + K(7), name, fs, (dim && dim[sel]) ? 0x90F2F5F9 : Menu::GlassText, true, false);
    char clock[8] = "--:--";
    const time_t now = time(nullptr);
    if (const tm* tt = localtime(&now)) snprintf(clock, sizeof(clock), "%02d:%02d", tt->tm_hour, tt->tm_min);
    T(c, cx - W(clock, cs, true, false) / 2, cy - K(30), clock, cs, Menu::GlassHint, true, false);
    if (!hint.empty())
    {
        const int hs = std::max(9, K(14)), hw = W(hint, hs, true, true) + K(28);
        const int hx = cx - hw / 2, hy = c.h - K(40);
        Glass::Round(c, hx, hy, hw, K(26), 13 * k, 0xD8101418);
        T(c, hx + K(14), hy + K(18), hint, hs, 0xFFEBF0F5, true, true);
    }
}

inline void Draw(Canvas& c, const Geo& g, const State& s, const bool dim[APPS], const std::string label[APPS], const std::string& hint, int style = 0)
{
    if (style == 3) { DrawWheel(c, g, s, dim, label, hint); return; }
    if (style == 1) { DrawClassic(c, g, s, dim, label, hint); return; }
    if (style == 2) { DrawSimple(c, g, s, dim, label, hint); return; }
    using namespace SettingsArt;
    const float k = g.k;
    auto K = [&](float v) { return (int)std::lround(v * k); };
    // (The watch: its strap above and below, a graphite case, the red crown and a button on its side, and
    // the dark glass face set into the case, where the apps are.)
    const int sw = K(230), sx = g.x + (g.w - sw) / 2, groove = std::max(1, K(2));
    Glass::Round(c, sx, g.y - K(70), sw, K(110), 24 * k, 0xFF22262E);
    Glass::Round(c, sx, g.y + g.h - K(40), sw, K(130), 24 * k, 0xFF22262E);
    for (int yy = g.y - K(58); yy < g.y; yy += std::max(4, K(14))) c.Fill(sx + K(18), yy, sw - K(36), groove, 0xFF30363F);
    for (int yy = g.y + g.h + K(4); yy < std::min(c.h, g.y + g.h + K(86)); yy += std::max(4, K(14))) c.Fill(sx + K(18), yy, sw - K(36), groove, 0xFF30363F);
    Glass::Round(c, g.x + K(4), g.y + K(10), g.w, g.h, 70 * k, 0x50000000);
    Glass::Round(c, g.x, g.y, g.w, g.h, 70 * k, 0xFF40444E, 0xFF969CA8, 2 * k);
    {
        // (The case's shading, light at the top to dark at the bottom, inside its rounded edge.)
        const float r = 70 * k - 3;
        for (int y = g.y + K(3); y < g.y + g.h - K(3); y++)
        {
            const float t = (y - g.y) / (float)std::max(1, g.h), dy = (float)std::min(y - g.y, g.y + g.h - 1 - y);
            const int inset = dy < r ? (int)std::ceil(r - std::sqrt(std::max(0.0f, r * r - (r - dy) * (r - dy)))) : 0;
            const uint32_t col = 0xFF000000u | ((uint32_t)(92 - 56 * t) << 16) | ((uint32_t)(98 - 58 * t) << 8) | (uint32_t)(110 - 62 * t);
            c.Fill(g.x + inset + K(3), y, g.w - 2 * inset - K(6), 1, col);
        }
    }
    Glass::Round(c, g.x + K(18), g.y + K(4), g.w - K(36), K(6), 3 * k, 0x78B4BAC4);
    Glass::Round(c, g.x + g.w - K(4), g.y + K(110), K(20), K(60), 8 * k, 0xFFCE2E30, 0xFF141418, 2 * k);
    for (int yy = g.y + K(116); yy < g.y + K(166); yy += std::max(3, K(6))) c.Fill(g.x + g.w + K(3), yy, K(11), 1, 0x60000000);
    Glass::Round(c, g.x + g.w - K(3), g.y + K(210), K(12), K(60), 5 * k, 0xFF464C56, 0xFF141418, 2 * k);
    Glass::Round(c, g.sx0 - K(3), g.sy0 - K(3), g.sx1 - g.sx0 + K(6), g.sy1 - g.sy0 + K(6), 56 * k, 0xFF080A0E);
    Glass::Round(c, g.sx0, g.sy0, g.sx1 - g.sx0, g.sy1 - g.sy0, 54 * k, 0xF2121A24);
    Glass::Round(c, g.sx0, g.sy0, g.sx1 - g.sx0, g.sy1 - g.sy0, 54 * k, 0, 0x46FFFFFF, 2 * k);
    char clock[8] = "--:--";
    const time_t now = time(nullptr);
    if (const tm* t = localtime(&now)) snprintf(clock, sizeof(clock), "%02d:%02d", t->tm_hour, t->tm_min);
    const int fs = std::max(9, K(16));
    T(c, g.sx0 + K(24), g.sy0 + K(32), "FullDex Nav", fs, Menu::GlassText, true, false);
    T(c, g.sx1 - K(24) - W(clock, fs, true, false), g.sy0 + K(32), clock, fs, Menu::GlassHint, true, false);
    c.Fill(g.sx0 + K(20), g.sy0 + K(44), g.sx1 - g.sx0 - K(40), 1, 0x32FFFFFF);
    const uint32_t ink = Menu::GlassText;
    const int ls = std::max(8, K(13));
    for (int i = 0; i < APPS; i++)
    {
        const App& a = Get(i);
        const int* t = g.tile[i];
        const int x = t[0], y = t[1], sz = t[2];
        const bool faded = dim && dim[i];
        // (Faded: an app this game cannot do yet, the glass showing through.)
        auto fade = [&](uint32_t col) {
            if (!faded) return col;
            auto mix = [](uint32_t a, uint32_t b, int sh) { return (((a >> sh) & 0xFF) * 38 + ((b >> sh) & 0xFF) * 62) / 100; };
            const uint32_t bg = 0xFF121A24; // (the face's own colour: opaque, so no hole to the game)
            return 0xFF000000u | (mix(col, bg, 16) << 16) | (mix(col, bg, 8) << 8) | mix(col, bg, 0);
        };
        if (i == s.sel) Glass::Round(c, x - K(6), y - K(6), sz + K(12), sz + K(12), 20 * k, 0, 0xE6FFFFFF, 3 * k);
        Glass::Round(c, x, y + K(3), sz, sz, 14 * k, fade(a.dark));
        Glass::Round(c, x, y, sz, sz, 14 * k, fade(a.col));
        const int px = std::max(1, (sz - K(12)) / 16), ox = x + (sz - px * 16) / 2, oy = y + (sz - px * 16) / 2;
        for (int r = 0; r < 16; r++)
            for (int q = 0; q < 16; q++)
            {
                const char ch = a.rows[r][q];
                if (ch == '.') continue;
                const uint32_t lite = 0xFF000000u | ((((a.col >> 16) & 0xFF) * 45 / 100 + 140) << 16) | ((((a.col >> 8) & 0xFF) * 45 / 100 + 140) << 8) | ((a.col & 0xFF) * 45 / 100 + 140);
                uint32_t col = ch == 'W' ? 0xFFFFFFFF : ch == 'Y' ? 0xFFFFD640 : ch == 'R' ? 0xFFE23834 : ch == 'K' ? 0xFF1E1E24 : ch == 'G' ? 0xFFB0B0B8 : ch == 'L' ? lite : a.dark;
                c.Fill(ox + q * px, oy + r * px, px, px, fade(col));
            }
        if (i == COUNT && !faded) CountOnIcon(c, ox, oy, px, s.count);
        const std::string name = label && !label[i].empty() ? label[i] : std::string(a.label);
        const int tw = W(name, ls, true, false);
        T(c, x + sz / 2 - tw / 2, y + sz + K(22), name, ls, faded ? 0x70F2F5F9 : ink, true, false);
    }
    if (!hint.empty())
    {
        const int hs = std::max(9, K(14)), hw = W(hint, hs, true, true) + K(28);
        const int hx = g.x + g.w / 2 - hw / 2, hy = std::min(c.h - K(34), g.y + g.h + K(10));
        Glass::Round(c, hx, hy, hw, K(26), 13 * k, 0xD8101418);
        T(c, hx + K(14), hy + K(18), hint, hs, 0xFFEBF0F5, true, true);
    }
}

// The counter on the picture while you play (COUNTER, A: on): a small pill in the top right.
inline void DrawCount(Canvas& c, int outW, int outH, int count)
{
    using namespace SettingsArt;
    const float k = std::max(0.5f, std::min(outW / 1280.0f, outH / 720.0f));
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const std::string t = std::to_string(count) + (count == 1 ? " encounter" : " encounters");
    const int fs = std::max(9, K(16)), w = W(t, fs, true, false) + K(40);
    const int x = outW - w - K(16), y = K(14);
    Glass::Round(c, x, y, w, K(30), 15 * k, 0xD8101418);
    Glass::Round(c, x + K(6), y + K(6), K(18), K(18), 5 * k, 0xFFEC8828);
    T(c, x + K(30), y + K(21), t, fs, 0xFFFFFFFF, true, false);
}
} // namespace FdNav
