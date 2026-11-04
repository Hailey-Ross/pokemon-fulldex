#pragma once
// The GBA games inside a Pokemon Fulldex build (SSR_GBA_DIR): the list's GBA section and playing one.
// Our own player on mGBA with the widescreen change (patches/mgba-wide.patch) and our widescreen
// patches (wide-patches/), put on a copy of the ROM in memory as the game starts.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace GbaWide
{

struct ListGame
{
    std::string file;  // the ROM
    std::string key;   // its save's name without .sav: firered, leafgreen, emerald, or the file's own name
    std::string name;  // shown in the list
    bool hasSave = false;
    std::string art;   // a name for Art ("": none)
};

// The GBA games in a folder (not its subfolders), their saves in `saves`.
std::vector<ListGame> Find(const std::string& roms, const std::string& saves);

// Built-in box art by ListGame::art. We have none: always false.
bool Art(const std::string& name, const unsigned char*& jpeg, size_t& size);

// RetroAchievements, run by the program that embeds us (frontend/gba_ra.h in Fulldex).
struct AchievementsHost
{
    // RetroAchievements' GBA memory: IWRAM from 0, EWRAM from 0x8000, save RAM from 0x48000.
    using Read = uint32_t (*)(void* userdata, uint32_t address, uint8_t* buffer, uint32_t bytes);

    void* ud = nullptr;
    bool (*begin)(void* ud, const uint8_t* rom, size_t n, void* core, Read read) = nullptr;
    void (*end)(void* ud) = nullptr;
    void (*frame)(void* ud) = nullptr;
    void (*pump)(void* ud, int paused) = nullptr;
    void (*reset)(void* ud) = nullptr;
    void (*setOn)(void* ud, int on) = nullptr;
    void (*login)(void* ud, const char* user, const char* password) = nullptr;
    void (*logout)(void* ud) = nullptr;
    bool (*account)(void* ud, char* buf, size_t cap) = nullptr;
    bool (*game)(void* ud, char* buf, size_t cap) = nullptr;
    bool (*detail)(void* ud, char* buf, size_t cap) = nullptr;
    bool (*pop)(void* ud, char* buf, size_t cap) = nullptr;
    bool (*loggedIn)(void* ud) = nullptr;
    bool (*active)(void* ud) = nullptr;
    bool (*busy)(void* ud) = nullptr;
    bool (*on)(void* ud) = nullptr;
    int (*count)(void* ud) = nullptr;
    bool (*at)(void* ud, int index, char* title, size_t titleCap, char* description, size_t descriptionCap,
               char* progress, size_t progressCap, unsigned* points, int* unlocked) = nullptr;
};

struct Options
{
    std::string roms;   // the ROM's folder
    std::string saves;  // where <key>.sav lives
    std::string data;   // our settings: gba-settings.ini
    bool fullscreen = false;
    int w = 0, h = 0;   // the window's size when not fullscreen (0: our own)
    AchievementsHost* achievements = nullptr;
    std::string save;   // a save file to use instead of <saves>/<key>.sav (the stand-alone player's --save)
};

// Plays the game until the player quits. False when it could not start.
bool Play(const std::string& rom, Options& o);

}
