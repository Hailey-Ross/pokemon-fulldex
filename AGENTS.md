# AGENTS.md

Instructions for AI agents and contributors working on **Pokémon Fulldex**
(`mamesj/pokemon-fulldex`). Read this before building or changing anything. The fuller
human guide is [`docs/DEVELOPING.md`](docs/DEVELOPING.md); the player-facing guide is
[`README.md`](README.md).

## What this repository is

A C++17/SDL2 front end: melonDS (DS games) and mGBA (GBA games, under `gba/`) with a
widescreen renderer, a launcher (`frontend/pc_launcher.cpp`), and Android, iOS, Switch,
macOS, Windows and Linux builds. The player supplies their own ROMs; **no ROMs, saves,
keys or Nintendo artwork are committed**.

Two build paths exist and must not be confused:

- `setup.sh` / `tools/switch/build.sh` — the original ssrecomp path: fetch the
  pokeheartgold decompilation, build one game from the player's ROM, generate recompiled
  C++ and compile it. The result contains game code and stays local.
- `tools/pc/build_all.sh`, `tools/android/build_all.sh`, `tools/mac/build_all.sh`,
  `tools/ios/build_all.sh` — the all-games programs. The SoulSilver ROM is read only for
  table addresses; the downloads contain no game code. These are what releases ship.

## Hard rules

- **Never commit** ROMs, saves, save states, the Google sign-in client
  (`fulldex-google-client.txt`, `~/.config/pokemon-ds-wide/fulldex-google-client.txt`),
  signing keys, or Nintendo artwork (`gba/art/`). `.gitignore` covers these; run
  `git status --short` before every commit.
- **Never put game data or generated game code in the repo or CI.** CI fixtures are
  synthetic (`tests/native_smoke.py`); no workflow is given a ROM.
- **Never print, copy or commit credentials.** Refer to credential files by path only.
- **Work on a branch; never commit to `main`.** Push to your fork and open a PR.
- **Do not run two `build_all.sh` builds at once.** They share the `build-decomp-*` Git
  checkouts and will fail on `index.lock` or corrupt each other.
- **Do not modify the user's data folders** (`~/.local/share/pokemon-ds-wide`,
  `~/.config/pokemon-ds-wide`) during a build or test unless the task is explicitly about
  them; test with copied or temporary data.

## Build and test quick reference

All commands run from the repository root.

```bash
# Python/C++ unit tests (what CI runs on every push and PR)
python3 -I -m unittest discover -s tests -p 'test_*.py'

# ROM-free build smoke: generator, embedded-data link, native frontend
python3 tests/native_smoke.py --build-dir /tmp/ssrecomp-smoke

# Generated translation headers must match localisation/*.json
python3 tools/localisation/generate_translations.py --check
python3 tools/localisation/generate_guide_translations.py --check

# PC all-games (one of windows | linux | linux-arm64 | both)
tools/pc/build_all.sh linux "<path-to-soulsilver-us.nds>"

# Android (SDK with build-tools 34.0.0, platform android-34, NDK 27.2.12479018, JDK 17, Pillow)
ANDROID_SDK=/opt/android-sdk tools/android/build_all.sh "<path-to-soulsilver-us.nds>"

# Single-game native executable
./setup.sh --deps --rom "<path-to-your-rom.nds>"
```

Optional PC flags (see `tools/pc/build_all.sh`): `SSR_PC_ART`, `SSR_PC_ICON`,
`SSR_LINUX_ICON`, `SSR_GBA_DIR`, `GBAW_WORK`, `SSR_POKEMON_HUB`,
`FULLDEX_RELAY_URL`, `FULLDEX_GOOGLE_CLIENT_FILE`, `SSR_3DS_LINUX_KIT`,
`SSR_3DS_WIN_DIR`.

On SteamOS or another immutable system, and on hosts without the build packages, build in
an Ubuntu 24.04 container; `docs/DEVELOPING.md` has the exact recipe. Never install system
packages on the host for a one-off build.

## Verification before you claim a task is done

Run, and report honestly:

1. The tests that cover what you touched (`python3 -I -m unittest discover -s tests -p
   'test_*.py'`, or the specific `test_*.py`).
