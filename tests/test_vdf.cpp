// The binary VDF reader/writer (frontend/vdf.h) used for Steam's shortcuts.vdf. Standard C++ only.
#include "vdf.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main()
{
    // A fixture shaped like Steam's shortcuts.vdf.
    Vdf::Node root;
    root.kind = Vdf::Node::Object;
    root.key = "shortcuts";
    Vdf::Node entry;
    entry.kind = Vdf::Node::Object;
    entry.key = "0";
    Vdf::SetInt32(entry, "appid", -1102597091);
    Vdf::SetString(entry, "AppName", "Pokémon Fulldex");
    Vdf::SetString(entry, "Exe", "\"/home/deck/AppImages/pokémon_fulldex.appimage\"");
    Vdf::SetString(entry, "LaunchOptions", "--game IPGE");
    Vdf::SetInt32(entry, "IsHidden", 0);
    Vdf::SetObject(entry, "tags");
    root.children.push_back(entry);
    Vdf::SetString(root, "1", "unused"); // (a non-object child is preserved too)

    const std::vector<uint8_t> bytes = Vdf::Write(root);
    Vdf::Node back;
    CHECK(Vdf::Parse(bytes, back));
    const std::vector<uint8_t> again = Vdf::Write(back);
    CHECK(again == bytes);

    CHECK(back.key == "shortcuts");
    CHECK(back.children.size() == 2);
    const Vdf::Node& e = back.children[0];
    CHECK(e.key == "0" && e.kind == Vdf::Node::Object);
    CHECK(Vdf::Number(e, "appid") == -1102597091);
    CHECK(Vdf::String(e, "AppName") == "Pokémon Fulldex");
    CHECK(Vdf::String(e, "LaunchOptions") == "--game IPGE");
    CHECK(Vdf::String(e, "Exe") == "\"/home/deck/AppImages/pokémon_fulldex.appimage\"");
    CHECK(Vdf::Child(e, "tags") != nullptr && Vdf::Child(e, "tags")->children.empty());
    CHECK(Vdf::Number(e, "missing", 7) == 7);

    // Set replaces in place; adding appends.
    Vdf::SetString(entry, "AppName", "Renamed");
    CHECK(Vdf::String(entry, "AppName") == "Renamed");
    CHECK(entry.children.size() == 6);

    // Invalid data is refused, never half-parsed.
    std::vector<uint8_t> bad = bytes;
    bad.pop_back();
    Vdf::Node ignored;
    CHECK(!Vdf::Parse(bad, ignored));
    CHECK(!Vdf::Parse({}, ignored));

    fprintf(stderr, failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
