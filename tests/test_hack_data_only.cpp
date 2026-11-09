#include "../frontend/romhacks.h"
#include <cassert>

int main(int argc, char** argv)
{
    assert(argc == 3);
    uint32_t before = 0, after = 0;
    uint8_t rev = 0;
    uint64_t size = 0;
    assert(RomHacks::DsFingerprint(argv[1], before, rev, size));
    assert(RomHacks::DsFingerprint(argv[2], after, rev, size));
    assert(before == after && before == 0x9E24E992u);
    RomHacks::Cache cache;
    assert(RomHacks::CheckDs(argv[1], "CPUE", cache, true) == RomHacks::Kind::Clean);
    cache.Put(RomHacks::Cache::Key(argv[2], "ds"), "clean"); // Old partial-check cache.
    assert(RomHacks::CheckDs(argv[2], "CPUE", cache, true) == RomHacks::Kind::Modified);
}
