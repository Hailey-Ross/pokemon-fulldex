#pragma once
// Steam library entries for Fulldex and its games, written to Steam's shortcuts.vdf.
// Nothing here runs unless the player chooses an Add To Steam item: there is no automatic
// entry, no add-all. When Steam is not found the caller falls back to the desktop shortcuts
// in shortcuts.h, so the platform-launcher item never dead-ends.
#include "vdf.h"
#include "shortcuts.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace SteamShortcuts
{
namespace fs = std::filesystem;

inline std::string Home()
{
    const char* h = getenv("HOME");
    return h && *h ? std::string(h) : std::string();
}

inline bool IsSteamOS()
{
#if defined(__linux__) && !defined(__ANDROID__)
    if (const char* deck = getenv("SteamDeck"); deck && !strcmp(deck, "1")) return true;
    if (FILE* f = fopen("/etc/os-release", "rb"))
    {
        char line[256];
        bool steam = false;
        while (fgets(line, sizeof line, f))
            if (!strncmp(line, "ID=steamos", 10) || !strncmp(line, "ID=\"steamos\"", 12)) { steam = true; break; }
        fclose(f);
        if (steam) return true;
    }
    return access("/etc/steamos-release", F_OK) == 0;
#else
    return false;
#endif
}

// The reopen instruction when Steam is holding its shortcuts file; OS-specific, with the
// SteamOS / Desktop Mode wording on the Deck.
inline const char* QuitSteamMessage()
{
#if defined(_WIN32)
    return "QUIT STEAM, OPEN FULLDEX FROM THE START MENU OR ITS SHORTCUT, THEN CHOOSE THIS AGAIN";
#elif defined(__APPLE__) && !defined(SSR_IOS)
    return "QUIT STEAM, OPEN FULLDEX FROM APPLICATIONS, THEN CHOOSE THIS AGAIN";
#elif defined(__linux__) && !defined(__ANDROID__)
    return IsSteamOS() ? "QUIT STEAM, OPEN FULLDEX IN DESKTOP MODE, THEN CHOOSE THIS AGAIN"
                       : "QUIT STEAM, OPEN FULLDEX FROM YOUR APPS MENU, THEN CHOOSE THIS AGAIN";
#else
    return "STEAM IS NOT AVAILABLE ON THIS DEVICE";
#endif
}

inline bool SteamRunning()
{
#if defined(__linux__) && !defined(__ANDROID__)
    return std::system("pgrep -x steam >/dev/null 2>&1") == 0;
#elif defined(__APPLE__) && !defined(SSR_IOS)
    return std::system("pgrep -x steam_osx >/dev/null 2>&1") == 0 || std::system("pgrep -x steam >/dev/null 2>&1") == 0;
#else
    return false;
#endif
}

// Every Steam account folder with a config folder. (Linux first; Windows/macOS detection is a
// later slice, and until then Available() is false there and the desktop fallback is used.)
// The shortcuts file may not exist yet — a Steam account with no non-Steam shortcuts — so it
// is a candidate to create, not a reason to say Steam is missing.
struct Account
{
    std::string shortcuts;
    fs::file_time_type time{};
};
inline std::vector<Account> Accounts()
{
    std::vector<Account> out;
#if defined(__linux__) && !defined(__ANDROID__)
    const std::string h = Home();
    if (!h.empty())
    {
        const std::string roots[] = {
            h + "/.local/share/Steam/userdata",
            h + "/.steam/steam/userdata",
            h + "/.steam/root/userdata",
            h + "/.var/app/com.valvesoftware.Steam/.local/share/Steam/userdata",
        };
        for (const std::string& root : roots)
        {
            std::error_code ec;
            if (!fs::is_directory(fs::u8path(root), ec)) continue;
            for (fs::directory_iterator it(fs::u8path(root), ec), end; !ec && it != end; it.increment(ec))
            {
                const fs::path config = it->path() / "config";
                std::error_code e2;
                if (!fs::is_directory(config, e2)) continue;
                const fs::path vdf = config / "shortcuts.vdf";
                std::error_code e3;
                const bool exists = fs::is_regular_file(vdf, e3);
                const auto time = fs::last_write_time(exists ? vdf : config, e3);
                out.push_back({ vdf.u8string(), e3 ? fs::file_time_type{} : time });
            }
        }
    }
#endif
    std::sort(out.begin(), out.end(), [](const Account& a, const Account& b) { return a.shortcuts < b.shortcuts; });
    out.erase(std::unique(out.begin(), out.end(), [](const Account& a, const Account& b) { return a.shortcuts == b.shortcuts; }), out.end());
    return out;
}

// The active shortcuts file: decided once at load and saved in the launcher's settings.cfg
// (steam_shortcuts, steam_account). The game menu's label is drawn every frame, so there is no
// folder scan or VDF read here. If the saved account's folder has gone (Steam moved or was
// reinstalled), Initialize detects it again and saves the new path.
inline std::string& ActivePath()
{
    static std::string path;
    return path;
}
inline const std::string& ActiveShortcuts() { return ActivePath(); }
inline bool Available() { return !ActivePath().empty(); }

inline void Initialize(const std::string& configPath)
{
    const std::string saved = Launcher::ConfigValue(configPath, "steam_shortcuts");
    std::error_code ec;
    if (!saved.empty() && fs::is_directory(fs::u8path(saved).parent_path(), ec))
    {
        ActivePath() = saved;
        return;
    }
    std::string best;
    std::string account;
    fs::file_time_type bestTime{};
    bool have = false;
    for (const Account& a : Accounts())
    {
        if (!have || a.time > bestTime)
        {
            best = a.shortcuts;
            account = fs::u8path(a.shortcuts).parent_path().parent_path().filename().u8string();
            bestTime = a.time;
            have = true;
        }
    }
    ActivePath() = best;
    if (!best.empty())
        SetConfigValues(configPath, { { "steam_shortcuts", best }, { "steam_account", account } });
}

inline bool ReadAll(const std::string& path, std::vector<uint8_t>& data)
{
    data.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) data.insert(data.end(), buf, buf + n);
    const bool ok = !ferror(f);
    fclose(f);
    return ok;
}
inline bool WriteAtomic(const std::string& path, const std::vector<uint8_t>& data)
{
    const std::string tmp = path + ".fulldex-tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = data.empty() || fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = fflush(f) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    if (!ok) { remove(tmp.c_str()); return false; }
    std::error_code ec;
    fs::rename(fs::u8path(tmp), fs::u8path(path), ec);
    if (ec) { remove(tmp.c_str()); return false; }
    return true;
}
inline std::string Backup(const std::string& path)
{
    std::error_code ec;
    if (!fs::is_regular_file(fs::u8path(path), ec)) return path; // (a new file: nothing to back up)
    char stamp[32];
    const time_t now = time(nullptr);
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", localtime(&now));
    const std::string out = path + ".fulldex-" + stamp + ".bak";
    if (fs::exists(fs::u8path(out), ec)) return out;
    fs::copy_file(fs::u8path(path), fs::u8path(out), fs::copy_options::overwrite_existing, ec);
    return ec ? std::string() : out;
}

