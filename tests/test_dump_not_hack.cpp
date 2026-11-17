#include "../frontend/launcher.h"
#include <cassert>
#include <fstream>
#include <string>

// FullDex 1.6.1: a full-size dump that matches no listed release is its game, not a hack (a Spanish
// HeartGold was read-only in the Hub); a trimmed one is a hack; a save played as a "hack" is put back.
static void make(const std::string& path, uint64_t size)
{
    std::ofstream out(path, std::ios::binary);
    std::string header(0x200, '\0');
    header.replace(0x0C, 4, "IPKS");
    out << header;
    out.seekp((std::streamoff)size - 1);
    out.put('\x5A');
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::string dir = argv[1];
    const std::string full = dir + "/full.nds", trimmed = dir + "/trimmed.nds";
    make(full, 134217728u);
    make(trimmed, 100000000u);
    RomHacks::Cache cache;
    assert(RomHacks::CheckDs(full, "IPKS", cache, true) != RomHacks::Kind::Modified);
    assert(RomHacks::CheckDs(trimmed, "IPKS", cache, true) == RomHacks::Kind::Modified);
    // The save put back: the "hack" copy newer and different, the game's kept as .before-1.6.1.
    const std::string saves = dir + "/saves";
    std::filesystem::create_directories(saves);
    { std::ofstream(saves + "/heartgold-es.sav") << "old"; }
    { std::ofstream(saves + "/" + Launcher::HackSave(full)) << "played since 1.6"; }
    std::filesystem::last_write_time(saves + "/heartgold-es.sav", std::filesystem::file_time_type::clock::now() - std::chrono::hours(2));
    Launcher::TakeBackHackSave(saves, Launcher::HackSave(full), "heartgold-es.sav");
    auto read = [](const std::string& p) { std::ifstream f(p); return std::string((std::istreambuf_iterator<char>(f)), {}); };
    assert(read(saves + "/heartgold-es.sav") == "played since 1.6");
    assert(read(saves + "/heartgold-es.sav.before-1.6.1") == "old");
    // Once put back it is not done again (the game's save is now the newer).
    { std::ofstream(saves + "/heartgold-es.sav") << "played on"; }
    Launcher::TakeBackHackSave(saves, Launcher::HackSave(full), "heartgold-es.sav");
    assert(read(saves + "/heartgold-es.sav") == "played on");
    puts("full-size dumps are their game, trimmed ones hacks, and a 1.6 hack save is put back");
}
