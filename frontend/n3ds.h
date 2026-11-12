// Pokemon Omega Ruby and Alpha Sapphire (3DS) in FullDex's game list.
//
// The games are the omega-nx programs, each in a folder of its own with its saves and settings:
// FullDex lists them and starts them, and they come back to the list when the player quits.
//   Switch: the folders the FullDex Builder makes (omega-nx, pokemon-alpha-sapphire) in /switch, each
//     with a .nro, the player's game.3ds and titles/<title id>. The game is started as FullDex ends,
//     told where FullDex is (FULLDEX_RETURN), and starts FullDex again when it quits from its menu.
//   Windows: the program is in the download's 3ds\program folder. A dump (.3ds) found with the ROMs
//     gets a folder of its own, 3ds\<title id>, a copy of the program, where the game's code is built
//     once from the dump ("Pokemon ORAS.exe" --prepare, 5 to 15 minutes) and the game then plays.
#pragma once
#include <algorithm>
#include <atomic>
#include <cctype>
#include <ctime>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <sys/stat.h>
#ifdef __ANDROID__
#include <sys/statvfs.h>
#endif
#include <dirent.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "launcher.h"

namespace N3ds
{
struct Title { const char* code; const char* id; const char* name; };
inline const Title kTitles[] = {
    { "ECRA", "000400000011C400", "POKEMON OMEGA RUBY" },
    { "ECLA", "000400000011C500", "POKEMON ALPHA SAPPHIRE" },
};
inline const Title* ById(const std::string& id)
{
    for (const Title& t : kTitles)
        if (id == t.id) return &t;
    return nullptr;
}
inline std::string IdOf(const Launcher::Game& g)
{
    for (const Title& t : kTitles)
        if (g.code == t.code) return t.id;
    return "";
}

inline bool IsDir(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }
inline bool IsFile(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode); }
inline bool EndsWith(const std::string& s, const char* e)
{
    const size_t n = strlen(e);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[s.size() - n + i]) != e[i]) return false;
    return true;
}

// The title id of a .3ds dump (8 bytes at 0x108, little-endian), as 16 capital hex digits; "" if unreadable.
inline std::string DumpTitle(const std::string& path)
{
#ifdef _WIN32
    FILE* f = _wfopen(std::filesystem::u8path(path).wstring().c_str(), L"rb");
#else
    FILE* f = fopen(path.c_str(), "rb");
#endif
    if (!f) return "";
    unsigned char b[8] = {};
    const bool ok = fseek(f, 0x108, SEEK_SET) == 0 && fread(b, 1, 8, f) == 8;
    fclose(f);
    if (!ok) return "";
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = v << 8 | b[i];
    char hex[17];
    snprintf(hex, sizeof hex, "%016llX", (unsigned long long)v);
    return hex;
}

inline Launcher::Game Make(const Title& t, const std::string& rom, const std::string& folder)
{
    Launcher::Game g;
    g.platform = Launcher::N3DS;
    g.code = t.code;
    g.rom = rom;
    g.folder = folder;
    g.name = t.name;
    g.known = true;
    return g;
}

// A real game takes the place of its COMING SOON tile; and once their day has come, the tiles of
// games this program cannot play here go (a platform without the 3DS programs).
inline void DropPlaceholders(std::vector<Launcher::Game>& games)
{
    std::vector<Launcher::Game> keep;
    for (Launcher::Game& g : games)
    {
        if (g.comingSoon && g.platform == Launcher::N3DS)
        {
            bool real = false;
            for (const Launcher::Game& o : games) real |= !o.comingSoon && o.platform == Launcher::N3DS && o.code == g.code;
            if (real || (g.soonAt > 0 && g.soonAt <= (long long)std::time(nullptr))) continue;
        }
        keep.push_back(std::move(g));
    }
    games.swap(keep);
}

