// Run on Linux with -static-libstdc++ -Wl,--wrap=pthread_create to exercise resource exhaustion.
#include "romhacks.h"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <filesystem>
#include <pthread.h>

static std::atomic<bool> failThread{true};
extern "C" int __real_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
extern "C" int __wrap_pthread_create(pthread_t* t, const pthread_attr_t* a, void* (*f)(void*), void* p)
{
    if (failThread.exchange(false)) return EAGAIN;
    return __real_pthread_create(t, a, f, p);
}

int main()
{
    const auto dir = std::filesystem::temp_directory_path() / "fulldex-hash-worker-test";
    std::filesystem::create_directories(dir);
    const auto path = (dir / "synthetic.bin").string();
    FILE* f = fopen(path.c_str(), "wb");
    assert(f && fwrite("123456789", 1, 9, f) == 9);
    assert(fclose(f) == 0);
    auto& h = RomHacks::Hasher::Get();
    uint32_t crc = 0;
    uint64_t size = 0;
    // Failure must return, without aborting or caching a failed result.
    assert(!h.Result(path, "retry", crc, size, true));
    assert(!h.Busy());
    assert(h.Result(path, "retry", crc, size, true));
    assert(size == 9 && crc == 0xCBF43926u);
    // Concurrent waiters share the same result and all wake up.
    std::vector<std::thread> waiters;
    for (int i = 0; i < 8; i++) waiters.emplace_back([&] {
        uint32_t c; uint64_t n;
        assert(h.Result(path, "shared", c, n, true));
        assert(c == 0xCBF43926u && n == 9);
    });
    for (auto& t : waiters) t.join();
    assert(!h.Result(path + ".missing", "missing", crc, size, true));
    std::filesystem::remove(path);
    std::filesystem::remove(dir);
}