inline const char* ProductName() { return "Pokémon Fulldex"; }
inline std::string GameTitle(const Launcher::Game& g)
{
    return !g.customName.empty() ? g.customName : g.hack ? g.name : Shortcuts::Title(g);
}
inline std::string Quote(const std::string& s) { return "\"" + s + "\""; }
inline std::string Unquote(std::string s)
{
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
    return s;
}
inline std::string ArgsFor(const Launcher::Game& g) { return "--rom " + Quote(g.rom); }
inline uint32_t AppId(const std::string& exe, const std::string& name)
{
    const std::string key = Unquote(exe) + name;
    return Shortcuts::Crc32((const uint8_t*)key.data(), key.size()) | 0x80000000u;
}
inline std::string RomFromArgs(const std::string& args)
{
    const size_t at = args.find("--rom ");
    if (at == std::string::npos) return std::string();
    std::string v = args.substr(at + 6);
    while (!v.empty() && v.front() == ' ') v.erase(0, 1);
    if (!v.empty() && v.front() == '"')
    {
        const size_t end = v.find('"', 1);
        return end == std::string::npos ? v.substr(1) : v.substr(1, end - 1);
    }
    const size_t end = v.find(' ');
    return end == std::string::npos ? v : v.substr(0, end);
}

inline int64_t NextKey(const Vdf::Node& root)
{
    int64_t max = -1;
    for (const Vdf::Node& c : root.children)
    {
        if (c.kind != Vdf::Node::Object) continue;
        char* end = nullptr;
        const long long v = strtoll(c.key.c_str(), &end, 10);
        if (end && *end == 0 && v > max) max = v;
    }
    return max + 1;
}
inline Vdf::Node* GameEntry(Vdf::Node& root, const std::string& rom)
{
    for (Vdf::Node& c : root.children)
        if (c.kind == Vdf::Node::Object && RomFromArgs(Vdf::String(c, "LaunchOptions")) == rom) return &c;
    return nullptr;
}
inline Vdf::Node* LauncherEntry(Vdf::Node& root)
{
    // (The product name, and a player's earlier hand-made "… Fulldex" entry: only an entry
    // with no --rom/--game option can be the launcher itself.)
    for (Vdf::Node& c : root.children)
        if (c.kind == Vdf::Node::Object)
        {
            const std::string name = Vdf::String(c, "AppName");
            const bool named = name == ProductName() ||
                               (name.find("Fulldex") != std::string::npos && name.find("Pok") != std::string::npos);
            if (named && RomFromArgs(Vdf::String(c, "LaunchOptions")).empty()) return &c;
        }
    return nullptr;
}

