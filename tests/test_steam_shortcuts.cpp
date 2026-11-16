// The Steam writer: a fresh Steam account (no shortcuts.vdf yet), adding and updating an
// entry, keeping the player's existing shortcuts, removing, the Steam-running guard, and the
// automatic desktop fallback when Steam is not found.
#include "steam_shortcuts.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

static int failures = 0;
static void Check(bool ok, const std::string& what)
{
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what.c_str());
    fflush(stdout);
    if (!ok) failures++;
}

static Launcher::Game MakeGame(const std::string& rom, const std::string& code, const std::string& name)
{
    Launcher::Game g;
    g.rom = rom;
    g.code = code;
    g.name = name;
    g.known = true;
    g.folder = fs::path(rom).parent_path().u8string();
    g.saveName = "test.sav";
    return g;
}

static void WriteText(const fs::path& path, const std::string& text, bool executable = false)
{
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary);
    f << text;
    f.close();
    if (executable) chmod(path.c_str(), 0755);
}

static std::vector<Vdf::Node*> Entries(Vdf::Node& root)
{
    std::vector<Vdf::Node*> out;
    for (Vdf::Node& n : root.children)
        if (n.kind == Vdf::Node::Object) out.push_back(&n);
    return out;
}

static Vdf::Node* ByName(Vdf::Node& root, const std::string& name)
{
    for (Vdf::Node& n : root.children)
        if (n.kind == Vdf::Node::Object && Vdf::String(n, "AppName") == name) return &n;
    return nullptr;
}

static Vdf::Node* ByRom(Vdf::Node& root, const std::string& rom)
{
    for (Vdf::Node& n : root.children)
        if (n.kind == Vdf::Node::Object && SteamShortcuts::RomFromArgs(Vdf::String(n, "LaunchOptions")) == rom) return &n;
    return nullptr;
}

static bool Load(const fs::path& path, Vdf::Node& root)
{
    std::vector<uint8_t> data;
    return SteamShortcuts::ReadAll(path.string(), data) && Vdf::Parse(data, root);
}

