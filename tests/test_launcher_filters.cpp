#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "launcher.h"
#include <cassert>
#include <fstream>

// Only the GBA settings file boundary is supplied here; the settings implementation below
// is extracted from the real GBA frontend by the Python test.
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
namespace GbaConfig {
#include "gba_settings_under_test.h"
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string root = argv[1], cfg = root + "/settings.cfg";
    const std::string ini = root + "/gba-settings.ini";
    { std::ofstream f(cfg); f << "screen_filter=0\n"; }
    { std::ofstream f(ini); f << "lcd=1\ncolour=1\n"; }
    // Existing GBA choices survive until the launcher has an explicit override.
    Launcher::SyncGbaSettings(cfg, root);
    assert(Launcher::ConfigValue(ini, "lcd") == "1");
    Launcher::SettingsPage page;
    page.Open(cfg, true);
    page.Enter(1);
    auto select = [&](const char* key) {
        for (size_t i = 0; i < page.items.size(); ++i)
            if (page.items[i].key && !strcmp(page.items[i].key, key)) {
                page.selected = int(i);
                assert(Launcher::GroupOf(page.items[i]) == 1);
                return;
            }
        assert(false && "Display option missing");
    };
    select("screen_filter");
    assert(page.Shown(page.items[page.selected]) == "OFF");
    page.Change(1);
    assert(Launcher::ConfigValue(cfg, "screen_filter") == "1");
    page.Open(cfg, true); page.Enter(1); select("screen_filter");
    assert(page.Shown(page.items[page.selected]) == "SCANLINES");
    page.Change(1);
    assert(Launcher::ConfigValue(cfg, "screen_filter") == "2");
    assert(page.Shown(page.items[page.selected]) == "LCD GRID");
    select("gba_lcd");
    assert(page.Shown(page.items[page.selected]) == "AUTO");
    page.Change(1); // AUTO -> OFF.
    assert(Launcher::ConfigValue(cfg, "gba_lcd") == "0");
    Launcher::SyncGbaSettings(cfg, root);
    assert(Launcher::ConfigValue(ini, "lcd") == "0");
    assert(Launcher::ConfigValue(ini, "colour") == "1");

    GbaConfig::Settings gba;
    gba.Load(ini);
    bool found = false;
    for (const auto& option : GbaConfig::SettingsList())
        if (!strcmp(option.key, "lcd")) { gba.Step(option, 1); found = true; break; }
    assert(found);
    // The game's own setting writes back to the launcher without changing the DS filter.
    assert(Launcher::ConfigValue(cfg, "gba_lcd") == "1");
    assert(Launcher::ConfigValue(cfg, "screen_filter") == "2");
    page.Open(cfg, true); page.Enter(1); select("gba_lcd");
    assert(page.Shown(page.items[page.selected]) == "ON");
    Launcher::SyncGbaSettings(cfg, root);
    assert(Launcher::ConfigValue(ini, "lcd") == "1");
    page.Change(1); // ON -> AUTO keeps the current GBA choice.
    assert(Launcher::ConfigValue(cfg, "gba_lcd") == "auto");
    Launcher::SyncGbaSettings(cfg, root);
    assert(Launcher::ConfigValue(ini, "lcd") == "1");
}
