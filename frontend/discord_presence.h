// Discord Rich Presence on the desktop (Windows, Linux, macOS): shows "In the launcher" while the
// game list is open and the game's name while it is played. It talks to the Discord app running on
// the same computer over its local pipe (discord-ipc-0 to 9), so nothing leaves the computer from
// here and nothing needs signing in. No Discord app running: nothing happens, and it tries again
// now and then. Not on Android or iOS (no Discord app pipe there): the calls do nothing.
//
// The Application ID comes from (first found): the FULLDEX_DISCORD_APP_ID environment variable, a
// discord-app-id.txt beside the program, or SSR_DISCORD_APP_ID at build time. None: off.
// Off for a player: FULLDEX_DISCORD=0, or a file named discord-presence-off.txt beside the program.
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>

#if defined(__ANDROID__)
#define FDX_DISCORD_NONE 1
#endif
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#undef FDX_DISCORD_NONE
#define FDX_DISCORD_NONE 1
#endif
#endif

#ifndef SSR_DISCORD_APP_ID
#define SSR_DISCORD_APP_ID ""
#endif

#ifndef FDX_DISCORD_NONE
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#endif
#endif

namespace DiscordPresence
{
namespace Detail
{
struct State
{
    std::mutex m;
    std::condition_variable cv;
    std::thread th;
    bool run = false, dirty = false;
    std::string appId, details, state;
    long long start = 0;
};
inline State& S() { static State s; return s; }

inline std::string Esc(const std::string& s)
{
    std::string o;
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o;
}

// A file beside the program (its own folder first, whatever the working folder is), else in the working folder.
inline std::string Beside(const char* name)
{
    std::string dir;
#if defined(_WIN32)
    char b[1024] = {};
    const DWORD n = GetModuleFileNameA(nullptr, b, sizeof b - 1);
    if (n > 0 && n < sizeof b - 1) dir.assign(b, n);
#elif defined(__linux__) && !defined(FDX_DISCORD_NONE)
    char b[1024] = {};
    const ssize_t n = readlink("/proc/self/exe", b, sizeof b - 1);
    if (n > 0) dir.assign(b, (size_t)n);
#endif
    const size_t k = dir.find_last_of("/\\");
    if (k != std::string::npos)
    {
        const std::string full = dir.substr(0, k + 1) + name;
        if (FILE* f = fopen(full.c_str(), "rb")) { fclose(f); return full; }
    }
    return name;
}
inline bool FileHere(const char* name) { FILE* f = fopen(Beside(name).c_str(), "rb"); if (f) fclose(f); return f != nullptr; }
inline std::string AppId()
{
    if (const char* e = getenv("FULLDEX_DISCORD_APP_ID")) if (*e) return e;
    if (FILE* f = fopen(Beside("discord-app-id.txt").c_str(), "rb"))
    {
        char b[64] = {};
        const size_t n = fread(b, 1, sizeof b - 1, f);
        fclose(f);
        std::string id(b, n);
        std::string out;
        for (char c : id) if (c >= '0' && c <= '9') out += c;
        if (out.size() >= 15) return out;
    }
    return SSR_DISCORD_APP_ID;
}
inline bool Allowed()
{
    if (const char* e = getenv("FULLDEX_DISCORD")) if (e[0] == '0') return false;
    return !FileHere("discord-presence-off.txt");
}

#ifndef FDX_DISCORD_NONE
// One connection to the Discord app: frames are a 32-bit opcode, a 32-bit length and JSON.
struct Conn
{
#ifdef _WIN32
    HANDLE h = INVALID_HANDLE_VALUE;
    bool Open()
    {
        for (int i = 0; i < 10; i++)
        {
            char path[64];
            snprintf(path, sizeof path, "\\\\.\\pipe\\discord-ipc-%d", i);
            h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) return true;
        }
        return false;
    }
    bool Write(const void* p, size_t n)
    {
        DWORD done = 0;
        return WriteFile(h, p, (DWORD)n, &done, nullptr) && done == n;
    }
    bool Read(void* p, size_t n, int ms)
    {
        auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        for (;;)
        {
            DWORD avail = 0;
            if (!PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr)) return false;
            if (avail >= n) break;
            if (std::chrono::steady_clock::now() > end) return false;
            Sleep(10);
        }
        DWORD done = 0;
        return ReadFile(h, p, (DWORD)n, &done, nullptr) && done == n;
    }
    void Close() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); h = INVALID_HANDLE_VALUE; }
#else
    int fd = -1;
    bool Open()
    {
        const char* dirs[] = { getenv("XDG_RUNTIME_DIR"), getenv("TMPDIR"), getenv("TMP"), getenv("TEMP"), "/tmp" };
        for (const char* d : dirs)
        {
            if (!d || !*d) continue;
            for (int i = 0; i < 10; i++)
            {
                const int s = socket(AF_UNIX, SOCK_STREAM, 0);
                if (s < 0) return false;
                sockaddr_un a = {};
                a.sun_family = AF_UNIX;
                snprintf(a.sun_path, sizeof a.sun_path, "%s/discord-ipc-%d", d, i);
                if (connect(s, (sockaddr*)&a, sizeof a) == 0)
                {
                    timeval tv = { 2, 0 };
                    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
                    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
                    fd = s;
                    return true;
                }
                close(s);
            }
        }
        return false;
    }
    bool Write(const void* p, size_t n)
    {
        const char* c = (const char*)p;
        while (n)
        {
#ifdef MSG_NOSIGNAL
            const ssize_t k = send(fd, c, n, MSG_NOSIGNAL);
#else
            const ssize_t k = send(fd, c, n, 0);
#endif
            if (k <= 0) return false;
            c += k; n -= (size_t)k;
        }
        return true;
    }
    bool Read(void* p, size_t n, int)
    {
        char* c = (char*)p;
        while (n)
        {
            const ssize_t k = recv(fd, c, n, 0);
            if (k <= 0) return false;
            c += k; n -= (size_t)k;
        }
        return true;
    }
    void Close() { if (fd >= 0) close(fd); fd = -1; }
