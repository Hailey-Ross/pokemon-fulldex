#pragma once
// Binary KeyValues (Valve's VDF), the format of Steam's shortcuts.vdf: objects, strings and
// integers. Enough to read the player's shortcuts, add ours, and write them back unchanged.
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Vdf
{
struct Node
{
    // (Valve's binary VDF types: 0x00 object, 0x01 string, 0x02 int32, 0x03 float,
    // 0x04 pointer, 0x05 wstring, 0x06 color, 0x07 uint64; 0x08 ends an object and
    // 0x0B ends the file.)
    enum Kind { Object, String, Int32, Float, Pointer, WString, Color, Int64 };
    Kind kind = Object;
    std::string key;
    std::string text;
    int64_t number = 0;
    std::vector<Node> children;
    // (Where the node lives in the file it was read from: the type byte to just past its value.
    // Additions and removals splice these bytes rather than re-encoding the player's shortcuts.)
    size_t begin = 0;
    size_t end = 0;
};

inline void PutCstr(std::vector<uint8_t>& out, const std::string& s)
{
    out.insert(out.end(), s.begin(), s.end());
    out.push_back(0);
}
inline void PutInt(std::vector<uint8_t>& out, uint64_t v, int bytes)
{
    for (int i = 0; i < bytes; i++) out.push_back((uint8_t)(v >> (8 * i)));
}

inline void Write(const Node& n, std::vector<uint8_t>& out)
{
    const uint8_t type = n.kind == Node::Object ? 0x00 : n.kind == Node::String ? 0x01 : n.kind == Node::Int32 ? 0x02
                       : n.kind == Node::Float ? 0x03 : n.kind == Node::Pointer ? 0x04 : n.kind == Node::WString ? 0x05
                       : n.kind == Node::Color ? 0x06 : 0x07;
    out.push_back(type);
    PutCstr(out, n.key);
    if (n.kind == Node::Object)
    {
        for (const Node& c : n.children) Write(c, out);
        out.push_back(0x08);
    }
    else if (n.kind == Node::String) PutCstr(out, n.text);
    else if (n.kind == Node::WString) out.insert(out.end(), n.text.begin(), n.text.end()); // (raw UTF-16, with its terminator)
    else if (n.kind == Node::Int64) PutInt(out, (uint64_t)n.number, 8);
    else PutInt(out, (uint32_t)n.number, 4);
}
inline std::vector<uint8_t> Write(const Node& root)
{
    std::vector<uint8_t> out;
    Write(root, out);
    return out;
}

inline bool ReadCstr(const std::vector<uint8_t>& d, size_t& at, std::string& s)
{
    size_t end = at;
    while (end < d.size() && d[end]) end++;
    if (end >= d.size()) return false;
    s.assign((const char*)d.data() + at, end - at);
    at = end + 1;
    return true;
}

inline bool ReadNode(const std::vector<uint8_t>& d, size_t& at, Node& n)
{
    if (at >= d.size()) return false;
    n = Node();
    n.begin = at;
    const uint8_t type = d[at++];
    if (type == 0x08 || type == 0x0B) return false; // (the end belongs to the parent / the file)
    if (type == 0x00) n.kind = Node::Object;
    else if (type == 0x01) n.kind = Node::String;
    else if (type == 0x02) n.kind = Node::Int32;
    else if (type == 0x03) n.kind = Node::Float;
    else if (type == 0x04) n.kind = Node::Pointer;
    else if (type == 0x05) n.kind = Node::WString;
    else if (type == 0x06) n.kind = Node::Color;
    else if (type == 0x07) n.kind = Node::Int64;
    else return false; // (a type the format does not document: fail rather than corrupt)
    if (!ReadCstr(d, at, n.key)) return false;
    if (n.kind == Node::Object)
    {
        for (;;)
        {
            if (at >= d.size()) return false;
            if (d[at] == 0x08) { at++; n.end = at; return true; }
            Node child;
            if (!ReadNode(d, at, child)) return false;
            n.children.push_back(std::move(child));
        }
    }
    if (n.kind == Node::String)
    {
        if (!ReadCstr(d, at, n.text)) return false;
        n.end = at;
        return true;
    }
    if (n.kind == Node::WString)
    {
        // (Null-terminated UTF-16: the raw bytes, including the two-byte terminator.)
        const size_t start = at;
        while (at + 1 < d.size() && (d[at] || d[at + 1])) at += 2;
        if (at + 1 >= d.size()) return false;
        at += 2;
        n.text.assign((const char*)d.data() + start, at - start);
        n.end = at;
        return true;
    }
    const int bytes = n.kind == Node::Int64 ? 8 : 4;
    if (at + bytes > d.size()) return false;
    uint64_t v = 0;
    for (int i = 0; i < bytes; i++) v |= (uint64_t)d[at + i] << (8 * i);
    at += bytes;
    // (Steam writes non-Steam appids as signed int32s; keep the sign. Float/pointer/color are
    // 4 raw bytes, so keep their bits unsigned.)
    n.number = bytes == 4 ? (n.kind == Node::Int32 ? (int64_t)(int32_t)(uint32_t)v : (int64_t)(uint32_t)v) : (int64_t)v;
    n.end = at;
    return true;
}

inline bool Parse(const std::vector<uint8_t>& d, Node& root)
{
    size_t at = 0;
    if (!ReadNode(d, at, root)) return false;
    if (root.kind != Node::Object) return false;
    // (Steam's shortcuts.vdf ends with the root's end byte and, in practice, one more: accept
    // trailing end/padding bytes and the documented end-of-file marker, and nothing else.)
    while (at < d.size() && (d[at] == 0x08 || d[at] == 0x00 || d[at] == 0x0B)) at++;
    return at == d.size();
}

inline Node* Child(Node& n, const std::string& key)
{
    for (Node& c : n.children)
        if (c.key == key) return &c;
    return nullptr;
}
inline const Node* Child(const Node& n, const std::string& key)
{
    for (const Node& c : n.children)
        if (c.key == key) return &c;
    return nullptr;
}
inline std::string String(const Node& n, const std::string& key, const std::string& fallback = {})
{
    const Node* c = Child(n, key);
    return c && c->kind == Node::String ? c->text : fallback;
}
inline int64_t Number(const Node& n, const std::string& key, int64_t fallback = 0)
{
    const Node* c = Child(n, key);
    return c && (c->kind == Node::Int32 || c->kind == Node::Int64) ? c->number : fallback;
}
inline void SetString(Node& n, const std::string& key, const std::string& value)
{
    if (Node* c = Child(n, key)) { c->kind = Node::String; c->text = value; c->number = 0; c->children.clear(); }
    else { Node added; added.kind = Node::String; added.key = key; added.text = value; n.children.push_back(std::move(added)); }
}
inline void SetInt32(Node& n, const std::string& key, int32_t value)
{
    if (Node* c = Child(n, key)) { c->kind = Node::Int32; c->number = value; c->text.clear(); c->children.clear(); }
    else { Node added; added.kind = Node::Int32; added.key = key; added.number = value; n.children.push_back(std::move(added)); }
}
inline void SetObject(Node& n, const std::string& key)
{
    if (!Child(n, key)) { Node c; c.kind = Node::Object; c.key = key; n.children.push_back(std::move(c)); }
}
}
