#pragma once
// Pokemon Black's boss teams, from the cart data PokeDaisy's scripts/gen_gen5_guide.py
// reads (BLACK_BOSSES_RAW: levels, species ids and moves), with the species names and types
// from the same cart (Gen5TablesBlack.kt). PokeDaisy is GPL-3.0, by Lidor Itzhari -
// https://github.com/lidor30/pokedaisy. Facts only: no text is copied.
#include <cstring>
#include <string>
namespace Companion
{
struct BlackBossMon { int level; const char* name; const char* type; };
struct BlackBoss { const char* key; const char* title; const char* place; const char* badge; const char* reward; int badgeIndex; int count; BlackBossMon team[6]; };
struct BlackBossInfo { std::string title, hint, answer; bool any = false; };
inline const BlackBoss* BlackBossList(int& n)
{
    static const BlackBoss bosses[] = {
        { "CHILI", "CHILI", "STRIATON CITY GYM", "TRIO BADGE", "TM83 WORK UP", 0, 2, { {12, "LILLIPUP", "NORMAL"}, {14, "PANSEAR", "FIRE"} } },
        { "CILAN", "CILAN", "STRIATON CITY GYM", "TRIO BADGE", "TM83 WORK UP", 0, 2, { {12, "LILLIPUP", "NORMAL"}, {14, "PANSAGE", "GRASS"} } },
        { "CRESS", "CRESS", "STRIATON CITY GYM", "TRIO BADGE", "TM83 WORK UP", 0, 2, { {12, "LILLIPUP", "NORMAL"}, {14, "PANPOUR", "WATER"} } },
        { "LENORA", "LENORA", "NACRENE CITY GYM", "BASIC BADGE", "TM67 RETALIATE", 1, 2, { {18, "HERDIER", "NORMAL"}, {20, "WATCHOG", "NORMAL"} } },
        { "BURGH", "BURGH", "CASTELIA CITY GYM", "INSECT BADGE", "TM76 STRUGGLE BUG", 2, 3, { {21, "WHIRLIPEDE", "BUG/POISON"}, {21, "DWEBBLE", "BUG/ROCK"}, {23, "LEAVANNY", "BUG/GRASS"} } },
        { "ELESA", "ELESA", "NIMBASA CITY GYM", "BOLT BADGE", "TM72 VOLT SWITCH", 3, 3, { {25, "EMOLGA", "ELECTRIC/FLYING"}, {25, "EMOLGA", "ELECTRIC/FLYING"}, {27, "ZEBSTRIKA", "ELECTRIC"} } },
        { "CLAY", "CLAY", "DRIFTVEIL CITY GYM", "QUAKE BADGE", "TM78 BULLDOZE", 4, 3, { {29, "KROKOROK", "GROUND/DARK"}, {29, "PALPITOAD", "WATER/GROUND"}, {31, "EXCADRILL", "GROUND/STEEL"} } },
        { "SKYLA", "SKYLA", "MISTRALTON CITY GYM", "JET BADGE", "TM62 ACROBATICS", 5, 3, { {33, "SWOOBAT", "PSYCHIC/FLYING"}, {33, "UNFEZANT", "NORMAL/FLYING"}, {35, "SWANNA", "WATER/FLYING"} } },
        { "BRYCEN", "BRYCEN", "ICIRRUS CITY GYM", "FREEZE BADGE", "TM79 FROST BREATH", 6, 3, { {37, "VANILLISH", "ICE"}, {37, "CRYOGONAL", "ICE"}, {39, "BEARTIC", "ICE"} } },
        { "DRAYDEN", "DRAYDEN", "OPELUCID CITY GYM", "LEGEND BADGE", "TM82 DRAGON TAIL", 7, 3, { {41, "FRAXURE", "DRAGON"}, {41, "DRUDDIGON", "DRAGON"}, {43, "HAXORUS", "DRAGON"} } },
        { "SHAUNTAL", "SHAUNTAL", "POKeMON LEAGUE", "", "", 8, 4, { {48, "COFAGRIGUS", "GHOST"}, {48, "JELLICENT", "WATER/GHOST"}, {48, "GOLURK", "GROUND/GHOST"}, {50, "CHANDELURE", "GHOST/FIRE"} } },
        { "MARSHAL", "MARSHAL", "POKeMON LEAGUE", "", "", 8, 4, { {48, "THROH", "FIGHTING"}, {48, "SAWK", "FIGHTING"}, {48, "CONKELDURR", "FIGHTING"}, {50, "MIENSHAO", "FIGHTING"} } },
        { "GRIMSLEY", "GRIMSLEY", "POKeMON LEAGUE", "", "", 8, 4, { {48, "SCRAFTY", "DARK/FIGHTING"}, {48, "LIEPARD", "DARK"}, {48, "KROOKODILE", "GROUND/DARK"}, {50, "BISHARP", "DARK/STEEL"} } },
        { "CAITLIN", "CAITLIN", "POKeMON LEAGUE", "", "", 8, 4, { {48, "REUNICLUS", "PSYCHIC"}, {48, "MUSHARNA", "PSYCHIC"}, {48, "SIGILYPH", "PSYCHIC/FLYING"}, {50, "GOTHITELLE", "PSYCHIC"} } },
        { "N", "N", "N'S CASTLE", "", "", 8, 6, { {52, "ZEKROM", "DRAGON/ELECTRIC"}, {50, "CARRACOSTA", "WATER/ROCK"}, {50, "VANILLUXE", "ICE"}, {50, "ARCHEOPS", "ROCK/FLYING"}, {50, "ZOROARK", "DARK"}, {50, "KLINKLANG", "STEEL"} } },
        { "GHETSIS", "GHETSIS", "N'S CASTLE", "", "", 8, 6, { {52, "COFAGRIGUS", "GHOST"}, {52, "BOUFFALANT", "NORMAL"}, {52, "SEISMITOAD", "WATER/GROUND"}, {52, "BISHARP", "DARK/STEEL"}, {52, "EELEKTROSS", "ELECTRIC"}, {54, "HYDREIGON", "DARK/DRAGON"} } },
        { "ALDER", "ALDER", "POKeMON LEAGUE", "", "", 9, 6, { {75, "ACCELGOR", "BUG"}, {75, "BOUFFALANT", "NORMAL"}, {75, "DRUDDIGON", "DRAGON"}, {75, "VANILLUXE", "ICE"}, {75, "ESCAVALIER", "BUG/STEEL"}, {77, "VOLCARONA", "BUG/FIRE"} } },
    };
    n = (int)(sizeof bosses / sizeof bosses[0]);
    return bosses;
}
inline const BlackBoss* FindBlackBoss(const char* key)
{
    int n = 0; const BlackBoss* b = BlackBossList(n);
    for (int i = 0; i < n; i++) if (!strcmp(b[i].key, key)) return &b[i];
    return nullptr;
}
inline std::string BlackTeamSummary(const BlackBoss& b)
{
    std::string s;
    for (int i = 0; i < b.count; i++)
    {
        if (i) s += ", ";
        s += "Lv " + std::to_string(b.team[i].level) + " " + b.team[i].name + " (" + b.team[i].type + ")";
    }
    return s;
}
// The next gym (or the Elite Four) for a BLACK save: badges 0-7 are the gyms, then the League.
// starter: 1 SNIVY, 2 TEPIG, 3 OSHAWOTT, 0 unknown (then all three Striaton teams are shown).
inline BlackBossInfo BlackNextBoss(int badges, int bestLevel, int starter)
{
    BlackBossInfo r;
    auto levelLine = [&](const BlackBoss& b) { int hi = 0; for (int i = 0; i < b.count; i++) hi = std::max(hi, b.team[i].level);
        std::string s = b.place;
        s += "  \xC2\xB7  HIGHEST Lv " + std::to_string(hi);
        if (bestLevel > 0) s += "  \xC2\xB7  YOUR BEST Lv " + std::to_string(bestLevel);
        return s; };
    auto gym = [&](const char* key) { const BlackBoss* b = FindBlackBoss(key); if (!b) return r; r.any = true; r.title = std::string("NEXT BOSS: ") + b->title;
        r.hint = levelLine(*b); r.answer = "Reward: " + std::string(b->badge) + " and " + b->reward + ". Team: " + BlackTeamSummary(*b) + "."; return r; };
    if (badges >= 0 && badges < 8)
    {
        static const char* keys[8] = { nullptr, "LENORA", "BURGH", "ELESA", "CLAY", "SKYLA", "BRYCEN", "DRAYDEN" };
        if (badges == 0)
        {
            const char* pick = starter == 1 ? "CHILI" : starter == 2 ? "CRESS" : starter == 3 ? "CILAN" : nullptr;
            if (pick) return gym(pick);
            const BlackBoss* c = FindBlackBoss("CHILI"); const BlackBoss* l = FindBlackBoss("CILAN"); const BlackBoss* s = FindBlackBoss("CRESS");
            if (!c || !l || !s) return r;
            r.any = true; r.title = "NEXT BOSS: STRIATON GYM";
            r.hint = "STRIATON CITY GYM  \xC2\xB7  CHILI, CILAN or CRESS  \xC2\xB7  the one whose type beats your starter";
            r.answer = "The leader is the one whose type beats your first POKeMON: CHILI (FIRE) against SNIVY, CRESS (WATER) against TEPIG, CILAN (GRASS) against OSHAWOTT. "
                "CHILI: " + BlackTeamSummary(*c) + ". CILAN: " + BlackTeamSummary(*l) + ". CRESS: " + BlackTeamSummary(*s) + ". Reward: TRIO BADGE and TM83 WORK UP.";
            return r;
        }
        return gym(keys[badges]);
    }
    // Eight badges: the League. The Elite Four may be taken in any order; N and GHETSIS follow.
    const BlackBoss* e4[4] = { FindBlackBoss("SHAUNTAL"), FindBlackBoss("GRIMSLEY"), FindBlackBoss("MARSHAL"), FindBlackBoss("CAITLIN") };
    if (!e4[0] || !e4[1] || !e4[2] || !e4[3]) return r;
    r.any = true; r.title = "NEXT BOSS: THE ELITE FOUR";
    r.hint = "POKeMON LEAGUE  \xC2\xB7  FOUR ROOMS  \xC2\xB7  HIGHEST Lv 50";
    if (bestLevel > 0) r.hint += "  \xC2\xB7  YOUR BEST Lv " + std::to_string(bestLevel);
    r.answer = "You may take the rooms in any order. SHAUNTAL (GHOST): " + BlackTeamSummary(*e4[0]) + ". "
        "GRIMSLEY (DARK): " + BlackTeamSummary(*e4[1]) + ". MARSHAL (FIGHTING): " + BlackTeamSummary(*e4[2]) + ". "
        "CAITLIN (PSYCHIC): " + BlackTeamSummary(*e4[3]) + ".";
    const BlackBoss* n = FindBlackBoss("N"); const BlackBoss* g = FindBlackBoss("GHETSIS");
    if (n && g) r.answer += " Then N: " + BlackTeamSummary(*n) + ". GHETSIS: " + BlackTeamSummary(*g) + ".";
    return r;
}
}
