#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "launcher.h"
#include <cassert>
#include <cstdio>

const LauncherFontInfo kLauncherFonts[] = {{"noto", "Noto", "Sans", nullptr, nullptr, 1}};
const int kLauncherFontCount = 1;
int main(int argc, char** argv) {
    assert(argc == 2);
    Launcher::HubDir() = argv[1];
    Launcher::GlassOn() = true;
    Menu::Clean = true;
    for (bool phone : {true, false}) {
        Launcher::PhoneUi() = phone;
        for (auto size : {std::pair<int,int>{0,0},{1,1},{0,640},{640,0},{15,15},{20,20},{360,640},{640,360}}) {
            Canvas canvas;
            canvas.Resize(size.first, size.second);
            const auto grid = Launcher::Layout(canvas);
            assert(grid.cols > 0 && grid.colStep > 0 && grid.rowStep > 0);
            assert(grid.cw > 0 && grid.ch > 0);
            for (bool empty : {true, false}) {
                std::vector<Launcher::Game> games(empty ? 0 : 1);
                if (!empty) {games[0].platform = Launcher::DS; games[0].name = "Synthetic game";}
                const auto view = Launcher::MakeView(canvas, games, Launcher::TAB_ALL);
                assert(view.g.cols > 0 && view.height >= 0);
            }
        }
    }
    std::puts("Uninitialized, zero-sized, tiny and rotated launcher canvases remain safe with empty and populated libraries.");
}
