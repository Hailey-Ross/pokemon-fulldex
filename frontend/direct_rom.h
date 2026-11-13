#pragma once
#include <filesystem>
#include "launcher.h"

// Direct launches may name a file outside the configured library. Read it in place and keep
// the established save folder; never import over a different ROM with the same filename.
namespace DirectRom {
inline int Ds(const std::string& path, const std::string& saves, const std::string& artwork,
              std::vector<Launcher::Game>& games)
{
    if (!Launcher::EndsWithNds(path)) return -1;
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(std::filesystem::u8path(path), error);
    if (error || !std::filesystem::is_regular_file(canonical, error) || error) return -1;
    for (size_t i=0; i<games.size(); ++i) {
        const auto existing = std::filesystem::weakly_canonical(std::filesystem::u8path(games[i].rom), error);
        if (!error && existing == canonical) return (int)i;
    }
    Launcher::Game game;
    Launcher::BeginScan(saves);
    const bool read = Launcher::ReadGame(canonical.u8string(), saves, game);
    Launcher::EndScan();
    if (!read || !game.known) return -1;
    Launcher::FindArt(game, artwork);
    games.push_back(std::move(game));
    return (int)games.size()-1;
}
}