inline void FillEntry(Vdf::Node& e, const std::string& title, const std::string& exe, const std::string& args,
                      const std::string& icon, uint32_t appid)
{
    std::string dir = fs::u8path(exe).parent_path().u8string();
    if (dir.empty()) dir = ".";
    Vdf::SetInt32(e, "appid", (int32_t)appid);
    Vdf::SetString(e, "AppName", title);
    Vdf::SetString(e, "Exe", Quote(exe));
    Vdf::SetString(e, "StartDir", dir);
    Vdf::SetString(e, "icon", icon);
    if (!Vdf::Child(e, "ShortcutPath")) Vdf::SetString(e, "ShortcutPath", "");
    Vdf::SetString(e, "LaunchOptions", args);
    if (!Vdf::Child(e, "IsHidden")) Vdf::SetInt32(e, "IsHidden", 0);
    Vdf::SetInt32(e, "AllowDesktopConfig", 1);
    Vdf::SetInt32(e, "AllowOverlay", 1);
    if (!Vdf::Child(e, "openvr")) Vdf::SetInt32(e, "openvr", 0);
    if (!Vdf::Child(e, "Devkit")) Vdf::SetInt32(e, "Devkit", 0);
    if (!Vdf::Child(e, "DevkitGameID")) Vdf::SetString(e, "DevkitGameID", "");
    if (!Vdf::Child(e, "DevkitOverrideAppID")) Vdf::SetInt32(e, "DevkitOverrideAppID", 0);
    if (!Vdf::Child(e, "LastPlayTime")) Vdf::SetInt32(e, "LastPlayTime", 0);
    if (!Vdf::Child(e, "FlatpakAppID")) Vdf::SetString(e, "FlatpakAppID", "");
    if (!Vdf::Child(e, "sortas")) Vdf::SetString(e, "sortas", "");
    Vdf::SetObject(e, "tags");
}

inline std::string GridDir(const std::string& shortcutsPath)
{
    return (fs::u8path(shortcutsPath).parent_path() / "grid").u8string();
}
inline std::string RemoveGrid(const std::string& shortcutsPath, uint32_t appid)
{
    const std::string grid = GridDir(shortcutsPath);
    const std::string base = grid + "/" + std::to_string(appid);
    std::error_code ec;
    for (const char* suffix : { "_icon.png", "_p.png", "_hero.png", "_logo.png" })
        fs::remove(fs::u8path(base + suffix), ec);
    return std::string();
}
// The entry's icon: the game's picture, or for the launcher the AppImage's own icon beside it.
inline std::string WriteIcon(const std::string& shortcutsPath, uint32_t appid, const Launcher::Game* game,
                             const std::string& appPath)
{
    const std::string grid = GridDir(shortcutsPath);
    std::error_code ec;
    fs::create_directories(fs::u8path(grid), ec);
    const std::string path = grid + "/" + std::to_string(appid) + "_icon.png";
    std::vector<uint32_t> px;
    if (game) px = Shortcuts::Picture(*game, 256);
    if (!px.empty())
        return Shortcuts::WriteFile(path, Shortcuts::Png(px, 256, 256)) ? path : std::string();
    // (The launcher: the AppImage's .icons/<name>, which its .desktop entry uses too.)
    const fs::path app = fs::u8path(appPath);
    const fs::path own = app.parent_path() / ".icons" / app.stem();
    std::error_code c2;
    if (fs::is_regular_file(own, c2))
    {
        fs::copy_file(own, fs::u8path(path), fs::copy_options::overwrite_existing, c2);
        if (!c2) return path;
    }
    return std::string();
}

