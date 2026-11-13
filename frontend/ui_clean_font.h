// The pause menu's clean text (Menu::Clean, ui.h): Noto Sans drawn smooth at any size with
// stb_truetype, into the Canvas. The pixel font (UiText) stays for the game list, the companion
// panels and the Retro look; Japanese and Korean menus keep it too (no CJK in this cut of Noto).
// Included by ui.h after Canvas and glass.h.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "ui_clean_font_data.h"
#include "launcher_cjk.h"

namespace CleanFont
{
struct Face
{
    stbtt_fontinfo info{};
    bool ok = false;
    int ascent = 0, descent = 0, gap = 0;
    float scale = 1.0f; // (the font's own fit beside Noto Sans)
};

// The face drawn with: 0 Noto Sans (built in here), 1 and up a font FontSource gives (the launcher's
// own, launcher_fonts.h); and how much bigger or smaller (the launcher's Text Size). The launcher sets
// them while it draws and puts them back; the pause menu keeps Noto Sans.
inline int& FontId() { static int id = 0; return id; }
inline float& SizeScale() { static float s = 1.0f; return s; }
typedef const unsigned char* (*FontSourceFn)(int id, bool bold, float* scale);
inline FontSourceFn& FontSource() { static FontSourceFn f = nullptr; return f; }
inline Face& GetFace(int id, bool bold)
{
    static std::map<int, Face> faces;
    const int key = id * 2 + (bold ? 1 : 0);
    auto it = faces.find(key);
    if (it != faces.end()) return it->second;
    Face f;
    const unsigned char* data = id > 0 && FontSource() ? FontSource()(id, bold, &f.scale) : nullptr;
    if (!data) { data = bold ? CleanFontData::SemiBold : CleanFontData::Regular; f.scale = 1.0f; }
    f.ok = stbtt_InitFont(&f.info, data, stbtt_GetFontOffsetForIndex(data, 0)) != 0;
    if (f.ok) stbtt_GetFontVMetrics(&f.info, &f.ascent, &f.descent, &f.gap);
    return faces.emplace(key, f).first->second;
}
inline Face& Get(bool bold) { return GetFace(FontId(), bold); }

// Japanese and Korean letters Noto Sans lacks: the launcher's own cut of Noto Sans JP and KR (launcher_cjk.h).
// Cjk(): the launcher sets it while it draws, so its Japanese and Korean text is drawn smooth too.
inline bool& Cjk() { static bool on = false; return on; }
inline Face& CjkFace(uint32_t cp)
{
    static Face jp, kr;
    static bool made = false;
    if (!made)
    {
        made = true;
        jp.ok = stbtt_InitFont(&jp.info, kCjkJp, stbtt_GetFontOffsetForIndex(kCjkJp, 0)) != 0;
        if (jp.ok) stbtt_GetFontVMetrics(&jp.info, &jp.ascent, &jp.descent, &jp.gap);
        kr.ok = stbtt_InitFont(&kr.info, kCjkKr, stbtt_GetFontOffsetForIndex(kCjkKr, 0)) != 0;
        if (kr.ok) stbtt_GetFontVMetrics(&kr.info, &kr.ascent, &kr.descent, &kr.gap);
    }
    const bool hangul = (cp >= 0x1100 && cp <= 0x11FF) || (cp >= 0x3130 && cp <= 0x318F) || (cp >= 0xAC00 && cp <= 0xD7AF);
    if (hangul && kr.ok && stbtt_FindGlyphIndex(&kr.info, (int)cp)) return kr;
    return jp;
}

// Next code point of UTF-8 text (bad bytes are taken as Latin-1).
inline uint32_t Next(const std::string& s, size_t& i)
{
    const unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (!extra || i + extra > s.size()) return c;
    uint32_t cp = c & (0x3F >> extra);
    for (int k = 0; k < extra; k++)
    {
        const unsigned char d = (unsigned char)s[i];
        if ((d & 0xC0) != 0x80) return c;
        cp = cp << 6 | (d & 0x3F);
        i++;
    }
    return cp;
}

struct Glyph
{
    int w = 0, h = 0, x0 = 0, y0 = 0; // bitmap size and its offset from the pen at the baseline
    float advance = 0;
    std::vector<uint8_t> alpha;
};

// px: the font's size in pixels (ascent to descent).
inline const Glyph& Get(bool bold, int px, uint32_t cp)
{
    static std::map<uint64_t, Glyph> cache;
    const int id = FontId(), size = (int)(SizeScale() * 100 + 0.5f);
    const uint64_t key = (uint64_t)bold << 63 | (uint64_t)(id & 0x7F) << 56 | (uint64_t)(size & 0xFF) << 48 | (uint64_t)(px & 0xFFFF) << 32 | cp;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    Glyph g;
    Face* fp = &GetFace(id, bold);
    if (id > 0 && fp->ok && !stbtt_FindGlyphIndex(&fp->info, (int)cp)) fp = &GetFace(0, bold); // (not in that font: Noto Sans's)
    if (cp >= 0x2010 && fp->ok && !stbtt_FindGlyphIndex(&fp->info, (int)cp)) fp = &CjkFace(cp); // (nor there: Japanese or Korean)
    Face& f = *fp;
    if (f.ok)
    {
        const float scale = stbtt_ScaleForPixelHeight(&f.info, px * f.scale * SizeScale());
        int adv = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&f.info, (int)cp, &adv, &lsb);
        g.advance = adv * scale;
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&f.info, (int)cp, scale, scale, &x0, &y0, &x1, &y1);
        g.w = x1 - x0; g.h = y1 - y0; g.x0 = x0; g.y0 = y0;
        if (g.w > 0 && g.h > 0)
        {
            g.alpha.assign((size_t)g.w * g.h, 0);
            stbtt_MakeCodepointBitmap(&f.info, g.alpha.data(), g.w, g.h, g.w, scale, scale, (int)cp);
        }
    }
    return cache.emplace(key, std::move(g)).first->second;
}

