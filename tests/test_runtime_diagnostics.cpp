#include "runtime_diagnostics.h"
#include <cassert>
#include <iterator>
#if defined(__linux__) && !defined(__ANDROID__)
#include "n3ds.h"
#endif

static std::string Output(FILE* log)
{
    std::fflush(log);
    std::rewind(log);
    std::string text;
    char buffer[4096];
    for (size_t n; (n = std::fread(buffer, 1, sizeof buffer, log));) text.append(buffer, n);
    return text;
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    namespace fs = std::filesystem;
    const fs::path root = fs::u8path(argv[1]);
    fs::create_directories(root / "data");
    FILE* log = std::tmpfile();
    assert(log);
    const auto missing = RuntimeDiagnostics::Before(root.u8string());
    RuntimeDiagnostics::AppendChanged(root.u8string(), missing, log);
    assert(Output(log).empty());
    const fs::path source = root / "data/log.txt";
    std::ofstream(source) << "renderer: GPU example\n";
    RuntimeDiagnostics::AppendChanged(root.u8string(), missing, log);
    assert(Output(log).find("renderer: GPU example") != std::string::npos);
    const auto prior = RuntimeDiagnostics::Before(root.u8string());
    const auto oldOutput = Output(log);
    RuntimeDiagnostics::AppendChanged(root.u8string(), prior, log);
    assert(Output(log) == oldOutput); // Do not present a previous session as current evidence.
    std::fclose(log);
    log = std::tmpfile();
    assert(log);
    std::ofstream(source) << std::string(512 * 1024, 'x') << "newest driver failure\n";
    RuntimeDiagnostics::AppendChanged(root.u8string(), prior, log);
    const auto text = Output(log);
    assert(text.size() < 256 * 1024 + 200);
    assert(text.find("last 256 KiB") != std::string::npos);
    assert(text.find("newest driver failure") != std::string::npos);
    std::fclose(log);
    assert(fs::file_size(source) > 512 * 1024); // Collection never modifies runtime files.
#if defined(__linux__) && !defined(__ANDROID__)
    Launcher::Game game;
    game.folder = (fs::absolute(root) / "fake title").u8string();
    game.rom = (fs::absolute(root) / "synthetic.3ds").u8string();
    game.name = "Synthetic Linux runtime";
    fs::create_directories(fs::u8path(game.folder));
    std::ofstream(fs::u8path(game.rom)) << "synthetic fixture, no game data";
    const auto exe = fs::u8path(N3ds::Exe(game.folder));
    std::ofstream(exe) << "#!/bin/sh\nmkdir -p data\n"
        "if test -f restarted; then echo 'linux renderer second' > data/log.txt; exit 0; fi\n"
        "echo 'linux renderer first' > data/log.txt\ntouch restarted\nexit 3\n";
    fs::permissions(exe, fs::perms::owner_all);
    const auto integrated = root / "integrated.log";
    assert(std::freopen(integrated.c_str(), "wb", stderr));
    assert(N3ds::Play(root.u8string(), game) == 0);
    std::fflush(stderr);
    std::ifstream in(integrated);
    const std::string result {std::istreambuf_iterator<char>(in), {}};
    assert(result.find("linux renderer first") != std::string::npos);
    assert(result.find("linux renderer second") != std::string::npos);
    assert(result.find("ended (3)") != std::string::npos);
    assert(result.find("ended (0)") != std::string::npos);
#endif
}
