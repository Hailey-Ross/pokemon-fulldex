# GBA games with widescreen

This folder lets a Pokemon Fulldex build list and play Game Boy Advance games. FireRed, LeafGreen
(1.0 and 1.1) and Emerald get a wider picture. Build Fulldex with `-DSSR_PC_ALL_GAMES=ON
-DSSR_GBA_DIR=<this folder>`.

- `patches/`: our change to mGBA (`mgba-wide.patch`, for the commit in `mgba.commit`). It adds a
  `gba.wideExtra` setting, a renderer that draws up to 240 + 2×128 pixels per line, and the registers
  the games use to ask for wide layers.
- `wide-patches/`: the game patches, in C. `prebuilt/` holds the built IPS files the program embeds;
  rebuild them with `wide-patches/build.sh` (needs arm-none-eabi-gcc and the pret decompilations'
  headers).
- `frontend/`: what Fulldex calls (`embed.h`): `Find` for its game list and `Play`, our player
  (SDL2). A patch goes on a copy of the ROM in memory, and only when the ROM's code matches the check
  list. Otherwise the game plays at 3:2.
- On its own (`cmake -S gba`, no `GBAW_EMBEDDED`): `gbawide.exe`, the same player for launchers:
  `gbawide [--rom] <file.gba> [--save <file.sav>] [--saves <folder>] [--data <folder>] [--fullscreen]
  [--size WxH]`. On Windows it is one static program. RetroAchievements are only there inside Fulldex.
  Hacks' saves are named as Fulldex names them (the file's name, lower case, `-` between words).
- CMake: target `gbawide`. mGBA is fetched at `mgba.commit` and patched, or taken from
  `-DGBAW_MGBA_DIR=<a patched checkout>`.

Settings live in `gba-settings.ini` in Fulldex's data folder:
- `widescreen`: `auto` (the window's shape), `off`, or a ratio such as `16:9`.
- `scaling`: `sharp`, `smooth` or `whole`.
- Also `volume`, `show_fps`, `buttons` (`labels` or `positions`) and `fast_forward` (2–8, hold Tab or
  the right trigger).

Keys: arrows, X = A, Z = B, A = L, S = R, Enter = Start, Backspace = Select. F11 switches full screen.
Esc (a controller's Guide, or Start and Select together) opens the pause menu: widescreen, scaling,
volume, FPS counter, fast-forward speed, the RetroAchievements list, reset, and quit to the list.
It looks like the DS games' menu (Fulldex's `frontend/ui.h`).

Credits: Pokemon GBA Widescreen by mamesj (the idea and the protocol, in Pokemon Fulldex), mGBA by
endrift and contributors (MPL-2.0; `patches/mgba-wide.patch` is MPL-2.0), the pret decompilations
(names and structs). Everything else here is GPL-3.0-or-later.
