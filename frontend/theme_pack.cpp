// Themes for the launcher: zips and JPEGs (theme_pack.h).
#include "theme_pack.h"
#include <cstdio>
#include <cstring>
#include "thirdparty/stb_image.h" // (its inflate, built where the list's pictures are read)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "thirdparty/stb_image_write.h"

namespace ThemePack
{
namespace
{
uint32_t Crc(const unsigned char* p, size_t n)
{
    static uint32_t t[256];
    static bool made = false;
    if (!made)
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        made = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = t[(c ^ p[i]) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
void Put16(std::vector<unsigned char>& o, unsigned v) { o.push_back((unsigned char)(v & 255)); o.push_back((unsigned char)(v >> 8 & 255)); }
void Put32(std::vector<unsigned char>& o, uint32_t v) { Put16(o, v & 0xFFFF); Put16(o, v >> 16); }
unsigned Get16(const unsigned char* p) { return p[0] | p[1] << 8; }
uint32_t Get32(const unsigned char* p) { return (uint32_t)Get16(p) | (uint32_t)Get16(p + 2) << 16; }
}

bool ZipWrite(const std::string& path, const std::vector<std::pair<std::string, std::vector<unsigned char>>>& files)
{
    std::vector<unsigned char> out, dir;
    for (const auto& f : files)
    {
        const uint32_t crc = Crc(f.second.data(), f.second.size()), at = (uint32_t)out.size(), size = (uint32_t)f.second.size();
        // (Its own header: version 2.0, names in UTF-8, stored, 1 January 1980.)
        Put32(out, 0x04034b50u); Put16(out, 20); Put16(out, 0x0800); Put16(out, 0); Put16(out, 0); Put16(out, 0x21);
        Put32(out, crc); Put32(out, size); Put32(out, size); Put16(out, (unsigned)f.first.size()); Put16(out, 0);
        out.insert(out.end(), f.first.begin(), f.first.end());
        out.insert(out.end(), f.second.begin(), f.second.end());
        // (And in the list at the end.)
        Put32(dir, 0x02014b50u); Put16(dir, 20); Put16(dir, 20); Put16(dir, 0x0800); Put16(dir, 0); Put16(dir, 0); Put16(dir, 0x21);
        Put32(dir, crc); Put32(dir, size); Put32(dir, size); Put16(dir, (unsigned)f.first.size()); Put16(dir, 0); Put16(dir, 0);
        Put16(dir, 0); Put16(dir, 0); Put32(dir, 0); Put32(dir, at);
        dir.insert(dir.end(), f.first.begin(), f.first.end());
    }
    const uint32_t dirAt = (uint32_t)out.size();
    out.insert(out.end(), dir.begin(), dir.end());
    Put32(out, 0x06054b50u); Put16(out, 0); Put16(out, 0); Put16(out, (unsigned)files.size()); Put16(out, (unsigned)files.size());
    Put32(out, (uint32_t)dir.size()); Put32(out, dirAt); Put16(out, 0);
    const std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    const bool ok = fwrite(out.data(), 1, out.size(), f) == out.size();
    if (fclose(f) != 0 || !ok) { remove(tmp.c_str()); return false; }
    remove(path.c_str());
    return rename(tmp.c_str(), path.c_str()) == 0;
}

bool ZipRead(const std::string& path, std::map<std::string, std::vector<unsigned char>>& out, size_t limit, const std::string& only)
{
    out.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    // (A whole theme read: not much more than it holds; one file from it: any theme up to 64 MB.)
    if (len < 22 || (size_t)len > (only.empty() ? limit + (8u << 20) : (size_t)64 << 20)) { fclose(f); return false; }
    std::vector<unsigned char> z((size_t)len);
    const bool read = fread(z.data(), 1, z.size(), f) == z.size();
    fclose(f);
    if (!read) return false;
    // (The list at the end: found from its end record, at most 64 KB of comment after it.)
    long e = len - 22;
    for (; e >= 0 && e >= len - 22 - 65535; e--) if (Get32(&z[(size_t)e]) == 0x06054b50u) break;
    if (e < 0 || e < len - 22 - 65535) return false;
    const unsigned count = Get16(&z[(size_t)e + 10]);
    size_t at = Get32(&z[(size_t)e + 16]), total = 0;
    for (unsigned i = 0; i < count; i++)
    {
        if (at + 46 > z.size() || Get32(&z[at]) != 0x02014b50u) return false;
        const unsigned method = Get16(&z[at + 10]), nameLen = Get16(&z[at + 28]), extraLen = Get16(&z[at + 30]), commentLen = Get16(&z[at + 32]);
        const uint32_t csize = Get32(&z[at + 20]), usize = Get32(&z[at + 24]), local = Get32(&z[at + 42]);
        if (at + 46 + nameLen > z.size()) return false;
        const std::string name((const char*)&z[at + 46], nameLen);
        at += 46 + nameLen + extraLen + commentLen;
        if (name.empty() || name.back() == '/' || name.find("..") != std::string::npos) continue; // (folders, and no way out of the theme)
        if (!only.empty() && name != only) continue;
        if (local + 30 > z.size() || Get32(&z[local]) != 0x04034b50u) return false;
        const size_t data = local + 30 + Get16(&z[local + 26]) + Get16(&z[local + 28]);
        if (data + csize > z.size()) return false;
        total += usize;
        if (total > limit) return false;
        std::vector<unsigned char> body(usize);
        if (method == 0) { if (csize != usize) return false; if (usize) memcpy(body.data(), &z[data], usize); }
        else if (method == 8)
        {
            if (usize && stbi_zlib_decode_noheader_buffer((char*)body.data(), (int)usize, (const char*)&z[data], (int)csize) != (int)usize) return false;
        }
        else continue; // (another way of squeezing: not read)
        out[name] = std::move(body);
    }
    return true;
}

bool Jpeg(const uint32_t* argb, int w, int h, int quality, std::vector<unsigned char>& out)
{
    out.clear();
    if (w <= 0 || h <= 0) return false;
    std::vector<unsigned char> rgb((size_t)w * h * 3);
    for (size_t i = 0; i < (size_t)w * h; i++) { rgb[i * 3] = argb[i] >> 16 & 255; rgb[i * 3 + 1] = argb[i] >> 8 & 255; rgb[i * 3 + 2] = argb[i] & 255; }
    auto put = [](void* ctx, void* data, int size) { auto* v = (std::vector<unsigned char>*)ctx; v->insert(v->end(), (unsigned char*)data, (unsigned char*)data + size); };
    return stbi_write_jpg_to_func(put, &out, w, h, 3, rgb.data(), quality) != 0 && !out.empty();
}
}
