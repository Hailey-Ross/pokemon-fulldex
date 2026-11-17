#pragma once
#include <filesystem>
#include <functional>
#include <string>

namespace GbaStateFile
{
// Commit a fully written and closed state, retaining the last successful version.
// An interrupted/failed write stays in .pending for recovery rather than truncating a slot.
inline bool Save(const std::string& path, const std::function<bool(const std::string&)>& write)
{
    namespace fs = std::filesystem;
    const auto target = fs::u8path(path), pending = fs::u8path(path + ".pending"), previous = fs::u8path(path + ".previous");
    std::error_code ec;
    if (fs::exists(pending, ec) || ec) return false;
    const bool had = fs::exists(target, ec);
    if (ec || (had && !fs::is_regular_file(target, ec)) || ec) return false;
    if (!write(pending.u8string())) return false;
    const auto size = fs::file_size(pending, ec);
    if (ec || !size) return false;
    if (had)
    {
        const bool backup = fs::exists(previous, ec);
        if (ec || (backup && !fs::is_regular_file(previous, ec)) || ec) return false;
        fs::remove(previous, ec);
        if (ec) return false;
        fs::rename(target, previous, ec);
        if (ec) return false;
    }
    fs::rename(pending, target, ec);
    if (!ec) return true;
    if (had)
    {
        std::error_code recovery;
        fs::rename(previous, target, recovery);
    }
    return false;
}
}
