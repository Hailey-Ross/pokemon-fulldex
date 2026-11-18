#pragma once
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace RuntimeDiagnostics
{
struct Stamp
{
    bool present = false;
    std::filesystem::file_time_type modified {};
    uintmax_t size = 0;
};
inline Stamp Before(const std::string& folder)
{
    const auto path = std::filesystem::u8path(folder) / "data/log.txt";
    std::error_code ec;
    Stamp stamp;
    stamp.size = std::filesystem::file_size(path, ec);
    if (ec) return stamp;
    stamp.modified = std::filesystem::last_write_time(path, ec);
    stamp.present = !ec;
    return stamp;
}
// The runtime's renderer writes to data/log.txt rather than standard output.
// Include only a changed session log, bounded so verbose runtime logs cannot flood game.log.
inline void AppendChanged(const std::string& folder, const Stamp& before, FILE* log = stderr)
{
    const Stamp after = Before(folder);
    if (!after.present || (before.present && after.modified == before.modified && after.size == before.size)) return;
    const auto path = std::filesystem::u8path(folder) / "data/log.txt";
    std::ifstream in(path, std::ios::binary);
    if (!in) return;
    constexpr size_t limit = 256 * 1024;
    const size_t count = static_cast<size_t>(std::min<uintmax_t>(after.size, limit));
    if (after.size > limit) in.seekg(-static_cast<std::streamoff>(limit), std::ios::end);
    std::vector<char> text(count);
    in.read(text.data(), static_cast<std::streamsize>(count));
    const auto got = in.gcount();
    std::fprintf(log, "\n[3ds] runtime data/log.txt%s\n", after.size > limit ? " (last 256 KiB)" : "");
    if (got > 0) std::fwrite(text.data(), 1, static_cast<size_t>(got), log);
    std::fprintf(log, "\n[3ds] end runtime data/log.txt\n");
    std::fflush(log);
}
}
