// SPDX-License-Identifier: GPL-3.0-or-later
#include "box_art.h"

#include <SDL.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace GbaWide
{

namespace
{

// libretro-thumbnails' Named_Boxarts, by No-Intro name
const char* const kHost = "raw.githubusercontent.com";
const char* const kPath = "/libretro-thumbnails/Nintendo_-_Game_Boy_Advance/master/Named_Boxarts/";
const std::map<std::string, const char*> kTitles = {
    { "firered", "Pokemon - FireRed Version (USA, Europe)" },
    { "leafgreen", "Pokemon - LeafGreen Version (USA, Europe)" },
    { "emerald", "Pokemon - Emerald Version (USA, Europe)" },
};

std::mutex gLock;
std::condition_variable gDone;
std::set<std::string> gBusy;
std::map<std::string, std::vector<unsigned char>> gLoaded;

std::string Folder()
{
    static std::string folder;
    if (folder.empty())
        if (char* p = SDL_GetPrefPath("gbawide", "box-art"))
        {
            folder = p;
            SDL_free(p);
        }
    return folder;
}

std::string Encode(const char* s)
{
    std::string out;
    for (; *s; s++)
    {
        const unsigned char c = (unsigned char)*s;
        if (isalnum(c) || c == '-' || c == '.' || c == '_') out += (char)c;
        else
        {
            char hex[4];
            snprintf(hex, sizeof hex, "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

bool ReadAll(const std::string& path, std::vector<unsigned char>& out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    unsigned char buf[65536];
    size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    fclose(f);
    return !out.empty();
}

#if defined(GBAW_FULLDEX_HTTP)
}
}
// Fulldex's own way to the internet on each platform (frontend/achievements_http.cpp; an empty post is a GET).
namespace Achievements
{
bool HttpPost(const std::string& url, const std::string& post, const std::string& contentType,
              const std::string& userAgent, int& status, std::string& body, std::string& error);
}
namespace GbaWide
{
namespace
{
bool Download(const std::string& path, std::vector<unsigned char>& out)
{
    int status = 0;
    std::string body, error;
    if (!Achievements::HttpPost(std::string("https://") + kHost + path, "", "", "gbawide", status, body, error) || status != 200)
        return false;
    if (body.size() <= 8 || body.size() > (8u << 20) || memcmp(body.data(), "\x89PNG", 4)) return false;
    out.assign(body.begin(), body.end());
    return true;
}
#elif defined(_WIN32)
bool Download(const std::string& path, std::vector<unsigned char>& out)
{
    bool ok = false;
    HINTERNET s = WinHttpOpen(L"gbawide", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return false;
    WinHttpSetTimeouts(s, 5000, 5000, 10000, 20000);
    std::wstring host(kHost, kHost + strlen(kHost)), wpath(path.begin(), path.end());
    HINTERNET c = WinHttpConnect(s, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (r && WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr))
    {
        DWORD status = 0, len = sizeof status;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
        if (status == 200)
        {
            out.clear();
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(r, &avail) && avail && out.size() < (8u << 20))
            {
                const size_t at = out.size();
                out.resize(at + avail);
                DWORD got = 0;
                if (!WinHttpReadData(r, out.data() + at, avail, &got)) break;
                out.resize(at + got);
            }
            ok = out.size() > 8 && !memcmp(out.data(), "\x89PNG", 4);
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok;
}
#else
bool Download(const std::string&, std::vector<unsigned char>&) { return false; }
#endif

}

void FetchBoxArt(const char* const* names, size_t count, int waitMs)
{
    const std::string folder = Folder();
    if (folder.empty()) return;
    std::vector<std::string> started;
    for (size_t i = 0; i < count; i++)
    {
        const auto t = kTitles.find(names[i]);
        if (t == kTitles.end()) continue;
        const std::string name = names[i], file = folder + name + ".png";
        if (FILE* f = fopen(file.c_str(), "rb"))
        {
            fclose(f);
            continue;
        }
        {
            std::lock_guard<std::mutex> l(gLock);
            if (!gBusy.insert(name).second) continue;
        }
        started.push_back(name);
        const std::string path = std::string(kPath) + Encode(t->second) + ".png";
        std::thread([name, file, path]() {
            std::vector<unsigned char> png;
            if (Download(path, png))
                if (FILE* f = fopen((file + ".tmp").c_str(), "wb"))
                {
                    const bool written = fwrite(png.data(), 1, png.size(), f) == png.size();
                    fclose(f);
                    if (written) rename((file + ".tmp").c_str(), file.c_str());
                    else remove((file + ".tmp").c_str());
                }
            std::lock_guard<std::mutex> l(gLock);
            gBusy.erase(name);
            gDone.notify_all();
        }).detach();
    }
    if (started.empty() || waitMs <= 0) return;
    std::unique_lock<std::mutex> l(gLock);
    gDone.wait_for(l, std::chrono::milliseconds(waitMs), [&]() {
        for (const std::string& n : started)
            if (gBusy.count(n)) return false;
        return true;
    });
}

bool CachedBoxArt(const std::string& name, const unsigned char*& data, size_t& size)
{
    data = nullptr;
    size = 0;
    if (!kTitles.count(name)) return false;
    std::lock_guard<std::mutex> l(gLock);
    auto it = gLoaded.find(name);
    if (it == gLoaded.end())
    {
        std::vector<unsigned char> png;
        if (gBusy.count(name) || !ReadAll(Folder() + name + ".png", png)) return false;
        it = gLoaded.emplace(name, std::move(png)).first;
    }
    data = it->second.data();
    size = it->second.size();
    return true;
}

}
