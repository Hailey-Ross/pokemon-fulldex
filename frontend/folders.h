#pragma once
// FOLDERS: where FullDex keeps the saves, the save states, the cheats and its own settings files.
//
// TIDY (folders_layout=tidy: new installs, and updates once their files are moved):
//   <root>/saves  <root>/states  <root>/cheats  <root>/settings
//   root: the program's folder on a computer; on a phone, a FullDex folder inside the ROM folder,
//   so the ROM folder keeps only the ROMs and that one folder.
// CLASSIC (folders_layout=classic: before, kept by UNDO, or a saves folder of the player's own):
//   the settings files and the save states beside the saves, the cheats beside a folder called saves.
// Each can be a folder of the player's own (states_folder, cheats_folder, settings_folder; saves_folder
// as before).
//
// The move from CLASSIC to TIDY (Move): every file copied, compared with the original, and only when
// all of them are there and the same are the originals removed. A journal beside the settings file
// (folders-move.txt) says what was moved, so a move cut short is finished or undone at the next start
// and UNDO can put everything back.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace FolderSet
{
namespace fs = std::filesystem;

// Where the save states are ("": beside each save, as before) and the cheats ("": Cheats::FolderFor's
// rule): set by the launcher, and in the game by --states and --cheats.
inline std::string& States() { static std::string s; return s; }
inline std::string& CheatsDir() { static std::string s; return s; }
inline std::string BaseName(const std::string& p) { const size_t s = p.find_last_of("/\\"); return s == std::string::npos ? p : p.substr(s + 1); }
// The quick save state of a save (its slots add .slotN): in the states folder, or beside the save.
inline std::string StateOf(const std::string& save) { return States().empty() ? save + ".state" : States() + "/" + BaseName(save) + ".state"; }

inline bool IsDir(const std::string& p) { std::error_code e; return !p.empty() && fs::is_directory(fs::u8path(p), e); }
inline bool IsFile(const std::string& p) { std::error_code e; return !p.empty() && fs::is_regular_file(fs::u8path(p), e); }
inline bool MakeDirs(const std::string& p) { std::error_code e; fs::create_directories(fs::u8path(p), e); return IsDir(p); }

// FullDex's own settings files among a saves folder's (the GBA games' settings.ini stays: theirs).
inline bool IsSettingsName(const std::string& n, bool dir)
{
    if (dir) return n == "artwork" || n == "themes" || n == "launcher-pictures";
    auto ends = [&](const char* x) { const size_t l = strlen(x); return n.size() >= l && n.compare(n.size() - l, l, x) == 0; };
    return (n.rfind("fulldex-", 0) == 0 && ends(".txt")) || (n.rfind("launcher-", 0) == 0 && ends(".txt")) ||
           n == "rom-check.txt" || n == "cloud.ini" || n == "cloud-state.txt";
}
// A DS game's save states (<save>.state, its .slotN, .undo-load and their pictures).
inline bool IsStateName(const std::string& n) { return n.find(".state") != std::string::npos; }

using Pairs = std::vector<std::pair<std::string, std::string>>; // (from, to)

// Every file under from (a file or a folder), as pairs to the same place under to.
inline void AddTree(const std::string& from, const std::string& to, Pairs& out)
{
    std::error_code e;
    const fs::path f = fs::u8path(from);
    if (fs::is_regular_file(f, e)) { out.push_back({ from, to }); return; }
    if (!fs::is_directory(f, e)) return;
    for (fs::recursive_directory_iterator it(f, e), end; !e && it != end; it.increment(e))
        if (it->is_regular_file(e))
        {
            const std::string rel = fs::relative(it->path(), f, e).u8string();
            out.push_back({ it->path().u8string(), to + "/" + rel });
        }
}

inline bool SameFile(const std::string& a, const std::string& b)
{
    std::ifstream x(fs::u8path(a), std::ios::binary), y(fs::u8path(b), std::ios::binary);
    if (!x || !y) return false;
    char p[65536], q[65536];
    for (;;)
    {
        x.read(p, sizeof p); y.read(q, sizeof q);
        const auto n = x.gcount(), m = y.gcount();
        if (n != m || memcmp(p, q, (size_t)n) != 0) return false;
        if (n == 0) return x.eof() && y.eof();
    }
}

inline bool WriteJournal(const std::string& path, const char* stage, const Pairs& pairs)
{
    const std::string tmp = path + ".new";
    {
        std::ofstream j(fs::u8path(tmp), std::ios::binary | std::ios::trunc);
        if (!j) return false;
        j << "fulldex-folders-move-v1\n" << stage << "\n";
        for (const auto& p : pairs) j << p.first << "\t" << p.second << "\n";
        if (!j.good()) return false;
    }
    std::error_code e;
    fs::rename(fs::u8path(tmp), fs::u8path(path), e);
    return !e;
}
inline bool ReadJournal(const std::string& path, std::string& stage, Pairs& pairs)
{
    std::ifstream j(fs::u8path(path), std::ios::binary);
    std::string line;
    if (!j || !std::getline(j, line) || line != "fulldex-folders-move-v1" || !std::getline(j, stage)) return false;
    pairs.clear();
    while (std::getline(j, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t t = line.find('\t');
        if (t != std::string::npos) pairs.push_back({ line.substr(0, t), line.substr(t + 1) });
    }
    return true;
}

inline void RemoveEmptyParents(const std::string& file, const std::vector<std::string>& keep)
{
    std::error_code e;
    fs::path d = fs::u8path(file).parent_path();
    while (!d.empty())
    {
        for (const auto& k : keep) if (fs::u8path(k) == d) return;
        if (!fs::is_directory(d, e) || !fs::is_empty(d, e)) return;
        fs::remove(d, e);
        d = d.parent_path();
    }
}

// Moves the pairs: copies all (none written over: a file already there that is not the same stops the
// move), compares each, then removes the originals. Nothing removed unless every copy is good; on a
// failure the copies it made are removed again and false returned, the files where they were.
// journal: "copying" while copying, "done" after (kept for UNDO). keep: folders not to remove when emptied.
inline bool Move(const Pairs& pairs, const std::string& journal, const std::vector<std::string>& keep, std::string& why)
{
    std::error_code e;
    if (!WriteJournal(journal, "copying", pairs)) { why = "COULD NOT WRITE ITS NOTES"; return false; }
    Pairs made;
    bool ok = true;
    for (const auto& p : pairs)
    {
        const fs::path to = fs::u8path(p.second);
        if (fs::exists(to, e))
        {
            if (SameFile(p.first, p.second)) continue; // (there already: from a move cut short)
            why = "A FILE IS ALREADY THERE: " + p.second; ok = false; break;
        }
        fs::create_directories(to.parent_path(), e);
        fs::copy_file(fs::u8path(p.first), to, e);
        if (e || !SameFile(p.first, p.second)) { why = "COULD NOT COPY " + p.first; made.push_back(p); ok = false; break; }
        made.push_back(p);
    }
    if (!ok)
    {
        for (const auto& p : made) { fs::remove(fs::u8path(p.second), e); RemoveEmptyParents(p.second, keep); }
        fs::remove(fs::u8path(journal), e);
        return false;
    }
    if (!WriteJournal(journal, "removing", pairs)) { why = "COULD NOT WRITE ITS NOTES"; return false; }
    for (const auto& p : pairs)
        if (SameFile(p.first, p.second)) { fs::remove(fs::u8path(p.first), e); RemoveEmptyParents(p.first, keep); }
    WriteJournal(journal, "done", pairs);
    return true;
}

// A move cut short (the program stopped while it ran): one still copying is taken back (its copies
// removed), one removing is finished. True when there was one.
inline bool FinishCutShort(const std::string& journal, const std::vector<std::string>& keep)
{
    std::string stage; Pairs pairs; std::error_code e;
    if (!ReadJournal(journal, stage, pairs)) return false;
    if (stage == "copying")
    {
        for (const auto& p : pairs)
            if (IsFile(p.first) && IsFile(p.second) && SameFile(p.first, p.second)) { fs::remove(fs::u8path(p.second), e); RemoveEmptyParents(p.second, keep); }
        fs::remove(fs::u8path(journal), e);
        return true;
    }
    if (stage == "removing")
    {
        for (const auto& p : pairs)
            if (IsFile(p.first) && IsFile(p.second) && SameFile(p.first, p.second)) { fs::remove(fs::u8path(p.first), e); RemoveEmptyParents(p.first, keep); }
        WriteJournal(journal, "done", pairs);
        return true;
    }
    return false;
}

// UNDO: the last move's files back where they were (the same careful way). Files made since in the new
// folders (a new save, a new setting) go back with their folder's: the classic place of each folder.
inline bool Undo(const std::string& journal, const std::vector<std::pair<std::string, std::string>>& folders,
                 const std::vector<std::string>& keep, std::string& why)
{
    std::string stage; Pairs pairs;
    if (!ReadJournal(journal, stage, pairs) || stage != "done") { why = "NOTHING TO UNDO"; return false; }
    Pairs back;
    for (const auto& p : pairs) if (IsFile(p.second)) back.push_back({ p.second, p.first });
    // (New files in the tidy folders since: each to its folder's classic place.)
    for (const auto& f : folders)
    {
        Pairs all; AddTree(f.first, f.second, all);
        for (const auto& a : all)
        {
            bool known = false;
            for (const auto& b : back) if (b.first == a.first) { known = true; break; }
            if (!known) back.push_back(a);
        }
    }
    std::error_code e;
    const std::string undoJournal = journal + ".undo";
    if (!Move(back, undoJournal, keep, why)) return false;
    fs::remove(fs::u8path(undoJournal), e);
    fs::remove(fs::u8path(journal), e);
    return true;
}
} // namespace FolderSet
