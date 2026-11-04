// Which widescreen patch fits a ROM, and putting it on a copy in memory.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "wide_rom.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// From cmake/embed.cmake: wide-patches/prebuilt/<version>.ips and wide-patches/versions/<version>.check
struct GbawFile
{
    const char* name;
    const unsigned char* data;
    size_t size;
};
extern const GbawFile gGbawFiles[];
extern const size_t gGbawFileCount;

namespace GbaWide
{

static const Profile kProfiles[] = {
    { "firered", "BPRE", 0, "firered", "Pokemon FireRed" },
    { "firered11", "BPRE", 1, "firered", "Pokemon FireRed" },
    { "leafgreen", "BPGE", 0, "leafgreen", "Pokemon LeafGreen" },
    { "leafgreen11", "BPGE", 1, "leafgreen", "Pokemon LeafGreen" },
    { "emerald", "BPEE", 0, "emerald", "Pokemon Emerald" },
};

static const GbawFile* File(const std::string& name)
{
    for (size_t i = 0; i < gGbawFileCount; i++)
        if (name == gGbawFiles[i].name) return &gGbawFiles[i];
    return nullptr;
}

static uint32_t Crc32(const uint8_t* p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++)
    {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

// Every line of the check list: offset size crc32 name, all hex but the name.
static bool PassesCheck(const GbawFile& check, const std::vector<uint8_t>& rom)
{
    std::string text((const char*)check.data, check.size);
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (line.empty() || line[0] == '#') continue;
        unsigned long off = 0, size = 0, crc = 0;
        if (sscanf(line.c_str(), "%lx %lx %lx", &off, &size, &crc) != 3) return false;
        if (off + size > rom.size() || Crc32(rom.data() + off, size) != crc) return false;
    }
    return true;
}

std::string GameCode(const std::vector<uint8_t>& rom)
{
    if (rom.size() < 0xC0) return "";
    return std::string((const char*)rom.data() + 0xAC, 4);
}

const Profile* FindProfile(const std::vector<uint8_t>& rom)
{
    const std::string code = GameCode(rom);
    for (const Profile& p : kProfiles)
    {
        if (code != p.code || rom[0xBC] != p.revision) continue;
        const GbawFile* check = File(std::string(p.version) + ".check");
        if (check && PassesCheck(*check, rom)) return &p;
    }
    return nullptr;
}

bool ApplyIps(const uint8_t* d, size_t size, std::vector<uint8_t>& rom, bool widePatch)
{
    if (!d || size < 8 || memcmp(d, "PATCH", 5)) return false;
    std::vector<uint8_t> out = rom;
    size_t i = 5;
    bool ended = false;
    while (i + 3 <= size)
    {
        if (!memcmp(d + i, "EOF", 3)) { ended = true; i += 3; break; }
        if (i + 5 > size) return false;
        const size_t off = (size_t)d[i] << 16 | d[i + 1] << 8 | d[i + 2];
        size_t n = d[i + 3] << 8 | d[i + 4];
        i += 5;
        const uint8_t* src = d + i;
        std::vector<uint8_t> run;
        if (n == 0)
        {
            if (i + 3 > size) return false;
            run.assign(d[i] << 8 | d[i + 1], d[i + 2]);
            src = run.data();
            n = run.size();
            i += 3;
        }
        else
        {
            if (i + n > size) return false;
            i += n;
        }
        if (off + n > out.size())
        {
            if (widePatch || off + n > 32u << 20) return false;
            out.resize(off + n, 0xFF);
        }
        // Ours: the code block goes in free space only (0xFF); the 8-byte jumps go over the checked game code.
        if (widePatch && n > 8)
            for (size_t k = 0; k < n; k++)
                if (out[off + k] != 0xFF) return false;
        memcpy(out.data() + off, src, n);
    }
    if (!ended) return false;
    // (IPS's size extension: three bytes after EOF cut the ROM to that size.)
    if (!widePatch && i + 3 <= size)
    {
        const size_t cut = (size_t)d[i] << 16 | d[i + 1] << 8 | d[i + 2];
        if (cut && cut < out.size()) out.resize(cut);
    }
    rom.swap(out);
    return true;
}

bool ApplyWidePatch(const Profile& profile, std::vector<uint8_t>& rom)
{
    const GbawFile* ips = File(std::string(profile.version) + ".ips");
    return ips && ApplyIps(ips->data, ips->size, rom, true);
}

}