inline bool AddEntry(const std::string& shortcutsPath, const std::string& title, const std::string& exe,
                     const std::string& args, const Launcher::Game* game, std::string& error)
{
    std::vector<uint8_t> data;
    Vdf::Node root;
    std::error_code ec;
    if (ReadAll(shortcutsPath, data))
    {
        if (!Vdf::Parse(data, root)) { error = "CANNOT READ STEAM'S SHORTCUTS"; return false; }
    }
    else if (fs::exists(fs::u8path(shortcutsPath), ec))
    {
        error = "CANNOT READ STEAM'S SHORTCUTS";
        return false;
    }
    else
    {
        // (A Steam account with no non-Steam shortcuts yet: the first one creates the file.)
        root.kind = Vdf::Node::Object;
        root.key = "shortcuts";
    }
    Vdf::Node* entry = game ? GameEntry(root, game->rom) : LauncherEntry(root);
    const uint32_t appid = AppId(exe, title);
    const std::string icon = WriteIcon(shortcutsPath, appid, game, exe);
    if (!entry)
    {
        Vdf::Node made;
        made.kind = Vdf::Node::Object;
        made.key = std::to_string(NextKey(root));
        root.children.push_back(std::move(made));
        entry = &root.children.back();
    }
    FillEntry(*entry, title, exe, args, icon, appid);
    const std::string backup = Backup(shortcutsPath);
    if (backup.empty()) { error = "CANNOT BACK UP STEAM'S SHORTCUTS"; return false; }
    if (!WriteAtomic(shortcutsPath, Vdf::Write(root))) { error = "CANNOT WRITE STEAM'S SHORTCUTS"; return false; }
    return true;
}

inline bool RemoveEntry(const std::string& shortcutsPath, const Launcher::Game* game, std::string& error)
{
    std::vector<uint8_t> data;
    if (!ReadAll(shortcutsPath, data))
    {
        std::error_code ec;
        if (!fs::exists(fs::u8path(shortcutsPath), ec)) return true; // (nothing to remove)
        error = "CANNOT READ STEAM'S SHORTCUTS";
        return false;
    }
    Vdf::Node root;
    if (!Vdf::Parse(data, root)) { error = "CANNOT READ STEAM'S SHORTCUTS"; return false; }
    Vdf::Node* entry = game ? GameEntry(root, game->rom) : LauncherEntry(root);
    if (!entry) return true;
    const uint32_t appid = AppId(Vdf::String(*entry, "Exe"), Vdf::String(*entry, "AppName"));
    root.children.erase(std::remove_if(root.children.begin(), root.children.end(), [&](const Vdf::Node& n) { return &n == entry; }),
                        root.children.end());
    int64_t key = 0;
    for (Vdf::Node& n : root.children)
        if (n.kind == Vdf::Node::Object) n.key = std::to_string(key++);
    const std::string backup = Backup(shortcutsPath);
    if (backup.empty()) { error = "CANNOT BACK UP STEAM'S SHORTCUTS"; return false; }
    if (!WriteAtomic(shortcutsPath, Vdf::Write(root))) { error = "CANNOT WRITE STEAM'S SHORTCUTS"; return false; }
    RemoveGrid(shortcutsPath, appid);
    return true;
}

