#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../frontend/launcher.h"
#include "../frontend/picture_browser.h"
#include <cassert>
#include <filesystem>
#include <fstream>

int main(int argc, char** argv)
{
    assert(argc == 2);
    namespace fs = std::filesystem;
    const std::string root = argv[1], saves = root + "/saves";
    const std::string red = root + "/Red picture.PNG", blue = root + "/Blue.jpeg";
    const std::string first = root + "/A/Same.nds", second = root + "/B/Same.nds";
    std::string error;
    assert(Launcher::ImportArt(first, saves, red, error));
    auto names = Launcher::ReadPairs(saves + "/fulldex-art.txt");
    const std::string firstName = names[first];
    assert(!firstName.empty());
    assert(Launcher::ImportArt(second, saves, blue, error));
    names = Launcher::ReadPairs(saves + "/fulldex-art.txt");
    assert(names[first] != names[second]);
    assert(names[first] == firstName);
    assert(Launcher::ImportArt(first, saves, red, error));
    assert(Launcher::ReadPairs(saves + "/fulldex-art.txt") == names);
    Launcher::Game a, b;
    a.rom = first; b.rom = second;
    std::vector<Launcher::Game> games = {a, b};
    Launcher::ApplyArt(games, saves);
    assert(games[0].art.size() == Launcher::Art * Launcher::Art);
    assert(games[0].art[0] == 0xFFFF0000u);
    assert(games[1].art[0] == 0xFF0000FFu);
    assert(!Launcher::ImportArt(first, saves, root + "/Invalid.png", error));
    assert(!error.empty());
    assert(Launcher::ReadPairs(saves + "/fulldex-art.txt") == names);
    fs::create_directory(fs::u8path(root + "/Nested"));
    std::ofstream(root + "/Ignored.exe") << "not a picture";
    std::ofstream(root + "/.hidden.png") << "hidden";
    const auto entries = PictureBrowser::Entries(root);
    assert(entries.size() == 5); // Nested, saves and three visible image filenames.
    assert(entries[0].directory && entries[1].directory);
    for (const auto& entry : entries) {
        assert(entry.label != "Ignored.exe" && entry.label != ".hidden.png");
    }
    assert(PictureBrowser::Entries(root + "/missing").empty());
}
