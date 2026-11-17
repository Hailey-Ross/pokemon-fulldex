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

    // The documented binary VDF types (0x03 float, 0x04 pointer, 0x05 wstring, 0x06 color,
    // 0x07 uint64) survive a round trip, and 0x0B ends the file.
    Vdf::Node typed;
    typed.kind = Vdf::Node::Object;
    typed.key = "types";
    Vdf::Node f; f.kind = Vdf::Node::Float; f.key = "f"; f.number = 0x40490FDB; typed.children.push_back(f);
    Vdf::Node p; p.kind = Vdf::Node::Pointer; p.key = "p"; p.number = 0x12345678; typed.children.push_back(p);
    Vdf::Node w; w.kind = Vdf::Node::WString; w.key = "w"; w.text = std::string("A\0B\0\0\0", 6); typed.children.push_back(w);
    Vdf::Node c; c.kind = Vdf::Node::Color; c.key = "c"; c.number = 0xAABBCCDD; typed.children.push_back(c);
    Vdf::Node i; i.kind = Vdf::Node::Int64; i.key = "i"; i.number = 0x1122334455667788LL; typed.children.push_back(i);
    const std::vector<uint8_t> typedBytes = Vdf::Write(typed);
    Vdf::Node typedBack;
    CHECK(Vdf::Parse(typedBytes, typedBack));
    CHECK(Vdf::Write(typedBack) == typedBytes);
    CHECK(typedBack.children.size() == 5);
    CHECK(typedBack.children[0].kind == Vdf::Node::Float && typedBack.children[0].number == 0x40490FDB);
    CHECK(typedBack.children[1].kind == Vdf::Node::Pointer && typedBack.children[1].number == 0x12345678);
    CHECK(typedBack.children[2].kind == Vdf::Node::WString && typedBack.children[2].text == std::string("A\0B\0\0\0", 6));
    CHECK(typedBack.children[3].kind == Vdf::Node::Color && typedBack.children[3].number == 0xAABBCCDD);
    CHECK(typedBack.children[4].kind == Vdf::Node::Int64 && typedBack.children[4].number == 0x1122334455667788LL);
    std::vector<uint8_t> typedEof = typedBytes;
    typedEof.push_back(0x0B);
    Vdf::Node eofBack;
    CHECK(Vdf::Parse(typedEof, eofBack));

    // Invalid data is refused, never half-parsed.
    std::vector<uint8_t> bad = bytes;
    bad.pop_back();
    Vdf::Node ignored;
    CHECK(!Vdf::Parse(bad, ignored));
    CHECK(!Vdf::Parse({}, ignored));

    fprintf(stderr, failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