// The per-game state, cached by the shortcuts file's modification time (labels are drawn every
// frame; the file is small but not that small).
struct State
{
    std::string path;
    fs::file_time_type time{};
    bool valid = false;
    bool launcher = false;
    std::vector<std::string> roms;
};
inline State& Cached() { static State s; return s; }
inline void Refresh()
{
    State& s = Cached();
    const std::string path = ActiveShortcuts();
    if (path.empty()) { s = State(); return; }
    std::error_code ec;
    const auto time = fs::last_write_time(fs::u8path(path), ec);
    if (s.valid && s.path == path && !ec && s.time == time) return;
    s = State();
    s.path = path;
    s.time = ec ? fs::file_time_type{} : time;
    s.valid = true;
    std::vector<uint8_t> data;
    Vdf::Node root;
    if (!ReadAll(path, data) || !Vdf::Parse(data, root)) return;
    for (const Vdf::Node& e : root.children)
    {
        if (e.kind != Vdf::Node::Object) continue;
        const std::string name = Vdf::String(e, "AppName");
        const std::string args = Vdf::String(e, "LaunchOptions");
        const std::string rom = RomFromArgs(args);
        if (!rom.empty()) s.roms.push_back(rom);
        else if (name == ProductName() || (name.find("Fulldex") != std::string::npos && name.find("Pok") != std::string::npos))
            s.launcher = true;
    }
}
inline bool HasGame(const std::string& rom)
{
    Refresh();
    for (const std::string& r : Cached().roms)
        if (r == rom) return true;
    return false;
}
inline bool HasLauncher()
{
    Refresh();
    return Cached().launcher;
}

inline std::string AddGame(const Launcher::Game& g)
{
    const std::string path = ActiveShortcuts();
    if (path.empty()) return "STEAM NOT FOUND";
    if (SteamRunning()) return QuitSteamMessage();
    const std::string exe = Shortcuts::ExecutablePath();
    if (exe.empty()) return "NO PROGRAM PATH";
    std::string error;
    return AddEntry(path, GameTitle(g), exe, ArgsFor(g), &g, error) ? "ADDED TO STEAM" : error;
}
inline std::string RemoveGame(const Launcher::Game& g)
{
    const std::string path = ActiveShortcuts();
    if (path.empty()) return "STEAM NOT FOUND";
    if (SteamRunning()) return QuitSteamMessage();
    std::string error;
    return RemoveEntry(path, &g, error) ? "REMOVED FROM STEAM" : error;
}
inline std::string AddLauncher()
{
    const std::string path = ActiveShortcuts();
    if (path.empty()) return "STEAM NOT FOUND";
    if (SteamRunning()) return QuitSteamMessage();
    const std::string exe = Shortcuts::ExecutablePath();
    if (exe.empty()) return "NO PROGRAM PATH";
    std::string error;
    return AddEntry(path, ProductName(), exe, "", nullptr, error) ? "ADDED TO STEAM" : error;
}
inline std::string RemoveLauncher()
{
    const std::string path = ActiveShortcuts();
    if (path.empty()) return "STEAM NOT FOUND";
    if (SteamRunning()) return QuitSteamMessage();
    std::string error;
    return RemoveEntry(path, nullptr, error) ? "REMOVED FROM STEAM" : error;
}

// The game menu's platform-launcher item: Steam when found, otherwise the OS desktop shortcut.
inline std::string PlatformEntryLabel(const std::string& dir, const Launcher::Game& g)
{
    if (Available()) return HasGame(g.rom) ? "Remove From Steam" : "Add To Steam";
    return Shortcuts::Exists(dir, g) ? "Remove From Desktop" : "Add To Desktop";
}
inline std::string PlatformEntryDo(const std::string& dir, const Launcher::Game& g)
{
    if (Available())
    {
        if (SteamRunning()) return QuitSteamMessage();
        return HasGame(g.rom) ? RemoveGame(g) : AddGame(g);
    }
    std::string where;
    if (Shortcuts::Exists(dir, g))
    {
        Shortcuts::Remove(dir, g, where);
        return "REMOVED " + where;
    }
    const int made = Shortcuts::Make(dir, std::vector<Launcher::Game>{ g }, where);
    if (made > 0) return "ADDED " + where;
    return where.rfind("NO ", 0) == 0 ? where : std::string("COULD NOT MAKE THE SHORTCUT");
}
inline std::string LauncherDo()
{
    if (!Available()) return "STEAM NOT FOUND";
    if (SteamRunning()) return QuitSteamMessage();
    return HasLauncher() ? RemoveLauncher() : AddLauncher();
}
}
