#pragma once
// The launcher's words in the player's language (LANGUAGE in the settings, UiLang): looked up by
// their English, case aside. The tables are built in (launcher_lang_data.h, from lang/launcher-xx.txt:
// the English, a tab, the translation); launcher-xx.txt with the saves adds to or corrects one.
// {} in a line stands for a name or number, itself looked up too: "Move To {}" with "Box 3".
// Anything not in the table is shown as it is, in English.
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>
#include "ui_lang.h"
#include "launcher_lang_data.h"

namespace LauncherText
{
struct Pattern { std::vector<std::string> parts; std::vector<bool> number; std::string out; size_t fixed = 0; }; // (number: that {} is {#}, digits only)
struct Table
{
    std::unordered_map<std::string, std::string> exact; // (by the English made lower case)
    std::vector<Pattern> patterns;                      // (longest first)
    std::unordered_map<std::string, std::string> memo;  // (asked before)
};
inline std::string& OverrideDir() { static std::string d; return d; } // (the saves: launcher.h sets it)
inline std::string Lower(const std::string& s) { std::string o = s; for (char& ch : o) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a'); return o; }
inline void Add(Table& t, const std::string& text)
{
    for (size_t at = 0; at < text.size();)
    {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string l = text.substr(at, end - at);
        at = end + 1;
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
        const size_t tb = l.find('\t');
        if (l.empty() || l[0] == '#' || tb == std::string::npos || tb == 0 || tb + 1 >= l.size()) continue;
        const std::string en = Lower(l.substr(0, tb)), out = l.substr(tb + 1);
        if (en.find("{}") == std::string::npos && en.find("{#}") == std::string::npos) { t.exact[en] = out; continue; }
        Pattern p;
        p.out = out;
        for (size_t k = 0;;)
        {
            const size_t h1 = en.find("{}", k), h2 = en.find("{#}", k), h = std::min(h1, h2);
            p.parts.push_back(en.substr(k, h == std::string::npos ? std::string::npos : h - k));
            if (h == std::string::npos) break;
            p.number.push_back(h == h2);
            k = h + (h == h2 ? 3 : 2);
        }
        for (size_t h; (h = p.out.find("{#}")) != std::string::npos;) p.out.replace(h, 3, "{}");
        for (const std::string& part : p.parts) p.fixed += part.size();
        t.patterns.push_back(p);
    }
}
inline Table& For(int lang)
{
    static Table tables[8];
    static bool made[8] = {};
    if (lang < 0 || lang >= 8) lang = 0;
    Table& t = tables[lang];
    if (!made[lang])
    {
        made[lang] = true;
        const char* code = UiLang::Codes[lang < 7 ? lang : 0];
        if (const char* builtIn = LauncherLangData(code)) Add(t, builtIn);
        if (!OverrideDir().empty())
            if (FILE* f = fopen((OverrideDir() + "/launcher-" + code + ".txt").c_str(), "rb"))
            {
                std::string all;
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof buf, f)) > 0) all.append(buf, n);
                fclose(f);
                Add(t, all);
            }
        // (The patterns with most words of their own first: "Move To {}" before "To {}".)
        std::stable_sort(t.patterns.begin(), t.patterns.end(), [](const Pattern& a, const Pattern& b) { return a.fixed > b.fixed; });
    }
    return t;
}
inline std::string Look(Table& t, const std::string& s, int depth);
inline bool Match(const Pattern& p, const std::string& low, const std::string& s, std::vector<std::string>& caps)
{
    caps.clear();
    if (low.size() <= p.fixed || low.compare(0, p.parts[0].size(), p.parts[0]) != 0) return false;
    size_t at = p.parts[0].size();
    for (size_t i = 1; i < p.parts.size(); i++)
    {
        const std::string& part = p.parts[i];
        size_t found;
        if (i + 1 == p.parts.size())
        {
            if (low.size() < at + 1 + part.size()) return false;
            found = low.size() - part.size();
            if (low.compare(found, part.size(), part) != 0) return false;
        }
        else
        {
            found = part.empty() ? std::string::npos : low.find(part, at + 1);
            if (found == std::string::npos) return false;
        }
        if (found <= at) return false; // (each {} at least one letter)
        const std::string cap = s.substr(at, found - at);
        if (p.number[i - 1] && cap.find_first_not_of("0123456789.,") != std::string::npos) return false;
        caps.push_back(cap);
        at = found + part.size();
    }
    return true;
}
inline std::string Look(Table& t, const std::string& s, int depth)
{
    if (s.empty()) return s;
    const std::string low = Lower(s);
    const auto e = t.exact.find(low);
    if (e != t.exact.end()) return e->second;
    if (depth > 2) return s;
    std::vector<std::string> caps;
    for (const Pattern& p : t.patterns)
        if (Match(p, low, s, caps))
        {
            std::string out;
            size_t k = 0, c = 0;
            for (size_t h; (h = p.out.find("{}", k)) != std::string::npos; k = h + 2)
            {
                out += p.out.substr(k, h - k);
                if (c < caps.size()) out += Look(t, caps[c++], depth + 1);
            }
            return out + p.out.substr(k);
        }
    // (Words joined with gaps or dots, "Continue Playing   ·   GBA", "Custom  40%": each looked up on its own.)
    if (depth == 0)
    {
        std::string out, piece;
        bool changed = false, split = false;
        auto flush = [&]() { if (piece.empty()) return; const std::string tr = Look(t, piece, 1); changed |= tr != piece; out += tr; piece.clear(); };
        for (size_t i = 0; i < s.size();)
        {
            const bool dot = s.compare(i, 2, "\xC2\xB7") == 0;
            if ((s[i] == ' ' && i + 1 < s.size() && s[i + 1] == ' ') || dot)
            {
                // (the gap, as it is: spaces and a middle dot)
                while (!piece.empty() && piece.back() == ' ') { piece.pop_back(); }
                flush();
                size_t j = i;
                while (j < s.size() && (s[j] == ' ' || s.compare(j, 2, "\xC2\xB7") == 0)) j += s[j] == ' ' ? 1 : 2;
                if (!out.empty() && out.back() != ' ' && s[i] == ' ' && i > 0 && s[i - 1] != ' ') {}
                out += s.substr(i, j - i);
                i = j;
                split = true;
                continue;
            }
            piece += s[i++];
        }
        flush();
        if (split && changed) return out;
    }
    return s;
}
// The words as shown: in English, or in the language chosen.
inline std::string Tr(const std::string& s)
{
    const int lang = UiLang::lang;
    if (lang == 0 || s.empty()) return s;
    Table& t = For(lang);
    if (t.exact.empty() && t.patterns.empty()) return s;
    const auto m = t.memo.find(s);
    if (m != t.memo.end()) return m->second;
    if (t.memo.size() > 4096) t.memo.clear();
    return t.memo[s] = Look(t, s, 0);
}
}