// Switch: each folder in root with game.3ds, titles/<a title id> and a .nro. One tile a game: the
// FullDex Builder's own folder (omega-nx, pokemon-alpha-sapphire) when there, else the newest .nro
// (an older test folder of the same game is left out of the list).
inline void AddSwitch(std::vector<Launcher::Game>& games, const std::string& root = "sdmc:/switch")
{
    DIR* d = opendir(root.c_str());
    if (!d) return;
    std::vector<std::string> names;
    while (dirent* e = readdir(d))
        if (e->d_name[0] != '.') names.push_back(e->d_name);
    closedir(d);
    std::sort(names.begin(), names.end());
    for (const Title& t : kTitles)
    {
        const std::string builderFolder = !strcmp(t.code, "ECRA") ? "omega-nx" : "pokemon-alpha-sapphire";
        std::string best, bestNro;
        long long bestTime = -1;
        bool bestIsBuilders = false;
        for (const std::string& name : names)
        {
            const std::string folder = root + "/" + name;
            if (!IsFile(folder + "/game.3ds") || !IsDir(folder + "/titles/" + t.id)) continue;
            std::string nro;
            long long when = -1;
            if (DIR* f = opendir(folder.c_str()))
            {
                while (dirent* e = readdir(f))
                {
                    if (e->d_name[0] == '.' || !EndsWith(e->d_name, ".nro")) continue;
                    struct stat st;
                    if (stat((folder + "/" + e->d_name).c_str(), &st) != 0) continue;
                    if ((long long)st.st_mtime > when) { when = (long long)st.st_mtime; nro = e->d_name; }
                }
                closedir(f);
            }
            if (nro.empty()) continue;
            fprintf(stderr, "[3ds] %s in %s (%s)\n", t.name, folder.c_str(), nro.c_str());
            const bool builders = name == builderFolder;
            if (bestIsBuilders && !builders) continue;
            if (builders || when > bestTime) { best = folder; bestNro = nro; bestTime = when; bestIsBuilders = builders; }
        }
        if (best.empty()) continue;
        bool dup = false;
        for (const Launcher::Game& o : games) dup |= o.rom == best + "/" + bestNro;
        if (!dup) games.push_back(Make(t, best + "/" + bestNro, best));
        fprintf(stderr, "[3ds] %s: listed from %s\n", t.name, best.c_str());
    }
}

#ifdef __ANDROID__
// Android, with the Omega Ruby kit in the app (GameActivity.threeDsAvailable): a game already added
// (<app files>/3ds/<title id>/game.3ds, the kit's own folder), else a dump in the ROM folders, which
// is copied there the first time it is chosen (GameActivity.threeDsAdd).
inline void AddAndroid(const std::vector<std::string>& romDirs, const std::string& appDir, std::vector<Launcher::Game>& games)
{
    auto have = [&](const Title& t) {
        for (const Launcher::Game& o : games) if (o.platform == Launcher::N3DS && o.code == t.code) return true;
        return false;
    };
    for (const Title& t : kTitles)
    {
        const std::string folder = appDir + "/3ds/" + t.id;
        if (IsFile(folder + "/game.3ds")) games.push_back(Make(t, folder + "/game.3ds", folder));
    }
    for (const std::string& dir : romDirs)
    {
        DIR* d = opendir(dir.c_str());
        if (!d) continue;
        while (dirent* e = readdir(d))
        {
            const std::string path = dir + "/" + e->d_name;
            if (!EndsWith(path, ".3ds") || !IsFile(path)) continue;
            const Title* t = ById(DumpTitle(path));
            if (!t || have(*t)) continue;
            games.push_back(Make(*t, path, appDir + "/3ds/" + t->id));
            fprintf(stderr, "[3ds] %s: %s\n", t->name, path.c_str());
        }
        closedir(d);
    }
}
inline bool Added(const Launcher::Game& g) { return IsFile(g.folder + "/game.3ds"); }
// Whether the phone has room to copy the dump in (its size and 300 MB to spare); how much is
// needed, in GB, for the note when it has not.
inline bool RoomFor(const Launcher::Game& g, const std::string& appDir, double& needGb)
{
    struct stat st;
    if (stat(g.rom.c_str(), &st) != 0) return true; // (the copy itself will say what went wrong)
    struct statvfs vs;
    if (statvfs(appDir.c_str(), &vs) != 0) return true;
    const double need = (double)st.st_size + 300.0 * 1024 * 1024;
    needGb = need / (1024.0 * 1024 * 1024);
    return (double)vs.f_bavail * vs.f_frsize >= need;
}
#endif

