#pragma once
#include <filesystem>
#include <system_error>

#include <fstream>
#include <string>

namespace RuntimeKit
{
// The kit as it is now: its program's size and time. A copy that finished writes it to the game's
// folder (.fulldex-kit); a missing or different one (a copy cut short, FullDex updated) copies
// the missing files again. Walking the whole kit at every start cost a second or two.
inline std::string Stamp(const std::filesystem::path& kitExe)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(kitExe, ec);
    if (ec) return {};
    const auto time = std::filesystem::last_write_time(kitExe, ec);
    if (ec) return {};
    return std::to_string(size) + " " + std::to_string((long long)time.time_since_epoch().count());
}
inline bool Marked(const std::filesystem::path& folder, const std::string& stamp)
{
    std::ifstream in(folder / ".fulldex-kit", std::ios::binary);
    std::string had((std::istreambuf_iterator<char>(in)), {});
    return in.good() || in.eof() ? had == stamp : false;
}
inline void Mark(const std::filesystem::path& folder, const std::string& stamp)
{
    std::ofstream out(folder / ".fulldex-kit", std::ios::binary | std::ios::trunc);
    out << stamp;
}
inline std::filesystem::path NativePath(const std::filesystem::path& input, std::error_code& ec)
{
#ifdef _WIN32
    auto path = std::filesystem::absolute(input, ec).lexically_normal();
    if (ec) return {};
    const std::wstring wide = path.make_preferred().wstring();
    if (wide.rfind(L"\\\\?\\", 0) == 0) return path;
    if (wide.rfind(L"\\\\", 0) == 0) return std::filesystem::path(L"\\\\?\\UNC\\" + wide.substr(2));
    return std::filesystem::path(L"\\\\?\\" + wide);
#else
    (void)ec;
    return input;
#endif
}

// Retry an interrupted kit installation even if its executable was copied first.
// Existing files can contain a prepared game or player settings: never replace them.
inline bool CopyMissing(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& ec)
{
    namespace fs = std::filesystem;
    ec.clear();
    const auto nativeFrom = NativePath(from, ec);
    if (ec) return false;
    const auto nativeTo = NativePath(to, ec);
    if (ec) return false;
    fs::create_directories(nativeTo, ec);
    if (ec) return false;
    auto prefix = nativeFrom.native();
    if (prefix.empty()) { ec = std::make_error_code(std::errc::invalid_argument); return false; }
    if (prefix.back() != fs::path::preferred_separator) prefix += fs::path::preferred_separator;
    for (fs::recursive_directory_iterator it(nativeFrom, ec), end; !ec && it != end; it.increment(ec))
    {
        // Validate the iterator's source prefix before stripping it. Both relative path
        // operations misparse extended Windows paths in some MinGW libraries.
        const auto entry = it->path().native();
        if (entry.size() <= prefix.size() || entry.compare(0, prefix.size(), prefix) != 0)
        { ec = std::make_error_code(std::errc::invalid_argument); break; }
        const fs::path relative(entry.substr(prefix.size()));
        const fs::path dest = nativeTo / relative;
        const bool directory = it->is_directory(ec);
        if (ec) break;
        if (directory) fs::create_directories(dest, ec);
        else
        {
            const bool exists = fs::exists(dest, ec);
            if (ec) break;
            if (!exists)
            {
                fs::copy_file(it->path(), dest, ec);
#ifndef _WIN32
                if (!ec)
                {
                    const auto permissions = fs::status(it->path(), ec).permissions();
                    if (!ec) fs::permissions(dest, permissions, ec);
                }
#endif
            }
        }
        // iterator.increment(ec) clears ec on success; do not let it hide a failed copy.
        if (ec) break;
    }
    return !ec;
}
}
