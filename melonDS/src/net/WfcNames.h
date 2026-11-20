#pragma once
// The names Nintendo WFC used (and the games still ask for): the subscription server (nas),
// the DWC and game-stats parts, and the connection test. When a community server stands in for
// Nintendo's closed ones, these names resolve to it (Net_Slirp.cpp); every other name resolves
// as before. On its own here so the rule can be tested without the emulator.
#include <cstring>

namespace melonDS::Wfc
{
inline bool IsName(const char* name)
{
    if (!name || !*name) return false;
    static const char* const suffixes[] = { "nintendowifi.net", "nintendowifi.com" };
    const size_t m = strlen(name);
    for (const char* suffix : suffixes)
    {
        const size_t n = strlen(suffix);
        if (m < n) continue;
        const char* tail = name + (m - n);
        bool same = true;
        for (size_t i = 0; i < n && same; i++)
        {
            char a = tail[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            same = a == suffix[i];
        }
        // (A whole label: "nas.nintendowifi.net", not "evilnintendowifi.net".)
        if (same && (m == n || tail[-1] == '.')) return true;
    }
    return false;
}
}