#ifdef _WIN32
inline std::wstring W(const std::string& s) { return std::filesystem::u8path(s).wstring(); }
inline std::string ProgramDir(const std::string& appDir) { return appDir + "/3ds/program"; }
inline std::string Exe(const std::string& folder) { return folder + "/Pokemon ORAS.exe"; }
inline bool HaveProgram(const std::string& appDir) { std::error_code ec; return std::filesystem::is_regular_file(W(Exe(ProgramDir(appDir))), ec); }
inline bool Prepared(const Launcher::Game& g) { std::error_code ec; return std::filesystem::is_regular_file(W(g.folder + "/title.dll"), ec); }

// Windows: the dumps (.3ds) in the ROM folders, each its folder 3ds\<title id> beside FullDex.
inline void AddPc(const std::vector<std::string>& romDirs, const std::string& appDir, std::vector<Launcher::Game>& games)
{
    if (!HaveProgram(appDir)) return;
    for (const std::string& dir : romDirs)
    {
        std::error_code ec;
        for (std::filesystem::directory_iterator it(W(dir), ec), end; !ec && it != end; it.increment(ec))
        {
            if (!it->is_regular_file(ec)) continue;
            const std::string path = it->path().u8string();
            if (!EndsWith(path, ".3ds")) continue;
            const Title* t = ById(DumpTitle(path));
            if (!t) continue;
            bool dup = false;
            for (const Launcher::Game& o : games) dup |= o.platform == Launcher::N3DS && o.code == t->code;
            if (dup) continue;
            games.push_back(Make(*t, path, appDir + "/3ds/" + t->id));
            fprintf(stderr, "[3ds] %s: %s\n", t->name, path.c_str());
        }
    }
}

// The game's folder made (a copy of the program, when it has none yet) and told where the dump is.
inline bool MakeFolder(const std::string& appDir, const Launcher::Game& g)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(W(g.folder), ec);
    if (!fs::is_regular_file(W(Exe(g.folder)), ec))
    {
        // (File by file: a recursive copy into a folder that is there already fails here, "File exists".)
        ec.clear();
        const fs::path from = W(ProgramDir(appDir)), to = W(g.folder);
        for (fs::recursive_directory_iterator it(from, ec), end; !ec && it != end; it.increment(ec))
        {
            const fs::path dest = to / fs::relative(it->path(), from, ec);
            if (ec) break;
            if (it->is_directory(ec)) fs::create_directories(dest, ec);
            else if (!fs::exists(dest, ec)) fs::copy_file(it->path(), dest, ec);
        }
        if (ec) { fprintf(stderr, "[3ds] copying the program to %s: %s\n", g.folder.c_str(), ec.message().c_str()); return false; }
    }
    FILE* f = _wfopen(W(g.folder + "/rom.txt").c_str(), L"wb");
    if (!f) { fprintf(stderr, "[3ds] cannot write %s/rom.txt\n", g.folder.c_str()); return false; }
    fprintf(f, "%s\n", g.rom.c_str());
    fclose(f);
    return true;
}

// Making the game's code: one at a time, on a thread of its own, the launcher showing how it goes.
struct Prep
{
    std::mutex m;
    std::thread th;
    std::atomic<bool> running { false };
    bool finished = false, failed = false, told = true;
    int done = 0, total = 0;
    std::string code, name, step, error;
};
inline Prep& P() { static Prep p; return p; }

