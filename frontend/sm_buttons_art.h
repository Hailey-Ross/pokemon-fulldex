// The words of BATTLE BUTTONS: SUN AND MOON (sm_buttons.h), drawn by FullDex into a picture the
// compositor takes as one more screen (SmButtons::kScreen): FIGHT, POKEMON, BAG and RUN in heavy
// dark letters (Rubik Bold, one of the launcher's fonts, Open Font License), each on its button's
// own colour so that it is laid on the button as it is. Made again when the menus' language
// changes (Japanese and Korean keep the English words: the font has no letters for them).
// Included after ui.h (stb_truetype comes with ui_clean_font.h).
#pragma once
#include <cstring>
#include <string>
#include <vector>

#include "launcher_fonts.h"
#include "sm_buttons.h"
#include "ui_lang.h"

namespace SmButtonsArt
{
inline uint32_t* Pixels() { static uint32_t px[256 * 192]; return px; }

inline std::vector<uint32_t> Codepoints(const std::string& s)
{
    std::vector<uint32_t> out;
    for (size_t at = 0; at < s.size();) out.push_back(UiLang::Next(s, at));
    return out;
}

// The picture for the menus' language [lang] (UiLang::lang); made once for each language.
inline void Ensure(int lang)
{
    static int made = -1;
    if (made == lang) return;
    made = lang;
    static stbtt_fontinfo font;
    static bool ok = false, tried = false;
    if (!tried)
    {
        tried = true;
        for (int i = 0; i < kLauncherFontCount; i++)
            if (!strcmp(kLauncherFonts[i].id, "rubik") && kLauncherFonts[i].bold)
                ok = stbtt_InitFont(&font, kLauncherFonts[i].bold, stbtt_GetFontOffsetForIndex(kLauncherFonts[i].bold, 0)) != 0;
    }
    uint32_t* px = Pixels();
    std::fill(px, px + 256 * 192, 0u);
    SmButtons::State& st = SmButtons::S();
    for (auto& l : st.label) l = {};
    if (!ok) return;
    static const char* const english[SmButtons::COUNT] = { "FIGHT", "POKÉMON", "BAG", "RUN" };
    // (FIGHT's letters bigger, as they are drawn bigger.)
    const float sizes[SmButtons::COUNT] = { 40, 34, 34, 34 };
    int top = 0, left = 0, rowH = 0; // (the words packed in rows)
    for (int b = 0; b < SmButtons::COUNT; b++)
    {
        // (The menus' own word in Spanish, Italian, French and German, where they have one.)
        std::string word = english[b];
        if (lang >= 1 && lang <= 4)
        {
            const int keep = UiLang::lang;
            UiLang::lang = lang;
            const std::string t = UiLang::Tr(b == SmButtons::POKEMON ? "POKEMON" : english[b]);
            UiLang::lang = keep;
            if (!t.empty() && t != (b == SmButtons::POKEMON ? "POKEMON" : english[b])) word = t;
        }
        const float scale = stbtt_ScaleForPixelHeight(&font, sizes[b]);
        int ascent, descent, gap;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);
        const float base = ascent * scale + 2;
        // Capital height: the H's box.
        int hx0, hy0, hx1, hy1;
        stbtt_GetCodepointBitmapBox(&font, 'H', scale, scale, &hx0, &hy0, &hx1, &hy1);
        const std::vector<uint32_t> cps = Codepoints(word);
        // Letters closer than the font's own spacing (Sun and Moon's are tight): 7% of the size.
        const float track = -0.07f * sizes[b];
        float pen = 3;
        std::vector<float> at;
        for (size_t i = 0; i < cps.size(); i++)
        {
            at.push_back(pen);
            int adv, lsb;
            stbtt_GetCodepointHMetrics(&font, (int)cps[i], &adv, &lsb);
            pen += adv * scale + track;
            if (i + 1 < cps.size()) pen += stbtt_GetCodepointKernAdvance(&font, (int)cps[i], (int)cps[i + 1]) * scale;
        }
        const int w = std::min(250, (int)std::ceil(pen - track) + 4), h = (int)std::ceil((ascent - descent) * scale) + 4;
        const int pad = 3;
        if (w + 2 * pad > 256) break;
        if (left + w + 2 * pad > 256) { top += rowH + 1; left = 0; rowH = 0; }
        if (top + h + 2 * pad > 192) break;
        // Coverage of the word, then made heavier: each pixel the most of itself and the one left of it.
        std::vector<float> cover((size_t)w * h, 0.0f), heavy((size_t)w * h, 0.0f);
        for (size_t i = 0; i < cps.size(); i++)
        {
            int gw, gh, gx, gy;
            unsigned char* g = stbtt_GetCodepointBitmap(&font, scale, scale, (int)cps[i], &gw, &gh, &gx, &gy);
            if (!g) continue;
            const int ox = (int)std::lround(at[i]) + gx, oy = (int)std::lround(base) + gy;
            for (int y = 0; y < gh; y++)
                for (int x = 0; x < gw; x++)
                {
                    const int X = ox + x, Y = oy + y;
                    if (X >= 0 && Y >= 0 && X < w && Y < h) cover[(size_t)Y * w + X] = std::max(cover[(size_t)Y * w + X], g[y * gw + x] / 255.0f);
                }
            stbtt_FreeBitmap(g, nullptr);
        }
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                float m = cover[(size_t)y * w + x];
                if (x > 0) m = std::max(m, cover[(size_t)y * w + x - 1]);
                heavy[(size_t)y * w + x] = m;
            }
        const uint32_t fill = SmButtons::LookOf(b).fill, ink = 0x100E12;
        // (A margin of the button's colour all round: the compositor's smoothing reads a little
        // past the word's edges.)
        for (int y = 0; y < h + 2 * pad; y++)
            for (int x = 0; x < w + 2 * pad; x++) px[(top + y) * 256 + left + x] = fill;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                const float a = heavy[(size_t)y * w + x];
                uint32_t c = 0;
                for (int sh = 0; sh <= 16; sh += 8)
                {
                    const float v = ((fill >> sh) & 0xFF) * (1 - a) + ((ink >> sh) & 0xFF) * a;
                    c |= (uint32_t)std::lround(v) << sh;
                }
                px[(top + pad + y) * 256 + left + pad + x] = c;
            }
        SmButtons::Label& l = st.label[b];
        l.x = left + pad; l.y = top + pad; l.w = w; l.h = h;
        l.capTop = base + hy0;
        l.capH = (float)(hy1 - hy0);
        left += w + 2 * pad + 1;
        rowH = std::max(rowH, h + 2 * pad);
    }
}
} // namespace SmButtonsArt
