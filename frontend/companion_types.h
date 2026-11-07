// Type matchups adapted from PokeDaisy TypeChart.kt and TypeChartUtil.kt (GPL-3.0).
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
// Reference commit: 52f959770de4085bacfda16e6d77be7e91134bc4
#pragma once
#include <string>

namespace CompanionTypes
{
// Generation 3-5 type numbers; 9 is the unused mystery type.
constexpr int None = -1;
inline bool Valid(int type) { return type >= 0 && type < 18 && type != 9; }
inline const char* Name(int type)
{
    static const char* names[] = { "NORMAL", "FIGHTING", "FLYING", "POISON", "GROUND",
        "ROCK", "BUG", "GHOST", "STEEL", "???", "FIRE", "WATER", "GRASS",
        "ELECTRIC", "PSYCHIC", "ICE", "DRAGON", "DARK" };
    return Valid(type) ? names[type] : "NONE";
}
inline int Cycle(int type, int direction, bool allowNone = false)
{
    do { type = (type + (direction < 0 ? -1 : 1) + 19) % 19; }
    while (type == 9 || (!allowNone && type == 18));
    return type == 18 ? None : type;
}
inline int Single(int attack, int defence)
{
    if (!Valid(attack) || !Valid(defence)) return 100;
    switch (attack * 100 + defence)
    {
    case 5: return 50;
    case 8: return 50;
    case 1010: return 50;
    case 1011: return 50;
    case 1012: return 200;
    case 1015: return 200;
    case 1006: return 200;
    case 1005: return 50;
    case 1016: return 50;
    case 1008: return 200;
    case 1110: return 200;
    case 1111: return 50;
    case 1112: return 50;
    case 1104: return 200;
    case 1105: return 200;
    case 1116: return 50;
    case 1311: return 200;
    case 1313: return 50;
    case 1312: return 50;
    case 1304: return 0;
    case 1302: return 200;
    case 1316: return 50;
    case 1210: return 50;
    case 1211: return 200;
    case 1212: return 50;
    case 1203: return 50;
    case 1204: return 200;
    case 1202: return 50;
    case 1206: return 50;
    case 1205: return 200;
    case 1216: return 50;
    case 1208: return 50;
    case 1511: return 50;
    case 1512: return 200;
    case 1515: return 50;
    case 1504: return 200;
    case 1502: return 200;
    case 1516: return 200;
    case 1508: return 50;
    case 1510: return 50;
    case 100: return 200;
    case 115: return 200;
    case 103: return 50;
    case 102: return 50;
    case 114: return 50;
    case 106: return 50;
    case 105: return 200;
    case 117: return 200;
    case 108: return 200;
    case 312: return 200;
    case 303: return 50;
    case 304: return 50;
    case 305: return 50;
    case 307: return 50;
    case 308: return 0;
    case 410: return 200;
    case 413: return 200;
    case 412: return 50;
    case 403: return 200;
    case 402: return 0;
    case 406: return 50;
    case 405: return 200;
    case 408: return 200;
    case 213: return 50;
    case 212: return 200;
    case 201: return 200;
    case 206: return 200;
    case 205: return 50;
    case 208: return 50;
    case 1401: return 200;
    case 1403: return 200;
    case 1414: return 50;
    case 1417: return 0;
    case 1408: return 50;
    case 610: return 50;
    case 612: return 200;
    case 601: return 50;
    case 603: return 50;
    case 602: return 50;
    case 614: return 200;
    case 607: return 50;
    case 617: return 200;
    case 608: return 50;
    case 510: return 200;
    case 515: return 200;
    case 501: return 50;
    case 504: return 50;
    case 502: return 200;
    case 506: return 200;
    case 508: return 50;
    case 700: return 0;
    case 714: return 200;
    case 717: return 50;
    case 708: return 50;
    case 707: return 200;
    case 1616: return 200;
    case 1608: return 50;
    case 1701: return 50;
    case 1714: return 200;
    case 1707: return 200;
    case 1717: return 50;
    case 1708: return 50;
    case 810: return 50;
    case 811: return 50;
    case 813: return 50;
    case 815: return 200;
    case 805: return 200;
    case 808: return 50;
    case 7: return 0;
    case 107: return 0;
    default: return 100;
    }
}
inline int Multiplier(int attack, int first, int second = None)
{
    int pct = Single(attack, first);
    if (Valid(second) && second != first) pct = pct * Single(attack, second) / 100;
    return pct;
}
inline const char* Label(int pct)
{
    switch (pct) { case 0: return "IMMUNE 0X"; case 25: return "RESISTED 0.25X";
        case 50: return "RESISTED 0.5X"; case 200: return "SUPER 2X";
        case 400: return "SUPER 4X"; default: return "NEUTRAL 1X"; }
}
}
