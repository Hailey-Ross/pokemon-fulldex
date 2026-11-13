#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace RomLibrary
{
// Shared libraries often group ROMs by console or game. Do not scan saves, generated
// hack caches, hidden folders or directory symlinks back into the same library.
inline std::vector<std::string> Folders(const std::string& root, const std::string& cache = "")
{
    namespace fs = std::filesystem;
    std::vector<std::string> out;
    if (root.empty()) return out;
    std::error_code ec;
    if (!fs::is_directory(fs::u8path(root), ec)) return out;
    const fs::path cachePath = cache.empty() ? fs::path() : fs::weakly_canonical(fs::u8path(cache), ec);
    ec.clear();
    out.push_back(root);
    fs::recursive_directory_iterator it(fs::u8path(root), fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec))
    {
        std::error_code entryError;
        if (!it->is_directory(entryError)) continue;
        std::string name = it->path().filename().u8string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (it.depth() >= 8 || name.empty() || name[0] == '.' || name == "saves" || name == "backups" || it->is_symlink(entryError))
        {
            it.disable_recursion_pending();
            continue;
        }
        if (!cachePath.empty() && fs::weakly_canonical(it->path(), entryError) == cachePath)
        {
            it.disable_recursion_pending();
            continue;
        }
        out.push_back(it->path().u8string());
    }
    std::sort(out.begin() + 1, out.end());
    return out;
}
}
