#include "../frontend/launcher.h"
#include <cassert>
#include <string>

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::string path = argv[1];
    uint32_t fp = 0;
    uint8_t revision = 0;
    uint64_t size = 0;
    assert(RomHacks::DsFingerprint(path, fp, revision, size));
    assert(fp == 0x749916A6u); // Synthetic bytes matching the known partial fingerprint.
    assert(size == 268435456u + 30720u); // Same size as the #113 diagnostic log.
    RomHacks::Cache fresh;
    assert(RomHacks::CheckDs(path, "IRBO", fresh, true) == RomHacks::Kind::Modified);
    RomHacks::Cache old;
    const std::string key = RomHacks::Cache::Key(path, "ds");
    old.Put(key, "clean");
    assert(RomHacks::CheckDs(path, "IRBO", old, true) == RomHacks::Kind::Modified);
    std::string value;
    assert(old.Get(key, value) && value == "modified");
    assert(RomHacks::CheckDs(path + ".missing", "IRBO", old, true) == RomHacks::Kind::Unknown);
    const std::string saves = path.substr(0, path.find_last_of("/\\"));
    Launcher::BeginScan(saves);
    Launcher::Game game;
    assert(Launcher::ReadGame(path, saves, game));
    assert(game.hack && game.saveName != "black.sav");
    assert(game.saveName == Launcher::HackSave(path));
    Launcher::EndScan();
    // The original-sized fingerprint shortcut still works for known clean releases.
    std::filesystem::resize_file(path, 268435456u);
    RomHacks::Cache normal;
    assert(RomHacks::CheckDs(path, "IRBO", normal, true) == RomHacks::Kind::Clean);
}
