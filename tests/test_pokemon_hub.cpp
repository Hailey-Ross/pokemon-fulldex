// The Pokémon hub on real saves. test_pokemon_hub <folder of saves> <scratch folder>
// Every Pokémon in every save is checked against every other game; then moves are made and
// written (into copies in the scratch folder) and read back.
#include "pokemon_hub.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace PokemonHub;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static std::unique_ptr<Save> Load(const std::string& p)
{
    std::string e;
    auto s = Save::Open(p, e);
    if (!s) fprintf(stderr, "cannot open %s: %s\n", p.c_str(), e.c_str());
    return s;
}

int main(int, char** argv)
{
    const fs::path src = argv[1], work = argv[2];
    fs::remove_all(work);
    fs::create_directories(work);
    std::vector<std::string> names;
    for (auto& e : fs::directory_iterator(src))
        if (e.is_regular_file() && e.path().extension() != ".bin" && e.path().extension() != ".zip")
        {
            fs::copy_file(e.path(), work / e.path().filename());
            names.push_back(e.path().filename().string());
        }
    std::sort(names.begin(), names.end(), [&](const std::string& a, const std::string& b) {
        auto x = Load((work / a).string()), y = Load((work / b).string());
        return x->Generation() != y->Generation() ? x->Generation() < y->Generation() : a < b;
    });

    // 1. Every Pokémon against every other game.
    printf("%-22s gen  mons | to each other game: ok / not in that game / backwards / egg\n", "save");
    std::map<std::string, std::unique_ptr<Save>> saves;
    for (auto& n : names) saves[n] = Load((work / n).string());
    std::map<Verdict, int> total;
    for (auto& a : names)
    {
        const Save& from = *saves[a];
        int mons = 0;
        std::map<Verdict, int> v;
        for (int b = 0; b < from.Boxes(); b++)
            for (const Mon& m : from.Box(b))
            {
                mons++;
                for (auto& t : names)
                    if (t != a) v[CanMove(from, m.box, m.slot, *saves[t]).verdict]++;
            }
        printf("%-22s %d  %4d | %6d / %5d / %6d / %d\n", a.c_str(), from.Generation(), mons, v[Verdict::Ok], v[Verdict::NotInGame],
               v[Verdict::Backwards], v[Verdict::Egg]);
        for (auto& kv : v) total[kv.first] += kv.second;
        CHECK(from.Generation() >= 3 && from.Generation() <= 7);
    }
    printf("total checks: %d ok, %d not in that game, %d backwards, %d eggs\n\n", total[Verdict::Ok], total[Verdict::NotInGame],
           total[Verdict::Backwards], total[Verdict::Egg]);

    // 2. Backwards is always refused, whatever the Pokémon.
    {
        const Save& b2 = *saves["Black 2.sav"];
        const Mon m = b2.Box(0).at(0);
        Check c = CanMove(b2, m.box, m.slot, *saves["HeartGold.sav"]);
        CHECK(c.verdict == Verdict::Backwards);
        printf("Black 2 -> HeartGold: %s\n", c.reason.c_str());
    }

    // 3. Real moves, one across each generation boundary and one within a generation, written and
    //    read back: the Pokémon has left the first save and arrived in the second; both backed up.
    struct Case { const char* from; const char* to; };
    for (Case cs : { Case{ "Emerald.sav", "Diamond.sav" }, Case{ "Platinum.sav", "Black.sav" }, Case{ "Black 2.sav", "Y (3DS main)" },
                     Case{ "SoulSilver.sav", "Platinum.sav" }, Case{ "Y (3DS main)", "Omega Ruby (3DS main)" }, Case{ "Sapphire.sav", "Emerald.sav" },
                     Case{ "Emerald.sav", "Ruby.sav" } })
    {
        auto from = Load((work / cs.from).string()), to = Load((work / cs.to).string());
        // The first Pokémon that can go, preferring one that loses something or can evolve.
        Mon pick;
        Check chk;
        bool found = false;
        for (int b = 0; b < from->Boxes() && !found; b++)
            for (const Mon& m : from->Box(b))
            {
                Check c = CanMove(*from, m.box, m.slot, *to);
                if (c.verdict != Verdict::Ok) continue;
                if (!found || (!c.losses.empty() && chk.losses.empty()) || (c.evolvesTo && !chk.evolvesTo)) { pick = m; chk = c; found = true; }
                if (!c.losses.empty() || c.evolvesTo) break;
            }
        CHECK(found);
        if (!found) continue;
        printf("%s -> %s: box %d slot %d #%d \"%s\" lv %d", cs.from, cs.to, pick.box + 1, pick.slot + 1, pick.species, pick.nickname.c_str(), pick.level);
        for (auto& l : chk.losses) printf(" | %s", l.c_str());
        if (chk.evolvesTo) printf(" | %s", chk.evolution.c_str());
        printf("\n");
        int tb, ts;
        CHECK(to->FirstEmpty(tb, ts));
        std::string error;
        std::vector<std::string> backups;
        CHECK(Move(*from, pick.box, pick.slot, *to, true, error));
        CHECK(Commit(*from, *to, error, &backups));
        CHECK(backups.size() == 2);
        for (auto& b : backups) CHECK(fs::exists(b));
        auto from2 = Load((work / cs.from).string()), to2 = Load((work / cs.to).string());
        CHECK(from2 && to2);
        if (!from2 || !to2) continue;
        bool gone = true;
        for (const Mon& m : from2->Box(pick.box)) gone = gone && m.slot != pick.slot;
        CHECK(gone);
        bool arrived = false;
        for (const Mon& m : to2->Box(tb))
            if (m.slot == ts)
            {
                arrived = true;
                CHECK(m.species == (chk.evolvesTo ? chk.evolvesTo : pick.species));
                CHECK(m.level == pick.level);
                printf("   arrived: box %d slot %d #%d \"%s\" lv %d\n", tb + 1, ts + 1, m.species, m.nickname.c_str(), m.level);
            }
        CHECK(arrived);
        saves[cs.from] = std::move(from2);
        saves[cs.to] = std::move(to2);
    }

    // 4. Evolving as if traded, in place: the first that can, in every save; read back.
    for (auto& n : names)
    {
        auto sv = Load((work / n).string());
        bool done = false;
        for (int b = 0; b < sv->Boxes() && !done; b++)
            for (const Mon& m : sv->Box(b))
            {
                const Check c = CanEvolve(*sv, m.box, m.slot);
                if (c.verdict != Verdict::Ok) continue;
                std::string error;
                CHECK(Evolve(*sv, m.box, m.slot, error));
                CHECK(Commit(*sv, error));
                auto again = Load((work / n).string());
                bool ok = false;
                for (const Mon& x : again->Box(m.box))
                    if (x.slot == m.slot) { ok = x.species == c.evolvesTo; printf("%s: %s -> \"%s\"\n", n.c_str(), c.evolution.c_str(), x.nickname.c_str()); }
                if (!ok) printf("%s: %s -> NOT FOUND after reading back (box %d slot %d)\n", n.c_str(), c.evolution.c_str(), m.box + 1, m.slot + 1);
                CHECK(ok);
                done = true;
                break;
            }
    }

    // 5. A ROM hack's save (Ultra Violet) is read-only.
    {
        std::string e;
        auto hack = Save::Open((work / "Emerald.sav").string(), e, true);
        const Mon m = hack->Box(0).at(0);
        CHECK(CanMove(*hack, m.box, m.slot, *saves["Diamond.sav"]).verdict == Verdict::ReadOnly);
    }

    fprintf(stderr, failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
