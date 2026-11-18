#pragma once
// Pokemon HeartGold/SoulSilver and Platinum's gyms, Elite Four and champions.
// Teams (levels and species) generated from the cart data PokeDaisy's generator reads
// (HG_BOSSES_RAW and PT_BOSSES_RAW); the names and types are looked up from the running
// game's own tables. Facts only.
#include <cstring>
namespace Companion
{
struct Gen4Mon {int level = 0, species = 0;};
struct Gen4Boss {const char* name = nullptr; int count = 0; Gen4Mon team[6];};
inline const Gen4Boss HgBosses[] = {
    {"FALKNER", 2, {{9, 16}, {13, 17}}},
    {"BUGSY", 3, {{17, 123}, {15, 14}, {15, 11}}},
    {"WHITNEY", 2, {{17, 35}, {19, 241}}},
    {"MORTY", 4, {{21, 92}, {21, 93}, {25, 94}, {23, 93}}},
    {"CHUCK", 2, {{29, 57}, {31, 62}}},
    {"JASMINE", 3, {{30, 81}, {30, 81}, {35, 208}}},
    {"PRYCE", 3, {{30, 86}, {32, 87}, {34, 221}}},
    {"CLAIR", 4, {{38, 130}, {38, 148}, {38, 148}, {41, 230}}},
    {"WILL", 5, {{40, 178}, {41, 124}, {41, 103}, {41, 80}, {42, 178}}},
    {"KOGA", 5, {{40, 168}, {41, 49}, {43, 205}, {42, 89}, {44, 169}}},
    {"BRUNO", 5, {{42, 237}, {42, 106}, {42, 107}, {43, 95}, {46, 68}}},
    {"KAREN", 5, {{42, 197}, {42, 45}, {45, 94}, {44, 198}, {47, 229}}},
    {"LANCE", 6, {{46, 130}, {49, 149}, {49, 149}, {48, 142}, {48, 6}, {50, 149}}},
    {"LT_SURGE", 5, {{51, 26}, {47, 101}, {47, 82}, {47, 101}, {53, 125}}},
    {"SABRINA", 3, {{53, 196}, {53, 122}, {55, 65}}},
    {"ERIKA", 4, {{51, 189}, {52, 114}, {56, 71}, {56, 182}}},
    {"MISTY", 4, {{49, 55}, {49, 195}, {52, 131}, {54, 121}}},
    {"JANINE", 5, {{47, 169}, {44, 110}, {47, 168}, {47, 168}, {50, 49}}},
    {"BROCK", 5, {{51, 75}, {51, 111}, {53, 139}, {54, 95}, {52, 141}}},
    {"BLAINE", 3, {{54, 219}, {54, 126}, {59, 78}}},
    {"BLUE", 6, {{55, 103}, {58, 59}, {58, 112}, {52, 130}, {56, 68}, {60, 18}}},
    {"RED", 6, {{88, 25}, {80, 131}, {82, 143}, {84, 3}, {84, 6}, {84, 9}}},
};
inline const Gen4Boss PtBosses[] = {
    {"ROARK", 3, {{12, 74}, {12, 95}, {14, 408}}},
    {"GARDENIA", 3, {{20, 387}, {20, 421}, {22, 407}}},
    {"FANTINA", 3, {{24, 355}, {24, 93}, {26, 429}}},
    {"MAYLENE", 3, {{28, 307}, {29, 67}, {32, 448}}},
    {"WAKE", 3, {{33, 130}, {34, 195}, {37, 419}}},
    {"BYRON", 3, {{37, 82}, {38, 208}, {41, 411}}},
    {"CANDICE", 4, {{40, 215}, {40, 221}, {42, 460}, {44, 478}}},
    {"VOLKNER", 4, {{46, 135}, {46, 26}, {48, 405}, {50, 466}}},
    {"AARON", 5, {{49, 469}, {49, 212}, {50, 416}, {51, 214}, {53, 452}}},
    {"BERTHA", 5, {{50, 340}, {53, 472}, {52, 450}, {52, 76}, {55, 464}}},
    {"FLINT", 5, {{52, 229}, {55, 136}, {53, 78}, {55, 392}, {57, 467}}},
    {"LUCIAN", 5, {{53, 122}, {55, 196}, {54, 437}, {56, 65}, {59, 475}}},
    {"CYNTHIA", 6, {{58, 442}, {58, 407}, {60, 468}, {60, 448}, {58, 350}, {62, 445}}},
};
inline const Gen4Boss* FindGen4Boss(const char* name, bool heartGold)
{
    const Gen4Boss* list = heartGold ? HgBosses : PtBosses;
    const int n = heartGold ? (int)(sizeof(HgBosses) / sizeof(HgBosses[0])) : (int)(sizeof(PtBosses) / sizeof(PtBosses[0]));
    for (int i = 0; i < n; i++) if (std::strcmp(list[i].name, name) == 0) return &list[i];
    return nullptr;
}
}
