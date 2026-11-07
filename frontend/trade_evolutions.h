#pragma once
// TRADE EVOLUTIONS WITHOUT TRADING (off unless chosen): the Pokémon that only evolve when traded
// evolve another way instead, as the Universal Pokémon Randomizer's "change impossible evolutions"
// does, so a player with one game can still complete the Pokédex:
//
//   traded (Kadabra, Machoke, Graveler, Haunter, Boldore, Gurdurr)    -> at level 37
//   traded holding an item (Onix + Metal Coat, Seadra + Dragon Scale)  -> level up holding the item
//   Slowpoke holding a King's Rock (it levels into Slowbro already)   -> a Water Stone
//   Karrablast and Shelmet (traded for each other)                    -> level up with the other in the party
//   GBA games (no "holding an item" in them): by level, or a Water Stone for Slowpoke and Clamperl
//
// Only the copy of the game in memory changes, before it starts; the ROM file never does. The
// game then evolves them itself, with its own evolution scene.
#include <cstdint>
#include <string>
#include <vector>

namespace TradeEvolutions
{
// A DS game (Diamond, Pearl, Platinum, HeartGold, SoulSilver, Black, White, Black 2, White 2):
// the evolutions changed (0: not one of those games, or none found). `log` says which.
int PatchNds(std::vector<uint8_t>& rom, std::string& log);
int PatchNds(uint8_t* rom, size_t size, std::string& log);

// A GBA game (Ruby, Sapphire, Emerald, FireRed, LeafGreen; not ROM hacks, whose tables differ).
int PatchGba(uint8_t* rom, size_t size, std::string& log);
}