inline void StartPrepare(const std::string& appDir, const Launcher::Game& g)
{
    Prep& p = P();
    if (p.running) return;
    if (p.th.joinable()) p.th.join();
    {
        std::lock_guard<std::mutex> l(p.m);
        p.finished = p.failed = false;
        p.told = false;
        p.done = p.total = 0;
        p.code = g.code;
        p.name = g.name;
        p.step = "GETTING READY";
        p.error.clear();
    }
    p.running = true;
    p.th = std::thread([appDir, g]() {
        Prep& p = P();
        auto fail = [&](const std::string& why) {
            std::lock_guard<std::mutex> l(p.m);
            p.failed = true;
            p.error = why;
        };
        if (!MakeFolder(appDir, g)) fail("COULDN'T MAKE ITS FOLDER");
        else
        {
            SECURITY_ATTRIBUTES sa { sizeof sa, nullptr, TRUE };
            HANDLE rd = nullptr, wr = nullptr;
            CreatePipe(&rd, &wr, &sa, 0);
            SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
            STARTUPINFOW si {};
            si.cb = sizeof si;
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = wr;
            si.hStdError = wr;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            PROCESS_INFORMATION pi {};
            std::wstring cmd = L"\"" + W(Exe(g.folder)) + L"\" --prepare \"" + W(g.rom) + L"\"";
            const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, W(g.folder).c_str(), &si, &pi);
            CloseHandle(wr);
            if (!ok) fail("COULDN'T START IT");
            else
            {
                std::string line;
                char buf[512];
                DWORD got = 0;
                auto take = [&](const std::string& l) {
                    fprintf(stderr, "[3ds] prepare: %s\n", l.c_str());
                    std::lock_guard<std::mutex> lk(p.m);
                    int a = 0, b = 0;
                    if (l.rfind("progress ", 0) == 0 && sscanf(l.c_str() + 9, "%d %d", &a, &b) == 2) { p.done = a; p.total = b; }
                    else if (l.rfind("step ", 0) == 0)
                    {
                        const std::string s = l.substr(5);
                        p.step = s == "unpack-compiler" ? "UNPACKING THE COMPILER" : s == "translate" ? "TRANSLATING THE GAME'S CODE"
                               : s == "compile" ? "BUILDING THE GAME'S CODE" : s == "shaders" ? "FINDING THE SHADERS"
                               : s == "compile-shaders" ? "PREPARING THE SHADERS" : s == "finish" ? "FINISHING" : "WORKING";
                    }
                    else if (l.rfind("error ", 0) == 0) { p.failed = true; p.error = l.substr(6); }
                };
                while (ReadFile(rd, buf, sizeof buf, &got, nullptr) && got > 0)
                    for (DWORD i = 0; i < got; i++)
                    {
                        if (buf[i] == '\n') { if (!line.empty() && line.back() == '\r') line.pop_back(); take(line); line.clear(); }
                        else line += buf[i];
                    }
                if (!line.empty()) take(line);
                WaitForSingleObject(pi.hProcess, INFINITE);
                DWORD code = 1;
                GetExitCodeProcess(pi.hProcess, &code);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                if (code != 0 && !p.failed) fail("IT STOPPED (SEE data\\prepare.log IN ITS FOLDER)");
            }
            CloseHandle(rd);
        }
        {
            std::lock_guard<std::mutex> l(p.m);
            p.finished = true;
        }
        p.running = false;
    });
}

// What the launcher says about it: while it is being made, and once, when it ends. "" otherwise.
inline std::string Status()
{
    Prep& p = P();
    std::lock_guard<std::mutex> l(p.m);
    if (p.told) return "";
    std::string name = Launcher::TitleCase(p.name);
    for (char& c : name) c = (char)toupper((unsigned char)c);
    if (!p.finished)
    {
        std::string s = "PREPARING " + name + " (ONE TIME, 5 TO 15 MINUTES): " + p.step;
        if (p.total > 0) s += " " + std::to_string(p.done * 100 / p.total) + "%";
        return s;
    }
    p.told = true;
    return p.failed ? "COULDN'T PREPARE " + name + ": " + p.error : name + " IS READY: CHOOSE IT TO PLAY";
}
inline bool Preparing(const std::string& code) { Prep& p = P(); std::lock_guard<std::mutex> l(p.m); return p.running && p.code == code; }
inline bool Busy() { Prep& p = P(); std::lock_guard<std::mutex> l(p.m); return !p.told; }

// Plays it (its window, until it quits); "Restart game" (exit code 3) starts it again.
inline int Play(const std::string& appDir, const Launcher::Game& g)
{
    if (!MakeFolder(appDir, g)) return -1; // (rom.txt says where the dump is now)
    for (;;)
    {
        STARTUPINFOW si {};
        si.cb = sizeof si;
        PROCESS_INFORMATION pi {};
        std::wstring cmd = L"\"" + W(Exe(g.folder)) + L"\"";
        if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, W(g.folder).c_str(), &si, &pi)) return -1;
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        fprintf(stderr, "[3ds] %s ended (%lu)\n", g.name.c_str(), (unsigned long)code);
        if (code != 3) return (int)code;
    }
}
#endif
}
