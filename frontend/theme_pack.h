#pragma once
// Themes for the launcher (its My Themes and Theme Gallery): a .zip of a few files, and JPEGs made
// for them. theme_pack.cpp.
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ThemePack
{
// A zip of the files, written whole. Stored, not squeezed: the pictures are squeezed already.
bool ZipWrite(const std::string& path, const std::vector<std::pair<std::string, std::vector<unsigned char>>>& files);
// The files in a zip (stored or deflated), at most limit bytes in all; only the one named when only is given.
bool ZipRead(const std::string& path, std::map<std::string, std::vector<unsigned char>>& out, size_t limit, const std::string& only = std::string());
// ARGB pixels as a JPEG (quality 1 to 100).
bool Jpeg(const uint32_t* argb, int w, int h, int quality, std::vector<unsigned char>& out);
}