2. Both translation generators with `--check` if you touched `localisation/` or a
   generated header.
3. `tests/native_smoke.py` if you touched CMake, `runtime/`, `melonDS/`, or the build
   scripts.
4. The actual build target if you changed build scripts, packaging, or a platform layer.
5. `git diff --check` and `git status --short`.

If something could not be run (no ROM, no device, no SDK), say so explicitly instead of
implying it passed.

## Where things are

| Path | What it is |
| --- | --- |
| `frontend/` | C++17/SDL2 front end, launcher (`pc_launcher.cpp`), menus, overlays, companion pages |
| `frontend/ui_translations.h`, `frontend/guide_translations.h` | Generated from `localisation/*.json`; never edit by hand |
| `melonDS/` | DS emulator core |
| `gba/` | GBA player: mGBA with `patches/mgba-wide.patch`, the frontend, wide patches |
| `runtime/` | Recompilation runtime for the single-game path (`recomp_*`) |
| `gen/` | Generated single-game tables and recompiled code (git-ignored) |
| `layouts/` | ROM layouts used by `tools/recomp.py` |
| `localisation/` | Translation catalogues and their README |
| `tools/pc/`, `tools/android/`, `tools/apple/`, `tools/mac/`, `tools/ios/`, `tools/switch/` | Platform builds and packaging |
| `tools/soulsilver/`, `tools/bw1/`, `tools/bw2/`, `tools/platinum/`, `tools/regions/` | Table and per-game address generation |
| `tools/localisation/` | Translation and font generators |
| `tools/updater/` | Release checksums and binary build IDs |
| `tests/` | Python/C++/Android tests and the ROM-free smoke fixture |
| `docs/howto/` | Player-facing install guides |
| `build-*/` | Build output (git-ignored) |

## Style

- C++17, SDL2. Small, direct changes; comments in parentheses explain *why*, not what.
- No unnecessary abstraction; match the surrounding code.
- The launcher has two input paths that must stay in sync: pointer/touch hit-tests the
  target and focuses/activates it; keyboard/gamepad move focus and activate with A/Enter.
  See `choose()` and the `FINGER`/`MOUSE` handlers in `frontend/main.cpp` and
  `frontend/pc_launcher.cpp`.
- Steam shortcuts (`frontend/steam_shortcuts.h`, `frontend/vdf.h`) must preserve the
  user's `shortcuts.vdf` byte for byte apart from the edited entry. Never re-encode the
  whole file.
- Commit subjects are short imperative sentences that say what changed and why, e.g.
  "Guide: the remaining gen 4 HAVE item ids, with a test". Keep commits focused; split
  unrelated work. Add a `CHANGELOG.md` line for player-visible changes.

## Working with an agent

- Read the relevant files before editing; do not guess at APIs.
- Make the smallest change that fixes the problem, then verify it as above.
- If a task needs a ROM, a device, or credentials, stop and ask instead of inventing a
  substitute or committing anything sensitive.
- Prefer a separate `git worktree` for long-running side work so you do not disturb a
  build running from the main checkout.
- When you change generated files, change the generator or source JSON and regenerate;
  never edit generated headers directly.

## Environment notes

- The repository may be on a root-owned mount; use `git -c safe.directory=$PWD ...`.
- In rootless Podman the container's root maps to your user, so build output stays owned
  by you; `HOME` inside the container is `/root`.
- The PC build downloads SDL2 for Windows once into `build-pc-deps/`; the Linux AppImage
  needs `zsyncmake` and Python Pillow (`python3-pil`).
- SteamOS updates may disable `sshd`; re-enable it in Desktop Mode if SSH stops.

## References

- `README.md` — player-facing features and installs.
- `docs/DEVELOPING.md` — layouts, build recipes, architecture, release packaging.
- `localisation/README.md` — the translation catalogue and font pipeline.
- `gba/README.md` — the GBA player and its licensing.
- `tools/pc/README.txt` — the PC/Steam Deck download instructions.
- `CHANGELOG.md` — release history.
- `.github/ISSUE_TEMPLATE/` — bug and feature forms.
