// FAST FORWARD MUSIC for the GBA games, the PokeDaisy way (adapted from PokeDaisy by Lidor Itzhari -
// https://github.com/lidor30/pokedaisy, GPL 3: FfMusicRenderer.kt, M4aSongs.kt, FfMusicKey.kt, M4aLoop.kt, pk_call.c).
// While the game fast forwards, the song it is playing is heard at its own speed: a clip of it, recorded beforehand on
// a second copy of the game that nobody sees or plays (no save, no input), played in a seamless loop.
//   Songs:    where the ROM's m4a sound engine keeps its songs (m4aSongNumStart and gSongTable), found by its code.
//   Key:      the song the background-music player is really playing (its song header, a ROM pointer).
//   Renderer: the second copy, on a thread of its own. It boots the game, parks its main loop on a `b .` (the VBlank
//             interrupt keeps the sound engine running), then for each song asked for calls m4aSongNumStart itself and
//             records it, cut where the sound engine loops (LoopWatch: the first track's GOTO back) into an intro and
//             a loop. Clips are kept on disk per ROM, so each song is recorded once.
// Nothing of the player's running game or save is touched.
#pragma once
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>
#include <mgba-util/vfs.h>
#include "pokedaisy/pk_call_lite.h"

namespace GbaFfMusic
{
// ---- Where the songs are -----------------------------------------------------------------------------------------
struct Songs
{
    uint32_t numStart = 0;   // m4aSongNumStart (Thumb, +1)
    uint32_t idle = 0;       // a `b .` in the ROM
    uint32_t headerXor = 0;  // (Monomyth stores the headers top bit flipped)
    std::map<uint32_t, int> ids; // song header (ROM offset) -> song number
    bool ok = false;
};

inline Songs Locate(const std::vector<uint8_t>& rom)
{
    struct Sig { const char* pattern; int tableLdr; uint32_t headerXor; };
    // (m4aSongNumStart: agbcc's, gcc's (Emerald Rogue), newer expansion's with its ident check, Monomyth's. Heart and
    // Soul's alternate table is left out.)
    static const Sig sigs[] = {
        { "00b50004074a0849400b401883885900c918890089180a680168101c00f0a2fb", 6, 0 },
        { "00b5..4b0004400bc018828853009b18..4a9b000168d058........01bc0047", 2, 0 },
        { "00b5..4a03045b0b9b1899884a005218..4992008858..4a416b914202d11968........01bc0047", 2, 0 },
        { "00b50004..4a..49400b401883885900c918890089180a680168802000064140101c", 6, 0x80000000u },
    };
    Songs s;
    auto u16 = [&](size_t o) { return (uint32_t)(rom[o] | rom[o + 1] << 8); };
    auto u32 = [&](size_t o) { return rom[o] | rom[o + 1] << 8 | rom[o + 2] << 16 | (uint32_t)rom[o + 3] << 24; };
    for (const Sig& sig : sigs)
    {
        const size_t n = strlen(sig.pattern) / 2;
        std::vector<int> val(n), mask(n);
        for (size_t k = 0; k < n; k++)
        {
            auto nib = [](char c) { return c == '.' ? 0 : c <= '9' ? c - '0' : c - 'a' + 10; };
            const char a = sig.pattern[k * 2], b = sig.pattern[k * 2 + 1];
            val[k] = nib(a) << 4 | nib(b);
            mask[k] = (a == '.' ? 0 : 0xF0) | (b == '.' ? 0 : 0x0F);
        }
        long at = -1;
        for (size_t i = 0; i + n <= rom.size() && at < 0; i += 2)
        {
            size_t j = 0;
            while (j < n && (rom[i + j] & mask[j]) == val[j]) j++;
            if (j == n) at = (long)i;
        }
        if (at < 0) continue;
        const size_t ins = at + sig.tableLdr;
        if (u16(ins) >> 11 != 0x09) return s;
        const size_t lit = ((ins + 4) & ~(size_t)3) + (u16(ins) & 0xFF) * 4;
        if (lit + 4 > rom.size()) return s;
        const uint32_t table = u32(lit);
        if (table < 0x08000000 || table - 0x08000000 >= rom.size()) return s;
        const size_t t = table - 0x08000000;
        for (int k = 0; k < 4096 && t + k * 8 + 8 <= rom.size(); k++)
        {
            const uint32_t h = u32(t + k * 8) ^ sig.headerXor;
            if (h < 0x08000000 || h - 0x08000000 >= rom.size()) break;
            s.ids.emplace(h - 0x08000000, k);
        }
        s.numStart = 0x08000000 + (uint32_t)at + 1;
        s.headerXor = sig.headerXor;
        for (size_t i = 0; i + 2 <= rom.size(); i += 2)
            if (rom[i] == 0xFE && rom[i + 1] == 0xE7) { s.idle = 0x08000000 + (uint32_t)i; break; }
        s.ok = !s.ids.empty() && s.idle;
        return s;
    }
    return s;
}

// ---- What is playing ---------------------------------------------------------------------------------------------
inline bool InRam(uint32_t a) { return (a >= 0x02000000 && a < 0x02040000) || (a >= 0x03000000 && a < 0x03008000); }
// The BGM player's MusicPlayerInfo (the tail of SoundInfo's chain of players), or 0.
inline uint32_t BgmPlayer(struct mCore* c)
{
    const uint32_t info = c->busRead32(c, 0x03007FF0);
    if (!InRam(info)) return 0;
    uint32_t p = c->busRead32(c, info + 0x24), bgm = 0;
    for (int n = 0; p; n++)
    {
        if (!InRam(p) || n >= 12) return 0;
        bgm = p;
        p = c->busRead32(c, p + 0x3C);
    }
    return bgm;
}
// The song playing as a key (its header's ROM offset + 1), or 0 for none.
inline uint32_t CurrentKey(struct mCore* c)
{
    const uint32_t bgm = BgmPlayer(c);
    if (!bgm || !(c->busRead32(c, bgm + 4) & 0xFFFF)) return 0;
    const uint32_t h = c->busRead32(c, bgm);
    return h >= 0x08000000 && h < 0x0A000000 ? h - 0x08000000 + 1 : 0;
}

// ---- Where a song loops, from the sound engine itself (M4aLoop.kt) --------------------------------------------------
class LoopWatch
{
public:
    explicit LoopWatch(struct mCore* c) : c(c) {}
    int startFrame = -1, endFrame = -1;
    bool Found() const { return endFrame >= 0; }
    void OnFrame()
    {
        const int f = (int)history.size();
        const long main = MainPos();
        history.push_back(main);
        if (Found() || f == 0) return;
        const long prev = history[f - 1];
        if (main < 0 || prev < 0 || main >= prev) return;
        const long target = GotoTarget((uint32_t)prev, (uint32_t)main);
        if (target < 0) return;
        int crossed = -1;
        for (int i = 0; i < f; i++)
            if (history[i] > target) { crossed = i; break; }
        if (crossed > 0 && history[crossed - 1] >= 0 && history[crossed - 1] <= target) { startFrame = crossed; endFrame = f; }
        else if (firstGoto >= 0) { startFrame = firstGoto; endFrame = f; }
        else firstGoto = f;
    }

private:
    struct mCore* c;
    std::vector<long> history;
    int track = -1, firstGoto = -1;
    long MainPos()
    {
        const uint32_t bgm = BgmPlayer(c);
        if (!bgm) return -1;
        const uint32_t tracks = c->busRead32(c, bgm + 0x2C);
        if (!InRam(tracks)) return -1;
        if (track < 0)
        {
            const int count = std::min<int>(c->busRead8(c, bgm + 0x08), 16);
            for (int i = 0; i < count && track < 0; i++)
                if (c->busRead8(c, tracks + i * 0x50) & 0x80) track = i;
            if (track < 0) return -1;
        }
        const uint32_t t = tracks + track * 0x50;
        const uint32_t pos = c->busRead8(c, t + 2) == 0 ? c->busRead32(c, t + 0x40) : c->busRead32(c, t + 0x44);
        return pos >= 0x08000000 && pos < 0x0A000000 ? (long)pos : -1;
    }
    long GotoTarget(uint32_t prev, uint32_t now)
    {
        for (int i = 0; i + 5 <= 128; i++)
        {
            if (c->busRead8(c, prev + i) != 0xB2) continue;
            uint32_t ptr = 0; // (byte by byte: the pointer isn't aligned, and the bus reads a word at an aligned address)
            for (int k = 0; k < 4; k++) ptr |= (uint32_t)c->busRead8(c, prev + i + 1 + k) << (8 * k);
            if (ptr >= 0x08000000 && ptr < 0x0A000000 && ptr <= now && ptr < prev + i) return ptr;
        }
        return -1;
    }
};

// A seamless loop out of a recording (M4aLoopSplice): starts a second into the loop, its length tuned to the sample.
inline bool Splice(const std::vector<int16_t>& pcm, size_t len, size_t loopStart, size_t loopEnd, int rate, size_t& from, size_t& count)
{
    const long total = (long)len / 2, start = (long)loopStart / 2, p0 = ((long)loopEnd - (long)loopStart) / 2;
    const long w = rate / 10, r = rate / 60 * 3;
    if (p0 <= r + w) return false;
    long s = start + rate;
    if (s + p0 + r + w > total) s = total - p0 - r - w;
    if (s < start) return false;
    long best = p0;
    long long bestErr = -1;
    for (long p = p0 - r; p <= p0 + r; p++)
    {
        long long err = 0;
        for (long i = 0; i < w * 2 && (bestErr < 0 || err < bestErr); i++)
        {
            const long long d = (long long)pcm[s * 2 + i] - pcm[(s + p) * 2 + i];
            err += d * d;
        }
        if (bestErr < 0 || err < bestErr) { bestErr = err; best = p; }
    }
    from = (size_t)s * 2;
    count = (size_t)best * 2;
    return true;
}

// ---- Calling the game's own function on the parked copy (pk_call_lite.c) -------------------------------------------
inline bool Call(struct mCore* core, uint32_t fn, const uint32_t args[4]) { return pk_lite_call(core, fn, args) != 0; }

// ---- A song's clip ----------------------------------------------------------------------------------------------
struct Clip
{
    std::vector<int16_t> intro, loop; // stereo, interleaved, at the renderer's rate
};

// ---- The second copy, recording songs --------------------------------------------------------------------------------
class Renderer
{
public:
    ~Renderer() { Stop(); }
    // rom: the game as it runs; rate: the device's; dir: where its clips are kept (one folder per ROM).
    void Start(const std::vector<uint8_t>& rom, int rate, const std::string& dir)
    {
        Stop();
        songs = Locate(rom);
        if (!songs.ok) return;
        romCopy = rom;
        this->rate = rate;
        this->dir = dir;
        stop = false;
        Log("songs found: %u, m4aSongNumStart %08x", (unsigned)songs.ids.size(), songs.numStart);
        worker = std::thread([this] { Run(); });
    }
    void Stop()
    {
        {
            std::lock_guard<std::mutex> l(m);
            stop = true;
        }
        cv.notify_all();
        if (worker.joinable()) worker.join();
        std::lock_guard<std::mutex> l(m);
        queue.clear();
        clips.clear();
        failed.clear();
        asked.clear();
    }
    bool Supported() const { return songs.ok; }
    // Record this song soon (the one the game plays now goes first).
    void Request(uint32_t key)
    {
        if (!songs.ok || !key) return;
        {
            std::lock_guard<std::mutex> l(m);
            if (!asked.count(key) && !clips.count(key) && !failed.count(key)) Log("asked for song %07x", key - 1);
        }
        std::lock_guard<std::mutex> l(m);
        if (clips.count(key) || failed.count(key) || asked.count(key)) return;
        asked.insert(key);
        queue.push_front(key);
        cv.notify_all();
    }
    std::shared_ptr<const Clip> Get(uint32_t key)
    {
        std::lock_guard<std::mutex> l(m);
        auto it = clips.find(key);
        return it == clips.end() ? nullptr : it->second;
    }
    // Whether this song's clip is or will be there (fast forward then waits silent rather than rushing the music).
    bool WillHave(uint32_t key)
    {
        std::lock_guard<std::mutex> l(m);
        return songs.ok && !failed.count(key) && (clips.count(key) || asked.count(key)) && songs.ids.count(key - 1);
    }

private:
    Songs songs;
    std::vector<uint8_t> romCopy;
    int rate = 48000;
    std::string dir;
    std::thread worker;
    std::mutex m;
    std::condition_variable cv;
    bool stop = false;
    std::deque<uint32_t> queue;
    std::map<uint32_t, std::shared_ptr<const Clip>> clips;
    std::set<uint32_t> failed, asked;

