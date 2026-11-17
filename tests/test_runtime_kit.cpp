#include "n3ds.h"
#include <cassert>
#include <fstream>
#include <iterator>

static void Put(const std::filesystem::path& path, const char* data)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << data;
}
static std::string Read(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
int main(int argc, char** argv)
{
    assert(argc==2);
    namespace fs=std::filesystem;
    const fs::path root=fs::u8path(argv[1]);
    const fs::path kit=fs::u8path(N3ds::ProgramDir(root.string()));
    const fs::path sourceExe=fs::u8path(N3ds::Exe(kit.string()));
    Put(sourceExe,"master executable");
    Put(kit/"tools/w64devkit-x64.7z.exe","compiler archive");
    Put(kit/"tools/3dsrecomp","translator");
#ifndef _WIN32
    fs::permissions(kit/"tools/3dsrecomp",fs::perms::owner_all);
#endif
    Launcher::Game game;
    game.folder=(root/"3ds/000400000011C400").string();
    game.rom=(root/"synthetic.3ds").string();
    Put(fs::u8path(game.rom),"synthetic header only");
    const fs::path dest=fs::u8path(game.folder);
    const fs::path destExe=fs::u8path(N3ds::Exe(game.folder));
    Put(destExe,"existing executable"); // partial installation: executable arrived, tools did not
    Put(dest/"settings.ini","player settings");
    Put(dest/"title.so","prepared game");
    Put(dest/"data/user/main","player save");
    assert(N3ds::MakeFolder(root.string(),game));
    assert(Read(dest/"tools/w64devkit-x64.7z.exe")=="compiler archive");
    assert(Read(dest/"tools/3dsrecomp")=="translator");
    assert(Read(destExe)=="existing executable");
    assert(Read(dest/"settings.ini")=="player settings");
    assert(Read(dest/"title.so")=="prepared game");
    assert(Read(dest/"data/user/main")=="player save");
    assert(Read(fs::u8path(game.rom))=="synthetic header only");
#ifndef _WIN32
    assert((fs::status(dest/"tools/3dsrecomp").permissions()&fs::perms::owner_exec)!=fs::perms::none);
#endif
    assert(fs::remove(dest/"tools/3dsrecomp"));
    assert(N3ds::MakeFolder(root.string(),game));
    assert(Read(dest/"tools/3dsrecomp")=="translator"); // a later retry also repairs a removed tool
    assert(N3ds::MakeFolder((root/"no-master-kit").string(),game)); // prepared installs still work offline

    Launcher::Game broken=game;
    broken.folder=(root/"3ds/broken").string();
    Put(fs::u8path(N3ds::Exe(broken.folder)),"existing executable");
    Put(fs::u8path(broken.folder)/"tools","blocking file");
    assert(!N3ds::MakeFolder(root.string(),broken));
    assert(Read(fs::u8path(broken.folder)/"tools")=="blocking file");
    assert(fs::remove(fs::u8path(broken.folder)/"tools"));
    assert(N3ds::MakeFolder(root.string(),broken));
    assert(Read(fs::u8path(broken.folder)/"tools/w64devkit-x64.7z.exe")=="compiler archive");
#ifdef _WIN32
    // The #208 log's install directory is 219 characters. Its source compiler path
    // fits MAX_PATH; the per-title copy grows to 267 characters and must still copy.
    const size_t padding=219-root.string().size()-1;
    assert(padding>0 && padding<240);
    const fs::path longApp=root/std::string(padding,'x');
    const fs::path longKit=fs::u8path(N3ds::ProgramDir(longApp.string()));
    Put(fs::u8path(N3ds::Exe(longKit.string())),"master executable");
    Put(longKit/"tools/w64devkit-x64.7z.exe","compiler archive");
    assert(Read(longKit/"tools/w64devkit-x64.7z.exe")=="compiler archive");
    Launcher::Game longGame=game;
    longGame.folder=(longApp/"3ds/000400000011C400").string();
    Put(fs::u8path(N3ds::Exe(longGame.folder)),"existing executable");
    assert(N3ds::MakeFolder(longApp.string(),longGame));
    std::error_code ec;
    const auto copied=RuntimeKit::NativePath(fs::u8path(longGame.folder)/"tools/w64devkit-x64.7z.exe",ec);
    assert(!ec && Read(copied)=="compiler archive");
#endif
}
