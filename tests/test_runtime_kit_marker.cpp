#include "runtime_kit.h"
#include <cassert>
#include <cstdio>
#include <fstream>

// The 3DS kit's marker: a game folder whose copy finished is not walked again at every start,
// and one whose copy was cut short, or whose kit changed (an update), is.
int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    const fs::path root = argv[1], kit = root / "kit", game = root / "game";
    fs::create_directories(kit / "bin");
    { std::ofstream(kit / "game.exe") << "version one"; std::ofstream(kit / "bin/cc") << "compiler"; }
    const std::string stamp = RuntimeKit::Stamp(kit / "game.exe");
    assert(!stamp.empty());
    assert(!RuntimeKit::Marked(game, stamp));              // never copied
    std::error_code ec;
    assert(RuntimeKit::CopyMissing(kit, game, ec));
    assert(!RuntimeKit::Marked(game, stamp));              // copied, not yet marked (a copy cut short looks the same)
    RuntimeKit::Mark(game, stamp);
    assert(RuntimeKit::Marked(game, stamp));               // complete: nothing walked next time
    { std::ofstream(kit / "game.exe") << "version two, longer"; }
    const std::string updated = RuntimeKit::Stamp(kit / "game.exe");
    assert(updated != stamp && !RuntimeKit::Marked(game, updated)); // an updated kit is copied again
    assert(RuntimeKit::Stamp(root / "no kit here.exe").empty());
    std::puts("3DS kit marker: complete copies are skipped, cut-short copies and updated kits are not.");
}
