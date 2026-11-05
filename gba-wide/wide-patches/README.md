# Widescreen patches for the GBA games

Patches that let Pokémon FireRed, LeafGreen (1.0 and 1.1) and Emerald draw a wider picture on an mGBA
build with `gba.wideExtra`. They follow the game-facing protocol of Pokemon GBA Widescreen by mamesj
(shipped in Pokemon Fulldex). We wrote the code ourselves from the pret decompilations' function names
and structs. Patches are only ever applied to a copy of the player's own ROM; nothing here contains game
data.

## How it works
- The emulator offers wide by answering `0x57nn` at `0x0400005C` (`nn` = extra pixels per side; the
  game uses at most 112).
- On the overworld the game switches BG1–3 to 64×32 tile maps, draws 8 extra metatiles to the left and 9
  to the right, reads east/west neighbours past the map border, spawns and keeps objects across the
  wider view, and spreads weather sprites over it. In battle it only asks for wide layers.
- Each frame it asks for wide by writing `0xA5xx` to `0x0400005E` (low bits: BG0–3, sprites, text-box
  stretch).
- Its own state lives in free EWRAM at `0x0203FF00` (`struct WideState`).
- `src/` holds replacements for game functions: `Wide_X` replaces game function `X`. `tools/mkips.py`
  writes an 8-byte jump at `X` and puts the code at `0x08F00000`.
- `versions/<v>.ld` lists the game addresses for each version. `versions/<v>.check` lists CRC32s of
  the game code that the patch replaces or calls. A ROM that differs (a hack, or another version) is
  refused.

## Build
Needs `arm-none-eabi-gcc` and Python 3, plus checkouts of pret/pokefirered and pret/pokeemerald (headers
only).

    sh build.sh <pokefirered> <pokeemerald> <out>
    python3 tools/applyips.py <rom copy> <out>/firered.ips <patched rom> versions/firered.check

## Credits and licence
Pokemon GBA Widescreen by mamesj (in Pokemon Fulldex, GPL-3.0-or-later) for the idea and the protocol;
the pret decompilation projects for names and structs; mGBA. This folder is GPL-3.0-or-later.