inline bool Has(bool bold, uint32_t cp) { Face& f = Get(bold); return f.ok && stbtt_FindGlyphIndex(&f.info, (int)cp) != 0; }

inline float Kern(bool bold, int px, uint32_t a, uint32_t b)
{
    Face& f = Get(bold);
    return f.ok ? stbtt_GetCodepointKernAdvance(&f.info, (int)a, (int)b) * stbtt_ScaleForPixelHeight(&f.info, px * f.scale * SizeScale()) : 0;
}

// The PlayStation button marks the hints use (\x01 cross, \x02 circle, \x03 square, \x04 triangle):
// drawn as shapes the size of a capital letter.
inline bool IsMark(uint32_t cp) { return cp >= 1 && cp <= 4; }
// (Sized and placed from the font's own capital height, so a mark sits on the line like a letter.)
inline float CapHeight(int px, bool bold);
inline float MarkAdvance(int px, bool bold) { return CapHeight(px, bold) * 1.32f; }

inline int Width(const std::string& s, int px, bool bold = false)
{
    float w = 0;
    uint32_t prev = 0;
    for (size_t i = 0; i < s.size();)
    {
        uint32_t cp = Next(s, i);
        if (cp == 5) cp = 0xC9; // (the pixel font's É)
        if (IsMark(cp)) { w += MarkAdvance(px, bold); prev = 0; continue; }
        if (prev) w += Kern(bold, px, prev, cp);
        w += Get(bold, px, cp).advance;
        prev = cp;
    }
    return (int)(w + 0.5f);
}

inline float CapHeight(int px, bool bold)
{
    const Glyph& g = Get(bold, px, 'H');
    return g.alpha.empty() ? px * 0.52f : (float)-g.y0;
}

// Draws s with its baseline at y.
inline void Draw(Canvas& c, int x, int y, const std::string& s, int px, uint32_t argb, bool bold = false)
{
    float pen = (float)x;
    uint32_t prev = 0;
    for (size_t i = 0; i < s.size();)
    {
        uint32_t cp = Next(s, i);
        if (cp == 5) cp = 0xC9;
        if (IsMark(cp))
        {
            const float cap = CapHeight(px, bold), size = cap * 0.5f, cx = pen + cap * 0.6f, cy = y - cap * 0.5f;
            const int icon = cp; // drawn with glass.h's shapes
            const int x0 = (int)(cx - size) - 2, y0 = (int)(cy - size) - 2, x1 = (int)(cx + size) + 3, y1 = (int)(cy + size) + 3;
            Glass::Mark(c, x0, y0, x1, y1);
            for (int py = std::max(0, y0); py < std::min(c.h, y1); py++)
                for (int qx = std::max(0, x0); qx < std::min(c.w, x1); qx++)
                {
                    const float u = (qx + 0.5f - cx) / size, v = (py + 0.5f - cy) / size, t = 0.2f;
                    float d = 1;
                    if (icon == 1) d = std::min(Glass::Segment(u, v, -0.8f, -0.8f, 0.8f, 0.8f, t), Glass::Segment(u, v, -0.8f, 0.8f, 0.8f, -0.8f, t));
                    else if (icon == 2) d = std::fabs(Glass::Circle(u, v, 0, 0, 0.85f)) - t;
                    else if (icon == 3) d = std::fabs(Glass::Box(u, v, 0, 0, 0.8f, 0.8f, 0.05f)) - t;
                    else d = std::min(std::min(Glass::Segment(u, v, 0, -0.85f, -0.9f, 0.7f, t), Glass::Segment(u, v, 0, -0.85f, 0.9f, 0.7f, t)), Glass::Segment(u, v, -0.9f, 0.7f, 0.9f, 0.7f, t));
                    const float cover = std::clamp(0.5f - d * size, 0.0f, 1.0f);
                    if (cover > 0) { uint32_t& p = c.px[(size_t)py * c.w + qx]; p = Glass::Over(p, argb, (int)(cover * 255)); }
                }
            pen += MarkAdvance(px, bold);
            prev = 0;
            continue;
        }
        if (prev) pen += Kern(bold, px, prev, cp);
        const Glyph& g = Get(bold, px, cp);
        const int gx = (int)(pen + 0.5f) + g.x0, gy = y + g.y0;
        if (!g.alpha.empty())
        {
            Glass::Mark(c, gx, gy, gx + g.w, gy + g.h);
            for (int r = 0; r < g.h; r++)
            {
                const int py = gy + r;
                if (py < 0 || py >= c.h) continue;
                for (int k = 0; k < g.w; k++)
                {
                    const int qx = gx + k;
                    if (qx < 0 || qx >= c.w) continue;
                    const uint8_t a = g.alpha[(size_t)r * g.w + k];
                    if (a) { uint32_t& p = c.px[(size_t)py * c.w + qx]; p = Glass::Over(p, argb, a); }
                }
            }
        }
        pen += g.advance;
        prev = cp;
    }
}

