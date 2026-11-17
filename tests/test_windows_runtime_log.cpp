#include "n3ds.h"
#include <cassert>
#include <fstream>
#include <iterator>

static std::string Read(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    if (argc == 1)
    {
        // A fake windowed game. More than a pipe buffer, including no final newline.
        const bool restarted = fs::exists("restarted.txt");
        fs::create_directories("data");
        std::ofstream("data/log.txt") << "renderer session " << (restarted ? "second" : "first");
        std::fprintf(stderr, "runtime stderr %s\n", restarted ? "second" : "first");
        std::fwrite("runtime stdout\n", 1, 15, stdout);
        const std::string bulk(128 * 1024, 'x');
        assert(std::fwrite(bulk.data(), 1, bulk.size(), stdout) == bulk.size());
        std::fprintf(stderr, "runtime tail without newline");
        if (!restarted) { std::ofstream("restarted.txt") << "yes"; return 3; }
        return 0;
    }
    assert(argc == 2);
    const fs::path root = fs::absolute(fs::u8path(argv[1]));
    fs::create_directories(root);
    Launcher::Game game;
    game.folder = (root / fs::u8path("title with spaces \xc3\xa9")).u8string();
    game.name = "Synthetic runtime";
    game.rom = (root / "synthetic.3ds").u8string();
    fs::create_directories(fs::u8path(game.folder));
    std::ofstream(fs::u8path(game.rom)) << "synthetic fixture, no game data";
    wchar_t self[32768];
    assert(GetModuleFileNameW(nullptr, self, 32768));
    fs::copy_file(self, fs::u8path(N3ds::Exe(game.folder)));
    const fs::path log = root / "runtime.log";
    assert(_wfreopen(log.c_str(), L"wb", stderr));
    assert(N3ds::Play(root.u8string(), game) == 0);
    std::fflush(stderr);
    auto text = Read(log);
    assert(text.find("runtime stderr first") != std::string::npos);
    assert(text.find("runtime stderr second") != std::string::npos);
    assert(text.find("runtime stdout") != std::string::npos);
    assert(text.find("renderer session first") != std::string::npos);
    assert(text.find("renderer session second") != std::string::npos);
    assert(text.find(std::string(128 * 1024, 'x')) != std::string::npos);
    assert(text.find("runtime tail without newline") != std::string::npos);
    assert(text.find("ended (3)") != std::string::npos);
    assert(text.find("ended (0)") != std::string::npos);
    assert(fs::exists(fs::u8path(game.folder) / "restarted.txt"));

    DWORD code = 999;
    assert(!WindowsRuntimeLog::Run((root / L"missing.exe").wstring(), root.wstring(), code));
    std::fflush(stderr);
    text = Read(log);
    assert(text.find("runtime start failed (Windows error 2)") != std::string::npos);
    assert(code == 999);
    std::puts("Windows runtime output, full pipes, restart, Unicode paths and launch error checks passed.");
}