#endif
    bool Send(uint32_t op, const std::string& json)
    {
        std::string f(8, '\0');
        const uint32_t len = (uint32_t)json.size();
        memcpy(&f[0], &op, 4);
        memcpy(&f[4], &len, 4);
        f += json;
        return Write(f.data(), f.size());
    }
    // A frame from Discord, thrown away (its answer is not needed), false if none came.
    bool Receive(uint32_t* op = nullptr)
    {
        uint32_t head[2];
        if (!Read(head, 8, 2000)) return false;
        if (op) *op = head[0];
        std::string body(head[1] > 65536 ? 0 : head[1], '\0');
        if (head[1] > 65536) return false;
        const bool got = body.empty() || Read(&body[0], body.size(), 2000);
        if (got && body.size() > 0) fprintf(stderr, "[discord] reply: %.200s\n", body.c_str());
        return got;
    }
};

inline std::string Activity(const State& s, int nonce)
{
    std::string j = "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"" + std::to_string(nonce) + "\",\"args\":{\"pid\":";
#ifdef _WIN32
    j += std::to_string((long)GetCurrentProcessId());
#else
    j += std::to_string((long)getpid());
#endif
    if (s.details.empty()) { j += "}}"; return j; } // (no activity: the status is cleared)
    j += ",\"activity\":{\"details\":\"" + Esc(s.details) + "\"";
    if (!s.state.empty()) j += ",\"state\":\"" + Esc(s.state) + "\"";
    if (s.start > 0) j += ",\"timestamps\":{\"start\":" + std::to_string(s.start) + "}";
    j += ",\"assets\":{\"large_image\":\"fulldex\",\"large_text\":\"Pokemon Fulldex\"}}}}";
    return j;
}

inline void Worker()
{
    State& s = S();
    Conn c;
    bool up = false;
    int nonce = 1;
    std::unique_lock<std::mutex> lock(s.m);
    while (s.run)
    {
        if (!up)
        {
            lock.unlock();
            const bool opened = c.Open();
            if (opened)
            {
                uint32_t rop = 0;
                up = c.Send(0, "{\"v\":1,\"client_id\":\"" + Esc(s.appId) + "\"}") && c.Receive(&rop);
                fprintf(stderr, "[discord] handshake %s (reply %u)\n", up ? "ok" : "failed", (unsigned)rop);
                if (!up) c.Close();
            }
            else fprintf(stderr, "[discord] no Discord pipe found\n");
            lock.lock();
            if (!up) { s.cv.wait_for(lock, std::chrono::seconds(15), [&] { return !s.run; }); continue; }
            s.dirty = true; // (just connected: say where we are)
        }
        if (!s.dirty) { s.cv.wait_for(lock, std::chrono::seconds(30), [&] { return s.dirty || !s.run; }); if (!s.run) break; if (!s.dirty) continue; }
        s.dirty = false;
        const std::string json = Activity(s, nonce++);
        lock.unlock();
        uint32_t aop = 0;
        const bool ok = c.Send(1, json) && c.Receive(&aop);
        fprintf(stderr, "[discord] status %s (reply %u)\n", ok ? "sent" : "failed", (unsigned)aop);
        lock.lock();
        if (!ok) { c.Close(); up = false; s.dirty = true; s.cv.wait_for(lock, std::chrono::seconds(15), [&] { return !s.run; }); }
    }
    if (up) { State blank; lock.unlock(); c.Send(1, Activity(blank, nonce++)); c.Receive(); c.Close(); }
}
#endif
} // namespace Detail

// Set what Discord shows: details (first line) and state (second, may be empty); empty details clears it.
inline void Set(const std::string& details, const std::string& state = "", bool timer = false)
{
#ifndef FDX_DISCORD_NONE
    using namespace Detail;
    State& s = S();
    {
        std::lock_guard<std::mutex> g(s.m);
        if (!s.run)
        {
            if (!Allowed()) return;
            s.appId = AppId();
            if (s.appId.empty()) return; // (no Application ID: off)
            s.run = true;
            s.th = std::thread(Worker);
        }
        s.details = details;
        s.state = state;
        s.start = timer ? (long long)time(nullptr) : 0;
        s.dirty = true;
    }
    s.cv.notify_all();
#else
    (void)details; (void)state; (void)timer;
#endif
}
inline void InLauncher() { Set("In the launcher", "", true); }
inline void Playing(const std::string& game) { Set("Playing " + game, "", true); }

// The status is cleared and the connection closed (when the program ends or restarts).
inline void Stop()
{
#ifndef FDX_DISCORD_NONE
    using namespace Detail;
    State& s = S();
    {
        std::lock_guard<std::mutex> g(s.m);
        if (!s.run) return;
        s.run = false;
    }
    s.cv.notify_all();
    if (s.th.joinable()) s.th.join();
#endif
}
} // namespace DiscordPresence
