#pragma once
// One internet request, any method, with headers and a body that may be binary. Each platform
// does it its own way (achievements_http.cpp, achievements_http_apple.mm); RetroAchievements and
// cloud saves both use it.
#include <cstddef>
#include <string>
#include <vector>

namespace Achievements
{
struct HttpCall
{
    std::string method = "GET";          // GET, POST, PUT, PATCH, DELETE
    std::string url;
    std::vector<std::string> headers;    // "Name: value"
    std::string body;                    // sent when not empty, or always for PUT/POST/PATCH
    std::string userAgent = "Pokemon-Fulldex";
    long timeout = 30;                   // seconds for the whole request
    size_t limit = 64u << 20;            // the most of a reply kept
};

// False when there was no answer at all (no network, bad address); a reply of any status is true.
bool HttpRequest(const HttpCall& call, int& status, std::string& body, std::string& error);
const char* SystemName();
#ifdef __SWITCH__
// The console's network, opened once and closed as the program ends (achievements_http.cpp).
bool EnsureNetwork();
#endif
}