// The menus' labels are written in capitals (they suit the pixel font). With the clean font they
// read better in title case: each word capitalised, short joining words small, and the capitals
// kept where they belong (FPS, HUD, GBA, button names...). UTF-8 letters of Latin-1 and Latin
// Extended-A are lowered too, so translated labels change the same way.
inline std::string Title(const std::string& s)
{
    static const char* const keep[] = { "FPS", "HD", "HUD", "GBA", "DS", "3DS", "PC", "RA", "3D", "2D", "USB", "ID", "OK", "A", "B", "X", "Y", "L", "R",
                                        "ZL", "ZR", "PP", "HP", "IV", "IVS", "EV", "EVS", "TM", "HM", "HMS", "TMS", "NDS", "SD", "UI", "LCD", "CRT",
                                        "MB", "GB", "KB", "AI", "TV", "CPU", "GPU", "SFX", "BGM", "XP", "EXP", "PS", "XBOX", "HGSS", "BW", "B2W2", "UV", "IPS", "BPS",
                                        "LB", "RB", "LT", "RT", "L1", "R1", "L2", "R2", "L3", "R3", "ZL", "ZR", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12" };
    static const char* const small[] = { "OF", "AND", "THE", "TO", "IN", "FOR", "OR", "AT", "BY", "WITH", "DE", "DI", "DEL", "LA", "LE", "EL", "Y" };
    std::string out;
    size_t i = 0;
    bool firstWord = true;
    while (i < s.size())
    {
        size_t j = i;
        while (j < s.size() && s[j] != ' ' && s[j] != '/' && s[j] != ':' && s[j] != '(' && s[j] != ')' && s[j] != '-') j++;
        std::string word = s.substr(i, j - i);
        bool hasLower = false;
        for (char ch : word) if (ch >= 'a' && ch <= 'z') hasLower = true;
        if (!word.empty() && !hasLower)
        {
            bool kept = false;
            for (const char* k : keep) if (word == k) kept = true;
            bool digits = false;
            for (char ch : word) if (ch >= '0' && ch <= '9') digits = true;
            if (word == "POKEMON") word = "Pokémon";
            else if (word == "POKEDEX") word = "Pokédex";
            else if (word == "POKEDAISY") word = "PokéDaisy";
            else if (word == "RETROACHIEVEMENTS") word = "RetroAchievements";
            else if (!kept && !digits)
            {
                bool isSmall = false;
                for (const char* k : small) if (word == k) isSmall = true;
                std::string w;
                bool first = true;
                for (size_t k = 0; k < word.size();)
                {
                    const size_t start = k;
                    uint32_t cp = Next(word, k);
                    const bool upperFirst = first && !(isSmall && !firstWord);
                    if (!upperFirst)
                    {
                        if (cp >= 'A' && cp <= 'Z') cp += 32;
                        else if ((cp >= 0xC0 && cp <= 0xDE && cp != 0xD7)) cp += 32;
                        else if (cp >= 0x100 && cp <= 0x17F && !(cp & 1) && cp != 0x130) cp += 1;
                    }
                    if (cp < 0x80) w += (char)cp;
                    else if (cp < 0x800) { w += (char)(0xC0 | cp >> 6); w += (char)(0x80 | (cp & 0x3F)); }
                    else w += word.substr(start, k - start);
                    first = false;
                }
                word = w;
            }
        }
        out += word;
        if (!word.empty()) firstWord = false;
        if (j < s.size())
        {
            out += s[j];
            if (s[j] == ':' || s[j] == '(' || s[j] == '-') firstWord = true;
        }
        i = j + 1;
    }
    return out;
}
} // namespace CleanFont
