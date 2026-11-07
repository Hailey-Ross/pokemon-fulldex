# PKSM-Core in Fulldex

PKSM-Core by FlagBrew (https://github.com/FlagBrew/PKSM-Core), GPL-3.0 (LICENSE here), much of it
translated from PKHeX by kwsch; memecrypto by SciresM; pcg-cpp by Melissa O'Neill (Apache-2.0/MIT).
Used by the Pokémon hub (frontend/pokemon_hub.cpp) to read and write the games' saves.

Taken from:
- PKSM-Core d9bca6fcdf622760a0aa963565b102276e2ae2a3
- memecrypto bd3b16ee2c12bd9ee353cf3098cb998e7880638c
- pcg-cpp 428802d1a5634f96bcd0705fab379ff0113bcf13 (include/ only)

Left out: the strings' subregions folders (not used by the hub).

Changed, so that its data files are built into the program instead of read from a folder (no
folder to ship to the Switch, Android or iPhone), and so that it builds without POSIX getline:
- include/utils/pksmcore_data.hpp, source/utils/pksmcore_data.cpp: new; the built-in copy comes
  from tools/pksm/embed_data.py at build time.
- source/personal/personal.cpp: readPersonal() reads through pksmcore_data.
- source/i18n/i18n.cpp, source/i18n/i18n_internal.hpp: load() reads through pksmcore_data.
