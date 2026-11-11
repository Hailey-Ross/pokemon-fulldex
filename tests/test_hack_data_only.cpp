#include "../frontend/romhacks.h"
#include <cassert>

// Copies of one clean release that differ only outside the game itself (the secure area, the
// padding) are that game: they keep its save. A changed program or file table is a hack.
int main(int argc, char** argv)
{
    assert(argc == 5);
    uint32_t a = 0, b = 0;
    uint8_t rev = 0;
    uint64_t size = 0;
    assert(RomHacks::DsFingerprint(argv[1], a, rev, size));
    assert(RomHacks::DsFingerprint(argv[2], b, rev, size));
    assert(a == b && a == 0x9E24E992u);
    RomHacks::Cache cache;
    // (Verdicts kept by the earlier whole-ROM check are not used.)
    cache.Put(RomHacks::Cache::Key(argv[2], "ds-complete-v1"), "modified");
    cache.Put(RomHacks::Cache::Key(argv[3], "ds-complete-v1"), "modified");
    assert(RomHacks::CheckDs(argv[1], "CPUE", cache, true) == RomHacks::Kind::Clean);
    assert(RomHacks::CheckDs(argv[2], "CPUE", cache, true) == RomHacks::Kind::Clean); // another dump's padding
    assert(RomHacks::CheckDs(argv[3], "CPUE", cache, true) == RomHacks::Kind::Clean); // secure area left encrypted
    assert(RomHacks::CheckDs(argv[4], "CPUE", cache, true) == RomHacks::Kind::Modified); // its program changed
}
