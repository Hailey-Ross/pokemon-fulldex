#include "../frontend/rom_library.h"
#include <cassert>
#include <filesystem>
#include <string>

int main(int argc, char** argv)
{
    assert(argc == 2);
    namespace fs = std::filesystem;
    const fs::path root = fs::u8path(argv[1]);
    fs::create_directories(root / "DS" / "Platinum");
    fs::create_directories(root / "GBA" / "Emerald");
    for (const char* name : {"saves", "backups", ".hidden"}) fs::create_directories(root / name / "nested");
    fs::create_directories(root / "hacks" / "Renegade");
    fs::create_directories(root / "generated-cache" / "nested");
    const auto folders = RomLibrary::Folders(root.u8string(), (root / "generated-cache").u8string());
    assert(folders.size() == 7);
    assert(folders.front() == root.u8string());
    for (const auto& p : {root / "DS", root / "DS" / "Platinum", root / "GBA", root / "GBA" / "Emerald", root / "hacks", root / "hacks" / "Renegade"})
        assert(std::find(folders.begin(), folders.end(), p.u8string()) != folders.end());
    assert(RomLibrary::Folders("").empty());
    assert(RomLibrary::Folders((root / "missing").u8string()).empty());
    std::error_code ec;
    fs::create_directory_symlink(root, root / "loop", ec);
    if (!ec) assert(RomLibrary::Folders(root.u8string(), (root / "generated-cache").u8string()).size() == 7);
}
