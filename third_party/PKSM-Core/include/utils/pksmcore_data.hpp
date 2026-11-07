#pragma once
// Fulldex: PKSM-Core's data files (personals, strings) come built into the program
// (pksmcore_data.cpp, made by tools/pksm/embed_data.py), so no folder of them has to be shipped
// to the Switch, Android or iPhone. A path not built in is read from disk as before.
#include <string>

namespace pksmcore_data
{
    // The file's bytes, from the built-in copy or else from disk. False: neither has it.
    bool read(const std::string& path, std::string& out);
    bool exists(const std::string& path);
    // (Generated:) the built-in copy of a path, or null.
    const std::string* builtIn(const std::string& path);
}
