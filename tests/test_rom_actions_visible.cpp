#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../frontend/launcher.h"
#include <cassert>

// Other launcher pages are linked but not visited by this ROM-page rendering test.
const LauncherFontInfo kLauncherFonts[] = {{"noto", "Noto", "Sans", nullptr, nullptr, 1}};
const int kLauncherFontCount = 1;
namespace CloudSaves { Settings LoadSettings(const std::string&) { return {}; } }
namespace HubLink { Status Snapshot() { return {}; } }

namespace ThemePack {
bool Jpeg(const uint32_t*, int, int, int, std::vector<unsigned char>&) { return false; }
bool ZipRead(const std::string&, std::map<std::string, std::vector<unsigned char>>&, size_t, const std::string&) { return false; }
}
namespace Achievements {
bool HttpRequest(const HttpCall&, int&, std::string&, std::string&) { return false; }
}

int main()
{
    Launcher::PhoneUi() = true;
    Canvas canvas;
    canvas.Resize(720, 1280);
    Launcher::Game game;
    game.name = "SYNTHETIC TEST";
    game.code = "CPUE";
    game.rom = "/synthetic/test.nds";
    game.saveName = "test.sav";
    Launcher::RomList() = {game};
    auto& focus = Launcher::Focus();
    focus = Launcher::GlassFocus{};
    focus.page = Launcher::SIDE_ROMS;
    focus.side = -1;
    focus.prow = 0;
    auto draw = [&]() {
        Launcher::DrawPage(canvas, Launcher::Spots(canvas), true, Launcher::GU(canvas));
    };
    draw();
    assert(Launcher::PageMove(2));
    for (int i = 1; i < Launcher::RomActions(); i++) assert(Launcher::PageMove(4));
    for (int i = 0; i < 4; i++) draw();
    auto visible = [&]() {
        const int bottom = focus.aY + Launcher::RomActions() * focus.aRow;
        assert(focus.aY >= focus.pdY);
        const int panelBottom = Launcher::Spots(canvas).navY - int(12 * Launcher::GU(canvas));
        assert(bottom <= panelBottom);
    };
    visible();
    assert(focus.dScroll > 0);
    // A finger can still scroll back to the ROM details without being pulled down again.
    focus.freeScroll = true;
    focus.dScroll = 0;
    draw();
    assert(focus.dScroll == 0);
    // Returning to controller navigation follows its chosen action again.
    assert(Launcher::PageMove(4));
    for (int i = 0; i < 4; i++) draw();
    visible();
}
