// Fulldex: see utils/pksmcore_data.hpp.
#include "utils/pksmcore_data.hpp"
#include <cstdio>

namespace pksmcore_data
{
    bool read(const std::string& path, std::string& out)
    {
        if (const std::string* b = builtIn(path))
        {
            out = *b;
            return true;
        }
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return false;
        out.clear();
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
        const bool ok = !ferror(f);
        fclose(f);
        return ok;
    }

    bool exists(const std::string& path)
    {
        if (builtIn(path)) return true;
        FILE* f = fopen(path.c_str(), "rb");
        if (f) fclose(f);
        return f != nullptr;
    }
}
