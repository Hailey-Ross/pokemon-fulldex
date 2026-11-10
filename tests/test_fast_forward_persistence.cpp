#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "launcher.h"
#include "optional_features.h"
#include "achievements.h"
#include "controls.h"
#include <cassert>
#include <fstream>
#include <sstream>

namespace Games {
bool ReadFile(const std::string& path, std::vector<uint8_t>& data, size_t limit) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    data.assign(std::istreambuf_iterator<char>(f), {});
    return data.size() <= limit;
}
bool WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
    return bool(f);
}
}
namespace DsConfig {
#include "ds_settings.h"
}
namespace GbaConfig {
#include "gba_settings.h"
}
#include "ff_actions.h"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string root = argv[1], cfgPath = root + "/settings.cfg", ini = root + "/gba-settings.ini";
    // Fresh Deck defaults need a menu button even when Steam owns Guide.
    SDL_setenv("SteamDeck", "1", 1);
    DsConfig::Settings deckDs;
    GbaConfig::Settings deckGba;
    assert(deckDs.startMenu && deckGba.Get("gba_start") == "menu");
    { std::ofstream f(cfgPath); f << "start_button=ds\n"; }
    deckDs.Load(cfgPath);
    deckGba.values["gba_start"] = "start";
    assert(!deckDs.startMenu && deckGba.Get("gba_start") == "start");
    std::remove(cfgPath.c_str());
    SDL_setenv("SteamDeck", "0", 1);
    GbaConfig::Settings desktopGba;
    assert(desktopGba.Get("gba_start") == "start");
#ifndef __SWITCH__
    assert(!DsConfig::Settings{}.startMenu);
#endif
    DsConfig::Settings cfg;
    cfg.Load(cfgPath);
    assert(!DsStartsFast(cfg)); // old files with no key keep the OFF default
    cfg.volume = 40;
    cfg.screenFilter = 2;
    cfg.romsFolder = "Shared ROMs";
    bool dsFast = DsStartsFast(cfg);
    ToggleDs(cfg, cfgPath, dsFast); // real DS pause action saves immediately
    DsConfig::Settings reopened;
    reopened.Load(cfgPath);
    assert(dsFast && DsStartsFast(reopened));
    assert(reopened.volume == 40 && reopened.screenFilter == 2 && reopened.romsFolder == "Shared ROMs");
    Launcher::SyncGbaSettings(cfgPath, root);
    GbaConfig::Settings set;
    set.Load(ini);
    bool gbaFast = GbaStartsFast(set);
    assert(gbaFast);
    ToggleGba(set, gbaFast); // real GBA pause action saves and writes back to DS
    GbaConfig::Settings gbaReopened;
    gbaReopened.Load(ini);
    assert(!gbaFast && !GbaStartsFast(gbaReopened));
    DsConfig::Settings dsAfterGba;
    dsAfterGba.Load(cfgPath);
    assert(!DsStartsFast(dsAfterGba) && dsAfterGba.volume == 40);
    ToggleGba(gbaReopened, gbaFast);
    assert(gbaFast);
    DsConfig::Settings dsOn;
    dsOn.Load(cfgPath);
    assert(DsStartsFast(dsOn));
    dsFast = DsStartsFast(dsOn);
    ToggleDs(dsOn, cfgPath, dsFast);
    Launcher::SyncGbaSettings(cfgPath, root);
    gbaReopened.Load(ini);
    assert(!dsFast && !GbaStartsFast(gbaReopened));
    // Temporary input state must not become the next launch's preference.
    gbaFast = !GbaStartsFast(gbaReopened);
    assert(gbaFast);
    gbaReopened.Load(ini);
    assert(!GbaStartsFast(gbaReopened));
    // No host key: preserve an existing standalone GBA preference.
    { std::ofstream f(cfgPath); f << "volume=40\n"; }
    { std::ofstream f(ini); f << "fast_lock=1\ncolour=1\n"; }
    Launcher::SyncGbaSettings(cfgPath, root);
    gbaReopened.Load(ini);
    assert(GbaStartsFast(gbaReopened) && gbaReopened.Get("colour") == "1");
}