    // A line in the folder's log.txt (what was recorded, what wasn't and why).
    void Log(const char* fmt, unsigned a = 0, unsigned b = 0, unsigned c = 0)
    {
        if (dir.empty()) return;
        if (FILE* f = fopen((dir + "/log.txt").c_str(), "a"))
        {
            fprintf(f, fmt, a, b, c);
            fputc('\n', f);
            fclose(f);
        }
    }
    std::string FileOf(uint32_t key) const
    {
        char b[32];
        snprintf(b, sizeof b, "/%07x.ffm", (unsigned)(key - 1));
        return dir + b;
    }
    bool LoadFile(uint32_t key)
    {
        FILE* f = fopen(FileOf(key).c_str(), "rb");
        if (!f) return false;
        uint32_t h[4] = {};
        bool ok = fread(h, 4, 4, f) == 4 && h[0] == 0x314D4646 && (int)h[1] == rate && h[2] < (1u << 26) && h[3] < (1u << 26) && h[3] > 0;
        auto c = std::make_shared<Clip>();
        if (ok)
        {
            c->intro.resize(h[2]);
            c->loop.resize(h[3]);
            ok = fread(c->intro.data(), 2, h[2], f) == h[2] && fread(c->loop.data(), 2, h[3], f) == h[3];
        }
        fclose(f);
        if (!ok) return false;
        std::error_code ec;
        std::filesystem::last_write_time(FileOf(key), std::filesystem::file_time_type::clock::now(), ec); // (played now)
        std::lock_guard<std::mutex> l(m);
        clips[key] = c;
        return true;
    }
    // The clips of every game together are kept under about 400 MB: the ones played longest ago go first.
    void Trim()
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path root = fs::path(dir).parent_path();
        std::vector<std::pair<fs::file_time_type, fs::path>> files;
        uintmax_t total = 0;
        for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
            if (it->is_regular_file(ec) && it->path().extension() == ".ffm")
            {
                total += it->file_size(ec);
                files.push_back({ it->last_write_time(ec), it->path() });
            }
        std::sort(files.begin(), files.end());
        for (size_t i = 0; i < files.size() && total > (400ull << 20); i++)
        {
            const uintmax_t size = fs::file_size(files[i].second, ec);
            if (fs::remove(files[i].second, ec)) total -= size;
        }
    }
    void SaveFile(uint32_t key, const Clip& c)
    {
        if (dir.empty()) return;
        Trim();
        if (FILE* f = fopen(FileOf(key).c_str(), "wb"))
        {
            const uint32_t h[4] = { 0x314D4646, (uint32_t)rate, (uint32_t)c.intro.size(), (uint32_t)c.loop.size() };
            fwrite(h, 4, 4, f);
            fwrite(c.intro.data(), 2, c.intro.size(), f);
            fwrite(c.loop.data(), 2, c.loop.size(), f);
            fclose(f);
        }
    }