int main()
{
    const fs::path temp = fs::temp_directory_path() / ("fulldex-steam-test-" + std::to_string((long)getpid()));
    fs::remove_all(temp);
    const fs::path home = temp / "home";
    const fs::path steam = home / ".local/share/Steam";
    const fs::path config = steam / "userdata/123/config";
    fs::create_directories(config);
    // (Account 123 signed in last; account 456 has a newer shortcuts file but is not signed in.
    // The refresh must choose the signed-in account, not merely the newest file.)
    WriteText(steam / "config/loginusers.vdf",
              "\"users\"\n{\n    \"76561197960265851\"\n    {\n        \"MostRecent\" \"1\"\n    }\n"
              "    \"76561197960266184\"\n    {\n        \"MostRecent\" \"0\"\n    }\n}\n");
    WriteText(steam / "userdata/456/config/shortcuts.vdf", "");
    const fs::path app = temp / "Fulldex.AppImage";
    WriteText(app, "fake appimage", true);
    setenv("HOME", home.string().c_str(), 1);
    setenv("APPIMAGE", app.string().c_str(), 1);

    const std::string oldPath = getenv("PATH") ? getenv("PATH") : "";
    const fs::path notRunning = temp / "bin/notrunning";
    const fs::path running = temp / "bin/running";
    WriteText(notRunning / "pgrep", "#!/bin/sh\nexit 1\n", true);
    WriteText(running / "pgrep", "#!/bin/sh\nexit 0\n", true);
    auto steamRunning = [&](bool on) { setenv("PATH", ((on ? running : notRunning).string() + ":" + oldPath).c_str(), 1); };
    steamRunning(false);

    const fs::path vdf = config / "shortcuts.vdf";
    const fs::path settings = temp / "settings.cfg";
    SteamShortcuts::Initialize(settings.string());
    Check(SteamShortcuts::Available(), "a fresh Steam account is found without shortcuts.vdf");
    Check(Launcher::ConfigValue(settings.string(), "steam_shortcuts") == vdf.string(), "saves the Steam shortcuts path in settings.cfg");
    Check(Launcher::ConfigValue(settings.string(), "steam_account") == "123", "saves the Steam account in settings.cfg");
    Check(SteamShortcuts::RefreshAccount(settings.string()) == "STEAM ACCOUNT 123", "refresh finds the signed-in account, not the newest file");
    Check(Launcher::ConfigValue(settings.string(), "steam_account") == "123", "refresh keeps the account in settings.cfg");
    Check(!fs::exists(vdf), "there is no shortcuts.vdf yet");

    Launcher::Game black = MakeGame((temp / "roms/black.nds").string(), "IREO", "POKEMON BLACK");
    Launcher::Game white2 = MakeGame((temp / "roms/white2.nds").string(), "IREO", "POKEMON WHITE 2");
    const std::string blackTitle = SteamShortcuts::GameTitle(black);

    Check(SteamShortcuts::AddGame(black) == "ADDED TO STEAM", "adds the first game to Steam");
    Check(fs::exists(vdf), "creates shortcuts.vdf");
    Vdf::Node root;
    Check(Load(vdf, root), "reads the new shortcuts.vdf");
    Check(Entries(root).size() == 1, "the new file has one entry");
    Vdf::Node* entry = ByRom(root, black.rom);
    Check(entry != nullptr, "the entry is keyed by the ROM path");
    Check(entry && Vdf::String(*entry, "AppName") == blackTitle, "the entry has the game's title");
    Check(entry && Vdf::String(*entry, "LaunchOptions") == SteamShortcuts::ArgsFor(black), "it launches --rom with the path");
    Check(entry && Vdf::String(*entry, "Exe") == SteamShortcuts::Quote(app.string()), "it points at the AppImage");
    Check(SteamShortcuts::HasGame(black.rom), "the menu state sees the game");
    Check(SteamShortcuts::PlatformEntryLabel(temp.string(), black) == "Remove From Steam", "the menu label offers Remove");
    Check(SteamShortcuts::AddGame(black) == "ADDED TO STEAM", "adding the same game again updates it");
    root = Vdf::Node();
    Check(Load(vdf, root) && Entries(root).size() == 1, "updating does not duplicate the entry");

    Check(SteamShortcuts::AddGame(white2) == "ADDED TO STEAM", "adds a second game");
    root = Vdf::Node();
    Check(Load(vdf, root) && Entries(root).size() == 2, "both games are in the file");
    Check(SteamShortcuts::RemoveGame(black) == "REMOVED FROM STEAM", "removes one game");
    Check(!SteamShortcuts::HasGame(black.rom), "the removed game is gone from the menu state");
    Check(SteamShortcuts::HasGame(white2.rom), "the other game remains");
    root = Vdf::Node();
    Check(Load(vdf, root) && Entries(root).size() == 1, "only the other game remains in the file");
    Check(SteamShortcuts::RemoveGame(white2) == "REMOVED FROM STEAM", "removes the other game");
    root = Vdf::Node();
    Check(Load(vdf, root) && Entries(root).empty(), "the file is empty of our entries");

    // A player's own entry is never lost.
    {
        Vdf::Node fixture;
        fixture.kind = Vdf::Node::Object;
        fixture.key = "shortcuts";
        Vdf::Node existing;
        existing.kind = Vdf::Node::Object;
        existing.key = "0";
        Vdf::SetString(existing, "AppName", "Existing Shortcut");
        Vdf::SetString(existing, "Exe", "\"/usr/bin/true\"");
        Vdf::SetString(existing, "LaunchOptions", "");
        fixture.children.push_back(std::move(existing));
        Check(SteamShortcuts::WriteAtomic(vdf.string(), Vdf::Write(fixture)), "writes a fixture with an existing shortcut");
    }
    Check(SteamShortcuts::AddGame(black) == "ADDED TO STEAM", "adds ours beside the existing shortcut");
    root = Vdf::Node();
    Check(Load(vdf, root) && Entries(root).size() == 2, "both shortcuts are present");
    Check(ByName(root, "Existing Shortcut") != nullptr, "the player's existing shortcut is preserved");
    Check(SteamShortcuts::RemoveGame(black) == "REMOVED FROM STEAM", "removes ours");
    root = Vdf::Node();
    Check(Load(vdf, root) && Entries(root).size() == 1 && ByName(root, "Existing Shortcut") != nullptr,
          "removing ours leaves the existing shortcut");

    Check(!SteamShortcuts::HasLauncher(), "the launcher is not in Steam yet");
    Check(SteamShortcuts::AddLauncher() == "ADDED TO STEAM", "adds the launcher itself");
    Check(SteamShortcuts::HasLauncher(), "the launcher is in Steam");
    root = Vdf::Node();
    Check(Load(vdf, root) && ByName(root, SteamShortcuts::ProductName()) != nullptr, "the launcher entry has the product name");
    Check(SteamShortcuts::RemoveLauncher() == "REMOVED FROM STEAM", "removes the launcher");
    Check(!SteamShortcuts::HasLauncher(), "the launcher is gone from Steam");

    // Steam running: refuse before touching the file.
    steamRunning(true);
    Launcher::Game emerald = MakeGame((temp / "roms/emerald.nds").string(), "IREO", "POKEMON EMERALD");
    Check(SteamShortcuts::AddGame(emerald) == SteamShortcuts::QuitSteamMessage(), "refuses to write while Steam runs");
    Check(!SteamShortcuts::HasGame(emerald.rom), "the refused add did not write");
    root = Vdf::Node();
    Check(Load(vdf, root) && Entries(root).size() == 1, "the file still has only the existing shortcut");
    steamRunning(false);

    // No Steam: the same item falls back to the desktop shortcut.
    const fs::path home2 = temp / "home2";
    fs::create_directories(home2);
    const fs::path launcherDir = temp / "launcher";
    fs::create_directories(launcherDir);
    setenv("HOME", home2.string().c_str(), 1);
    SteamShortcuts::Initialize((temp / "settings-home2.cfg").string());
    Check(!SteamShortcuts::Available(), "Steam is not found in a home without userdata");
    Check(SteamShortcuts::RefreshAccount((temp / "settings-home2.cfg").string()) == "STEAM NOT FOUND", "refresh reports no Steam");
    Check(Launcher::ConfigValue((temp / "settings-home2.cfg").string(), "steam_shortcuts").empty(), "refresh clears a stale path");
    Check(SteamShortcuts::PlatformEntryLabel(launcherDir.string(), black) == "Add To Desktop", "the menu label falls back to Desktop");
    const std::string added = SteamShortcuts::PlatformEntryDo(launcherDir.string(), black);
    Check(added.rfind("ADDED", 0) == 0, "the fallback adds a desktop shortcut");
    Check(Shortcuts::Exists(launcherDir.string(), black), "the desktop shortcut exists");
    Check(SteamShortcuts::PlatformEntryLabel(launcherDir.string(), black) == "Remove From Desktop", "the label offers Remove From Desktop");
    const std::string removed = SteamShortcuts::PlatformEntryDo(launcherDir.string(), black);
    Check(removed.rfind("REMOVED", 0) == 0, "the fallback removes the desktop shortcut");
    Check(!Shortcuts::Exists(launcherDir.string(), black), "the desktop shortcut is gone");

    setenv("PATH", oldPath.c_str(), 1);
    fs::remove_all(temp);
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
