#pragma once
// Which widescreen patch fits a ROM, and putting it on a copy in memory.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdint>
#include <string>
#include <vector>

namespace GbaWide
{

struct Profile
{
    const char* version;  // firered, firered11, leafgreen, leafgreen11, emerald
    const char* code;     // the cartridge's game code
    uint8_t revision;
    const char* key;      // its save's name
    const char* name;
};

// The ROM's game code (4 letters, "" when too short).
std::string GameCode(const std::vector<uint8_t>& rom);
// The profile whose game code and revision match and whose check list the ROM passes; null for any other
// ROM (another game, or a hack that changed the code the patch needs).
const Profile* FindProfile(const std::vector<uint8_t>& rom);
// Puts the profile's patch on the ROM (in memory). False when the patch would not fit.
bool ApplyWidePatch(const Profile& profile, std::vector<uint8_t>& rom);

}