    // The copy of the game, booted and parked; nullptr if it would not.
    struct Core
    {
        struct mCore* core = nullptr;
        std::vector<uint32_t> video;
        struct mAudioResampler resampler{};
        struct mAudioBuffer out{};
        bool parked = false;
        ~Core()
        {
            if (core)
            {
                mAudioResamplerDeinit(&resampler);
                mAudioBufferDeinit(&out);
                core->deinit(core);
            }
        }
    };
    std::unique_ptr<Core> Boot()
    {
        auto c = std::make_unique<Core>();
        c->core = mCoreCreate(mPLATFORM_GBA);
        if (!c->core || !c->core->init(c->core)) { c->core = nullptr; return nullptr; }
        mCoreInitConfig(c->core, nullptr);
        struct mCoreOptions opts{};
        opts.useBios = false;
        opts.skipBios = true;
        opts.volume = 0x100;
        opts.sampleRate = 48000;
        opts.audioBuffers = 4096;
        mCoreConfigLoadDefaults(&c->core->config, &opts);
        mCoreLoadConfig(c->core);
        c->video.assign((size_t)1024 * 512, 0); // (never looked at; room for any width the core draws)
        c->core->setVideoBuffer(c->core, c->video.data(), 1024);
        c->core->setAudioBufferSize(c->core, 4096);
        struct VFile* vf = VFileMemChunk(romCopy.data(), romCopy.size());
        if (!vf || !c->core->loadROM(c->core, vf)) { c->core->deinit(c->core); c->core = nullptr; return nullptr; }
        c->core->reset(c->core); // (no save: never the player's)
        mAudioBufferInit(&c->out, 16384, 2);
        mAudioResamplerInit(&c->resampler, mINTERPOLATOR_SINC);
        // Boot past the logos until the title music plays (the engine is set up then), then park the main loop.
        for (int i = 0; i < 1800 && !Stopping(); i++)
        {
            Frame(*c, nullptr);
            if (i >= 300 && CurrentKey(c->core)) break;
        }
        c->parked = pk_lite_park(c->core, songs.idle) != 0;
        return c->parked ? std::move(c) : nullptr;
    }
    bool Stopping()
    {
        std::lock_guard<std::mutex> l(m);
        return stop;
    }
    // One frame of the copy; its sound, at `rate`, added to buf (or dropped).
    void Frame(Core& c, std::vector<int16_t>* buf)
    {
        c.core->runFrame(c.core);
        struct mAudioBuffer* src = c.core->getAudioBuffer(c.core);
        mAudioResamplerSetSource(&c.resampler, src, c.core->audioSampleRate(c.core), true);
        mAudioResamplerSetDestination(&c.resampler, &c.out, rate);
        mAudioResamplerProcess(&c.resampler);
        int16_t tmp[4096];
        size_t n;
        while ((n = mAudioBufferRead(&c.out, tmp, 2048)) > 0)
            if (buf) buf->insert(buf->end(), tmp, tmp + n * 2);
    }
    bool Force(Core& c, int number)
    {
        const uint32_t args[4] = { (uint32_t)number, 0, 0, 0 };
        for (int i = 0; i < 60; i++)
        {
            if (Call(c.core, songs.numStart, args)) return true;
            Frame(c, nullptr);
        }
        return false;
    }
    // Records one song: 1 recorded, 0 failed (interrupted, never started, or ended by itself).
    int Record(Core& c, uint32_t key, std::vector<int16_t>& buf)
    {
        auto it = songs.ids.find(key - 1);
        if (it == songs.ids.end()) return 0;
        const size_t cap = (size_t)rate * 2 * 100; // (100 seconds at most)
        for (int attempt = 0; attempt < 2; attempt++)
        {
            if (!Force(c, it->second))
            {
                int thumb = 0, mode = 0, halted = 0;
                const uint32_t pc = pk_lite_where(c.core, &thumb, &mode, &halted);
                Log("song %u: m4aSongNumStart couldn't be called (pc %08x, thumb and mode %x)", it->second, pc, (unsigned)(thumb << 8 | mode | halted << 12));
                return 0;
            }
            uint32_t now = 0;
            for (int i = 0; i < 30 && !now; i++) { Frame(c, nullptr); now = CurrentKey(c.core); }
            if (now != key) { Log("song %u: playing %07x instead", it->second, now ? now - 1 : 0); return 0; }
            buf.clear();
            LoopWatch loop(c.core);
            std::vector<size_t> starts;
            size_t stopAt = cap;
            bool spoiled = false;
            while (buf.size() < stopAt && starts.size() < 100 * 60 + 60 && !Stopping())
            {
                starts.push_back(buf.size());
                Frame(c, &buf);
                if (const uint32_t k2 = CurrentKey(c.core); k2 != key)
                {
                    Log("song %07x: after %u frames the copy plays %07x", key - 1, (unsigned)starts.size(), k2 ? k2 - 1 : 0);
                    spoiled = true;
                    break;
                }
                loop.OnFrame();
                if (loop.Found() && stopAt == cap) stopAt = std::min(cap, buf.size() + (size_t)rate * 2 * 3);
            }
            if (Stopping()) return 0;
            if (spoiled && !CurrentKey(c.core)) return 0; // (it ended by itself: a fanfare, nothing to loop)
            if (spoiled) continue;
            Log("recorded %u frames of sound, loop %u", (unsigned)(buf.size() / 2), loop.Found() ? 1u : 0u);
            auto clip = std::make_shared<Clip>();
            size_t from = 0, count = 0;
            if (loop.Found() && Splice(buf, buf.size(), starts[loop.startFrame], starts[loop.endFrame], rate, from, count))
            {
                clip->intro.assign(buf.begin(), buf.begin() + from);
                clip->loop.assign(buf.begin() + from, buf.begin() + from + count);
            }
            else if (buf.size() >= cap)
                clip->loop.assign(buf.begin(), buf.end());
            else continue;
            SaveFile(key, *clip);
            std::lock_guard<std::mutex> l(m);
            clips[key] = clip;
            return 1;
        }
        return 0;
    }
    void Run()
    {
        std::unique_ptr<Core> core;
        std::vector<int16_t> buf;
        for (;;)
        {
            uint32_t key = 0;
            {
                std::unique_lock<std::mutex> l(m);
                cv.wait(l, [&] { return stop || !queue.empty(); });
                if (stop) break;
                key = queue.front();
                queue.pop_front();
            }
            if (LoadFile(key)) continue;
            if (!core)
            {
                core = Boot();
                Log(core ? "copy booted and parked on %08x" : "the copy didn't boot or park", songs.idle);
            }
            const int ok = core ? Record(*core, key, buf) : 0;
            Log(ok ? "song %07x recorded" : "song %07x not recorded", key - 1);
            if (!ok)
            {
                std::lock_guard<std::mutex> l(m);
                failed.insert(key);
            }
            buf.shrink_to_fit();
        }
    }
};

// ---- Playing a clip during fast forward ---------------------------------------------------------------------------
// Where in the song it is, a stereo frame at a time: the intro once, then the loop over and over.
struct Player
{
    std::shared_ptr<const Clip> clip;
    uint32_t key = 0;
    size_t at = 0; // stereo frames since the song's top
    int fadeIn = 0;
    void Begin(std::shared_ptr<const Clip> c, uint32_t k, size_t from)
    {
        clip = std::move(c);
        key = k;
        at = from;
        fadeIn = 240;
    }
    // n stereo frames into out, at gain (0..256).
    void Fill(int16_t* out, int n, int gain)
    {
        const size_t intro = clip->intro.size() / 2, loop = clip->loop.size() / 2;
        for (int i = 0; i < n; i++, at++)
        {
            const int16_t* s = at < intro ? &clip->intro[at * 2] : loop ? &clip->loop[((at - intro) % loop) * 2] : nullptr;
            int g = gain;
            if (fadeIn > 0) g = g * (240 - fadeIn--) / 240;
            out[i * 2] = s ? (int16_t)(s[0] * g / 256) : 0;
            out[i * 2 + 1] = s ? (int16_t)(s[1] * g / 256) : 0;
        }
    }
};
} // namespace GbaFfMusic
