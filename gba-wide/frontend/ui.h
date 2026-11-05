#pragma once
// Not compiled: Fulldex's tools/localisation/prepare_gba.py rewrites the parts below. Our menus use Fulldex's own ui.h.
#include <cstdint>
#include <string>
#include <vector>

namespace GbawLocalise {
template<class> struct Unused {
    static int TextWidth(const std::string& s, int scale) { return (int)s.size() * 6 * scale; }
    void Text(int, int, const std::string&, int, uint32_t) {}
    void TextCentred(int, int, const std::string&, int, uint32_t) {}
};

inline std::vector<std::string> Wrap(const std::string& text, int) { return { text }; }

// The colours of the menus.
}
