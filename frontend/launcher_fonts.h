#pragma once
// The launcher's fonts (its Theme Editor's Font): Noto Sans first (CleanFont's own), then 29 more,
// all under the Open Font License (fonts/FONT-LICENSES.txt). launcher_fonts.cpp, made by mkfonts.py.
#include <cstddef>

struct LauncherFontInfo
{
    const char* id;
    const char* name;
    const char* group;
    const unsigned char* regular; // (nullptr: CleanFont's Noto Sans)
    const unsigned char* bold;
    float scale;                  // (drawn this much bigger or smaller, to fill a line as Noto Sans does)
};
extern const LauncherFontInfo kLauncherFonts[];
extern const int kLauncherFontCount;

// For CleanFont::FontSource: the font id, 1 and up.
inline const unsigned char* LauncherFontSource(int id, bool bold, float* scale)
{
    if (id <= 0 || id >= kLauncherFontCount) return nullptr;
    *scale = kLauncherFonts[id].scale;
    return bold ? kLauncherFonts[id].bold : kLauncherFonts[id].regular;
}
