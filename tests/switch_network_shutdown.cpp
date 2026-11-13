// Exercise the actual Switch HTTP implementation with exhausted exit callbacks.
// Host headers are loaded before selecting the Switch branch; no Switch is needed.
// Linux: g++ -std=c++17 -pthread -Itests/switch_http_mock tests/switch_network_shutdown.cpp
//   -Wl,--wrap=curl_global_init -Wl,--wrap=curl_global_cleanup -lcurl -o /tmp/switch-http-test
// Run with: unused, normal, registered, socket-failure, curl-failure.
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>
#include <curl/curl.h>
static int opened, closed, curlOpened, curlClosed, registrations;
static bool failSockets, failCurl;
static bool rejectRegistration = true;
static void (*exitCallback)();
int socketInitializeDefault() { opened++; return failSockets ? 1 : 0; }
void socketExit() { closed++; }
void svcSleepThread(uint64_t) { assert(false && "no transfer should be running"); }
int SaturatedAtexit(void (*fn)(void))
{
    registrations++;
    if (rejectRegistration) return -1;
    exitCallback = fn;
    return 0;
}
namespace SwitchPlatform { void ExitTrace(const char*) {} }
extern "C" CURLcode __wrap_curl_global_init(long)
{
    curlOpened++;
    return failCurl ? CURLE_FAILED_INIT : CURLE_OK;
}
extern "C" void __wrap_curl_global_cleanup() { curlClosed++; }
#undef __linux__
#define __SWITCH__
#define atexit SaturatedAtexit
#include "../frontend/achievements_http.cpp"
#undef atexit

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::string mode = argv[1];
    if (mode == "unused")
    {
        Achievements::ShutdownNetwork();
        assert(opened == 0 && closed == 0 && curlClosed == 0);
        return 0;
    }
    failSockets = mode == "socket-failure";
    failCurl = mode == "curl-failure";
    rejectRegistration = mode != "registered";
    const bool ready = Achievements::EnsureNetwork();
    assert(ready == !(failSockets || failCurl));
    if (!failSockets)
    {
        assert(registrations == 1);
        assert(closed == 0);
    }
    if (exitCallback) exitCallback(); // normal atexit cleanup before the libnx hook
    // The libnx hook must close resources despite the rejected atexit registration.
    Achievements::ShutdownNetwork();
    assert(closed == (failSockets ? 0 : 1));
    assert(curlClosed == (ready ? 1 : 0));
    Achievements::ShutdownNetwork();
    assert(closed == (failSockets ? 0 : 1)); // duplicate shutdown is safe
    assert(curlClosed == (ready ? 1 : 0));
}
