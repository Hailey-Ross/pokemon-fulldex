#include "launcher.h"
#include "optional_features.h"
#include "achievements.h"
#include "controls.h"
#include "auto_state.h"
#include "state_slots.h"
#include <cassert>
#include <fstream>
#include <sstream>

namespace Games {
bool ReadFile(const std::string& path, std::vector<uint8_t>& data, size_t limit) {
    std::ifstream in(path, std::ios::binary);
    data.assign(std::istreambuf_iterator<char>(in), {});
    return bool(in) && data.size() <= limit;
}
bool WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return bool(out);
}
}
namespace DsConfig {
#include "ds_settings.h"
}
#define GBAW_EMBEDDED 1
namespace GbaConfig {
#include "gba_settings.h"
}
#undef GBAW_EMBEDDED
enum { P_DISPLAY, P_SOUND, P_GAMEPLAY, P_CONTROLS };
static std::vector<std::string> PageKeys(int pausePage) {
#include "gba_page_keys.h"
    return std::vector<std::string>(keys, keys + count);
}
static std::string Read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
int main(int argc, char** argv) {
    assert(argc == 2);
    AutoState::Timer timer;
    assert(!timer.Due(100, false, false));
    assert(!timer.Due(1000, true, false));
    assert(!timer.Due(60999, true, false));
    assert(!timer.Due(61000, true, true)); // No interval save while paused.
    assert(timer.Due(61000, true, false));
    assert(!timer.Due(61001, true, false));
    timer.Saved(65000); // Background/exit save restarts the interval.
    assert(!timer.Due(124999, true, false));
    assert(timer.Due(125000, true, false));
    assert(!timer.Due(130000, false, false));
    assert(!timer.Due(200000, true, false)); // Re-enabling starts a full new minute.
    AutoState::Timer wrapped;
    assert(!wrapped.Due(UINT32_MAX - 1000, true, false));
    assert(wrapped.Due(60000, true, false));

    const std::string root = argv[1], cfg = root + "/settings.cfg", quick = root + "/manual.state";
    DsConfig::Settings settings;
    assert(!settings.autoState && !settings.autoResume);
    { std::ofstream out(cfg); out << "auto_state=1\nauto_resume=1\n"; }
    settings.Load(cfg);
    assert(settings.autoState && settings.autoResume);
    settings.Save(cfg);
    DsConfig::Settings reload;
    reload.Load(cfg);
    assert(reload.autoState && reload.autoResume);
    Launcher::SyncGbaSettings(cfg, root);
    GbaConfig::Settings gbaDefaults;
    assert(gbaDefaults.Get("auto_state") == "0" && gbaDefaults.Get("auto_resume") == "0");
    const auto controlRows = PageKeys(P_CONTROLS);
    assert(controlRows.size() == 7);
    assert(std::find(controlRows.begin(), controlRows.end(), "auto_state") != controlRows.end());
    assert(std::find(controlRows.begin(), controlRows.end(), "auto_resume") != controlRows.end());
    const auto displayRows = PageKeys(P_DISPLAY);
    assert(std::find(displayRows.begin(), displayRows.end(), "show_fps") != displayRows.end());
    GbaConfig::Settings gba;
    gba.Load(root + "/gba-settings.ini");
    assert(gba.Get("auto_state") == "1" && gba.Get("auto_resume") == "1");
    gba.values["auto_state"] = "0";
    gba.ToHost("auto_state");
    settings.Load(cfg);
    assert(!settings.autoState && settings.autoResume);
    reload.Apply("auto_state=0"); reload.Apply("auto_resume=0"); reload.Save(cfg);
    settings.Load(cfg);
    assert(!settings.autoState && !settings.autoResume);

    const std::string automatic = AutoState::Path(quick, 0x12345678, 134217728);
    assert(!AutoState::Present(automatic));
    { std::ofstream empty(automatic); }
    assert(AutoState::Present(automatic));
    std::filesystem::remove(std::filesystem::u8path(automatic));
    assert(automatic != quick && automatic != StateSlots::Path(quick, 0));
    assert(automatic != AutoState::Path(quick, 0x12345679, 134217728));
    assert(automatic != AutoState::Path(quick, 0x12345678, 134217729));
    { std::ofstream out(quick); out << "manual slot unchanged"; }
    auto write = [](const std::string& path, const char* text) { std::ofstream out(path); out << text; return bool(out); };
    assert(StateSlots::Save(automatic, [&](const std::string& path) { return write(path, "first auto state"); }));
    assert(StateSlots::Save(automatic, [&](const std::string& path) { return write(path, "second auto state"); }));
    assert(Read(automatic + ".previous") == "first auto state");
    assert(!StateSlots::Save(automatic, [&](const std::string& path) { write(path, "partial"); return false; }));
    assert(Read(automatic) == "second auto state");
    assert(Read(automatic + ".pending") == "partial");
    assert(Read(quick) == "manual slot unchanged");
}
