// gbawide: the GBA player on its own, for launchers.
//   gbawide [--rom] <file.gba|file.ips> [--base <firered.gba>] [--save <file.sav>] [--saves <folder>] [--data <folder>]
//           [--config <settings.cfg>] [--fullscreen] [--size WxH]
// An .ips (Ultra Violet's) plays over FireRed 1.0: --base, else one beside it. Without --save the save is
// <saves>/<name>.sav (saves: beside the ROM). Settings: <data>/gba-settings.ini (data: the program's folder).
// RetroAchievements: Fulldex's account in --config (default <data>/settings.cfg). Exit code 0 when the game ran.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "embed.h"
#ifdef GBAW_RA
#include "gba_ra.h" // ra=, ra_user=, ra_token= in Fulldex's settings.cfg
#endif

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static std::string Folder(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

int main(int argc, char** argv)
{
    GbaWide::Options o;
    std::string rom, config;
    for (int i = 1; i < argc; i++)
    {
        const std::string a = argv[i];
        auto value = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rom") rom = value();
        else if (a == "--save") o.save = value();
        else if (a == "--base") o.base = value();
        else if (a == "--saves") o.saves = value();
        else if (a == "--data") o.data = value();
        else if (a == "--config") config = value();
        else if (a == "--fullscreen") o.fullscreen = true;
        else if (a == "--size") sscanf(value().c_str(), "%dx%d", &o.w, &o.h);
        else if (a.size() > 2 && a[0] != '-') rom = a;
        else
        {
            fprintf(stderr, "gbawide: unknown option %s\n", a.c_str());
            return 2;
        }
    }
    if (rom.empty())
    {
        fprintf(stderr, "usage: gbawide [--rom] <file.gba|file.ips> [--base <firered.gba>] [--save <file.sav>] [--saves <folder>]"
                        " [--data <folder>] [--config <settings.cfg>] [--fullscreen] [--size WxH]\n");
        return 2;
    }
    o.roms = Folder(rom);
    if (o.saves.empty()) o.saves = o.roms;
    if (o.data.empty())
    {
        char* base = SDL_GetBasePath();
        o.data = base ? std::string(base) : ".";
        SDL_free(base);
        while (o.data.size() > 1 && (o.data.back() == '/' || o.data.back() == '\\')) o.data.pop_back();
    }
    if (config.empty()) config = o.data + "/settings.cfg";
    if (SDL_Init(SDL_INIT_EVENTS) != 0)
    {
        fprintf(stderr, "gbawide: SDL: %s\n", SDL_GetError());
        return 1;
    }
#ifdef GBAW_RA
    const bool ok = GbaRa::Play(rom, o, config);
#else
    const bool ok = GbaWide::Play(rom, o);
#endif
    SDL_Quit();
    return ok ? 0 : 1;
}
