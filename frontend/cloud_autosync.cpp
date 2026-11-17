#include "cloud_autosync.h"
#include "cloud_saves.h"
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

namespace CloudAuto
{
namespace
{
std::shared_ptr<CloudSaves::Sync> g_sync;
std::string g_save;
std::thread g_job;
std::atomic<bool> g_busy{ false };
bool g_pending = false;
std::chrono::steady_clock::time_point g_last;

const char* Device()
{
#if defined(__SWITCH__)
    return "Nintendo Switch";
#elif defined(__ANDROID__)
    return "Android Device";
#elif defined(__APPLE__)
    return "Apple Device";
#elif defined(_WIN32)
    return "Windows PC";
#else
    return "Linux PC";
#endif
}
std::string Upper(std::string s)
{
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}
void Send()
{
    if (g_job.joinable()) g_job.join();
    auto sync = g_sync; // (held by the thread, so Finish may let go while it still runs)
    if (!sync) return;
    const std::string save = g_save;
    g_busy = true;
    g_job = std::thread([sync, save] {
        const CloudSaves::Result r = sync->AfterPlay(save);
        fprintf(stderr, "[cloud] after a save: %s\n", r.message.c_str());
        g_busy = false;
    });
}
// (Waits up to `seconds` for the thread; true when it finished.)
bool Wait(int seconds)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (g_busy && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (g_busy) return false;
    if (g_job.joinable()) g_job.join();
    return true;
}
bool HasFile(const std::string& path)
{
    if (FILE* f = fopen(path.c_str(), "rb")) { fclose(f); return true; }
    return false;
}
}

std::string Start(const std::string& configDir, const std::string& savePath)
{
    if (!Wait(5)) { fprintf(stderr, "[cloud] still busy: this start is not synced\n"); return ""; }
    g_pending = false;
    // (The launcher's Hub writes cloud.ini into the settings folder beside the program (FOLDERS);
    // the standalone game, the Switch and older layouts keep it in the program's own folder.)
    const std::string cloudDir = HasFile(configDir + "/settings/cloud.ini") ? configDir + "/settings" : configDir;
    CloudSaves::Settings s = CloudSaves::LoadSettings(cloudDir + "/cloud.ini");
    g_sync = CloudSaves::FromSettings(s, cloudDir + "/cloud-state.txt", s.deviceName.empty() ? Device() : s.deviceName);
    if (!g_sync)
    {
        fprintf(stderr, "[cloud] off (no cloud.ini in %s or its settings folder)\n", configDir.c_str());
        return "";
    }
    g_save = savePath;
    const CloudSaves::Result r = g_sync->BeforePlay(savePath);
    fprintf(stderr, "[cloud] before play: %s\n", r.message.c_str());
    switch (r.outcome)
    {
    case CloudSaves::Outcome::Pulled:
        return "CLOUD SAVE: THE NEWER SAVE FROM " + Upper(r.cloud.device.empty() ? std::string("ANOTHER DEVICE") : r.cloud.device) + " WAS BROUGHT DOWN";
    case CloudSaves::Outcome::Conflict:
        return "CLOUD SAVE CHANGED ON TWO DEVICES: THIS ONE IS PLAYED. CHOOSE WHICH TO KEEP IN THE HUB";
    case CloudSaves::Outcome::Failed:
        return "CLOUD SAVES COULD NOT BE REACHED: THIS SAVE SYNCS NEXT TIME";
    default:
        return "";
    }
}

void Saved()
{
    if (!g_sync) return;
    g_pending = true;
    g_last = std::chrono::steady_clock::now();
}

void Tick()
{
    // (The games write their save in parts over a few seconds: it goes up once it has been still a while.)
    if (!g_sync || !g_pending || g_busy) return;
    if (std::chrono::steady_clock::now() - g_last < std::chrono::seconds(8)) return;
    g_pending = false;
    Send();
}

void Finish()
{
    if (!g_sync) return;
    if (!Wait(6)) { g_job.detach(); return; }
    Send();
    if (!Wait(6)) g_job.detach();
    g_sync.reset();
}
}
