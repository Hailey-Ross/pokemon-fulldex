#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace PictureBrowser
{
struct Entry { std::string path, label; bool directory; };
inline std::vector<Entry> Entries(const std::string& folder)
{
    namespace fs = std::filesystem;
    std::vector<Entry> entries;
    std::error_code ec;
    for (fs::directory_iterator it(fs::u8path(folder), fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec))
    {
        std::error_code error;
        const bool dir = it->is_directory(error);
        if (error || (!dir && !it->is_regular_file(error))) continue;
        const std::string name = it->path().filename().u8string();
        if (name.empty() || name[0] == '.') continue;
        std::string ext = it->path().extension().u8string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (dir || ext == ".png" || ext == ".jpg" || ext == ".jpeg")
            entries.push_back({it->path().u8string(), name + (dir ? "/" : ""), dir});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.directory != b.directory ? a.directory : a.label < b.label;
    });
    return entries;
}
}
