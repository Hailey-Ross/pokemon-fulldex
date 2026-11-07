// TRADE EVOLUTIONS WITHOUT TRADING on real ROMs: test_trade_evolutions <rom>...
// Prints what changed, and checks nothing else in the ROM did.
#include "trade_evolutions.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
int main(int argc, char** argv)
{
    int failures = 0;
    for (int a = 1; a < argc; a++)
    {
        std::ifstream f(argv[a], std::ios::binary);
        std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), {}), before = rom;
        std::string log;
        const std::string name = argv[a];
        const bool gba = name.size() > 4 && name.substr(name.size() - 4) == ".gba";
        const int n = gba ? TradeEvolutions::PatchGba(rom.data(), rom.size(), log) : TradeEvolutions::PatchNds(rom, log);
        size_t diff = 0;
        for (size_t i = 0; i < rom.size(); i++) diff += rom[i] != before[i];
        printf("== %s: %d evolutions changed, %zu bytes differ\n%s", name.substr(name.rfind('/') + 1).c_str(), n, diff, log.c_str());
        if (n == 0 || diff > (size_t)n * 12) { printf("FAIL\n"); failures++; }
        std::string again;
        if ((gba ? TradeEvolutions::PatchGba(rom.data(), rom.size(), again) : TradeEvolutions::PatchNds(rom, again)) != 0) { printf("FAIL: patching twice changed more\n"); failures++; }
    }
    printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures;
}
