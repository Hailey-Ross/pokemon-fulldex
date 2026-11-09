#include "../gba/frontend/games.h"
#include <cassert>
#include <filesystem>

int main(int argc, char** argv)
{
    assert(argc == 2);
    namespace Games = gbaw::Games;
    Games::Game game;
    game.rom = argv[1];
    game.patch = "synthetic";
    game.forced = true;
    std::vector<uint8_t> original(512, 0xFF), output, before;
    original[100] = 0x42; // The hack uses the space the widescreen patch wants to overwrite.
    assert(Games::WriteFile(game.rom, original));
    std::string error;
    assert(Games::Prepare(game, output, error, &before));
    assert(output == original && before == original);
    assert(Games::Prepare(game, output, error, nullptr, true));
    assert(output == original); // Explicit retries must not bypass the safety check either.
    // A compatible ROM still receives its requested widescreen patch.
    original[100] = 0xFF;
    assert(Games::WriteFile(game.rom, original));
    game.forced = false;
    assert(Games::Prepare(game, output, error));
    assert(output[100] == 0x99);
    original[100] = 0x99;
    assert(output == original);
}
