#include "romhacks.h"
#include <cassert>
#include <filesystem>
#include <new>

static size_t largest = 0;
void* operator new(size_t n)
{
    largest = std::max(largest, n);
    if (void* p = malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { free(p); }
void operator delete(void* p, size_t) noexcept { free(p); }

int main()
{
    const auto path = (std::filesystem::temp_directory_path() / "fulldex-fingerprint-memory.bin").string();
    uint8_t header[512] = {};
    const uint32_t fatSize = 40u << 20;
    auto put = [&](size_t at, uint32_t n) {
        for (int i = 0; i < 4; i++) header[at + i] = (uint8_t)(n >> (8 * i));
    };
    put(0x48, 512); put(0x4C, fatSize);
    put(0x50, 512 + fatSize); put(0x54, 32);
    FILE* f = fopen(path.c_str(), "wb");
    assert(f && fwrite(header, 1, sizeof header, f) == sizeof header);
    assert(fseek(f, 512 + fatSize + 31, SEEK_SET) == 0);
    assert(fputc(0, f) != EOF && fclose(f) == 0);
    largest = 0;
    uint32_t fp; uint8_t rev; uint64_t size;
    assert(RomHacks::DsFingerprint(path, fp, rev, size));
    const size_t peakAllocation = largest;
    // The FAT and overlay table are all zero; stream the same reference bytes.
    std::vector<uint8_t> zeros(1u << 20);
    uint32_t expected = 0;
    for (int i = 0; i < 40; i++) expected = RomHacks::Crc32(expected, zeros.data(), zeros.size());
    expected = RomHacks::Crc32(expected, zeros.data(), 32);
    assert(fp == expected && rev == 0 && size == 512 + fatSize + 32);
    fprintf(stderr, "fingerprint %08X, largest allocation %zu bytes\n", fp, peakAllocation);
    assert(peakAllocation <= (1u << 20));
    assert(remove(path.c_str()) == 0);
}
