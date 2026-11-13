// The cheats built into Fulldex (builtin_cheats.h): every DS release in the table loads its codes
// into the folder POPULAR CHEATS, all off; a code turned on stays on after loading again; a game
// with no entry gets none. Build: g++ -std=c++17 -Ifrontend -ImelonDS/src tests/test_builtin_cheats.cpp
//   melonDS/src/ARCodeFile.cpp melonDS/src/ARDatabaseDAT.cpp melonDS/src/CRC32.cpp
#include "cheats.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "Platform.h"
// (No .mch or usrcheat.dat here, so melonDS's file functions are never called: stubs to link.)
namespace melonDS::Platform
{
FileHandle* OpenFile(const std::string&, FileMode) { return nullptr; }
bool CloseFile(FileHandle*) { return true; }
bool IsEndOfFile(FileHandle*) { return true; }
bool FileReadLine(char*, int, FileHandle*) { return false; }
u64 FilePosition(FileHandle*) { return 0; }
bool FileSeek(FileHandle*, s64, FileSeekOrigin) { return false; }
u64 FileRead(void*, u64, u64, FileHandle*) { return 0; }
u64 FileWriteFormatted(FileHandle*, const char*, ...) { return 0; }
u64 FileLength(FileHandle*) { return 0; }
void Log(LogLevel, const char*, ...) {}
}
int main()
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "fulldex-builtin-cheats-test";
    fs::remove_all(root);
    fs::create_directories(root);
    const std::string folder = (root / "cheats").string();
    int games = 0;
    for (const BuiltinCheats::Game& g : BuiltinCheats::Games)
    {
        Cheats::List list;
        list.Load(folder, g.code, nullptr);
        assert((int)list.items.size() == g.count && g.count >= 14);
        assert(list.folders.size() == 1 && list.folders[0].name == "POPULAR CHEATS");
        assert(list.On() == 0);
        for (const Cheats::Cheat& c : list.items)
            assert(!c.code.empty() && c.code.size() % 2 == 0 && c.category == "POPULAR CHEATS" && c.key.rfind("fulldex:", 0) == 0);
        games++;
    }
    // On stays on, and only for that game.
    {
        Cheats::List list; list.Load(folder, "IPKP", nullptr);
        size_t money = 0; while (list.items[money].key != "fulldex:Max Money") money++;
        list.Toggle(money);
        Cheats::List again; again.Load(folder, "IPKP", nullptr);
        assert(again.On() == 1 && again.items[money].on);
        Cheats::List other; other.Load(folder, "IPGP", nullptr);
        assert(other.On() == 0);
    }
    { Cheats::List none; none.Load(folder, "BPEE", nullptr); assert(none.items.empty() && none.folders.empty()); }
    // Hacks sharing a game code must not inherit the base game's enabled cheats.
    {
        Cheats::List hack;
        hack.Load(folder, "IPKP", nullptr, "/roms/First Hack.nds", true);
        assert(hack.On() == 0);
        hack.Toggle(0);
        Cheats::List other;
        other.Load(folder, "IPKP", nullptr, "C:\\roms\\Second Hack.nds", true);
        assert(other.On() == 0 && other.OnFile() != hack.OnFile());
        Cheats::List again;
        again.Load(folder, "IPKP", nullptr, "/roms/First Hack.nds", true);
        assert(again.On() == 1 && again.items[0].on);
    }
    fs::create_directories(folder);
    std::ofstream(folder + "/IPKP.txt") << "Generic code\n02000000 00000001\n";
    std::ofstream(folder + "/First Hack.txt") << "Custom code\n02000000 00000002\n";
    std::ofstream(folder + "/Second Hack.txt") << "Custom code\n02000000 00000003\n";
    {
        Cheats::List first, second, plain;
        first.Load(folder, "IPKP", nullptr, "/roms/First Hack.nds", true);
        second.Load(folder, "IPKP", nullptr, "C:\\roms\\Second Hack.nds", true);
        plain.Load(folder, "IPKP", nullptr, "/roms/Original.nds");
        assert(first.items.back().code.back() == 2);
        assert(second.items.back().code.back() == 3);
        assert(plain.items.back().code.back() == 1);
        assert(first.items.back().key == second.items.back().key);
        first.Toggle(first.items.size() - 1);
        second.Load(folder, "IPKP", nullptr, "/roms/Second Hack.nds", true);
        assert(!second.items.back().on);
        first.Load(folder, "IPKP", nullptr, "/roms/First Hack.nds", true);
        assert(first.items.back().on);
        assert(plain.OnFile() == folder + "/IPKP-on.txt");
        // Explicit ROM-specific files also work without hack detection.
        second.Load(folder, "IPKP", nullptr, "/roms/Second Hack.nds");
        assert(second.items.back().code.back() == 3 && second.On() == 0);
    }
    fs::remove_all(root);
    std::cout << "PASS: built-in cheats for " << games << " DS releases, all off, kept on when turned on\n";
}
