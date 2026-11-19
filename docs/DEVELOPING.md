# Developing Pokémon Fulldex

This is the developer guide for building, testing and packaging Fulldex. For the player-facing
guide see [`README.md`](../README.md); for AI-agent-specific rules see
[`AGENTS.md`](../AGENTS.md). Player install guides live in [`howto/`](howto/).

## Layout

| Path | What it is |
| --- | --- |
| `frontend/` | C++17/SDL2 front end and the all-games launcher (`pc_launcher.cpp`) |
| `frontend/ui_translations.h`, `frontend/guide_translations.h` | Generated interface text; never edit by hand |
| `melonDS/` | DS emulator core |
| `gba/` | GBA player: mGBA + widescreen renderer + its frontend |
| `runtime/` | Recompilation runtime used by the single-game path |
| `gen/` | Generated single-game tables and recompiled code (git-ignored) |
| `layouts/` | ROM layouts used by `tools/recomp.py` |
| `localisation/` | Translation catalogues (`*.json`) and their README |
| `tools/pc/` | PC/Steam Deck build and packaging |
| `tools/android/` | Android build and packaging |
| `tools/apple/`, `tools/mac/`, `tools/ios/` | Apple helpers, Mac app, iOS build |
| `tools/switch/` | Switch packaging |
| `tools/soulsilver/`, `tools/bw1/`, `tools/bw2/`, `tools/platinum/`, `tools/regions/` | Per-game tables and addresses |
| `tools/localisation/` | Translation and font generators |
| `tools/updater/` | Release checksums and binary build IDs |
| `tests/` | Python/C++/Android tests and the ROM-free smoke fixture |
| `docs/` | This guide, feature notes, screenshots and player how-tos |
| `build-*/` | Build output (ignored) |

## Prerequisites

Common: Linux or WSL2 (Ubuntu 24.04), Git, CMake, Ninja, Python 3, curl, SDL2 development
headers, libpng, zlib.

- Windows target: MinGW-w64 (`g++-mingw-w64-x86-64-posix`)
- Linux AppImage: `zsync`, Python Pillow (`python3-pil`); `appimagetool` is fetched by the script
- Linux ARM64: `g++-aarch64-linux-gnu`, `patchelf`
- Android: Android SDK with `build-tools;34.0.0`, `platforms;android-34`,
  `ndk;27.2.12479018`, JDK 17, `python3-pil`
- GBA: the patched mGBA and wide patches (see below)

On immutable systems (SteamOS, Bazzite, Silverblue) build inside a Ubuntu 24.04 container or
distrobox rather than modifying the host.

### Building in a container

The release builds are made this way, and it also works on any host without the build
packages. From the repository root:

```bash
podman run --rm --security-opt label=disable \
  -v "$PWD:/src" \
  -v "$HOME/.local/share/pokemon-ds-wide/roms:/roms:ro" \
  -w /src ubuntu:24.04 bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq cmake ninja-build g++ pkg-config git curl python3 python3-pil file zip \
  g++-mingw-w64-x86-64-posix libsdl2-dev libpng-dev zlib1g-dev zsync \
  libx11-dev libxext-dev libxcursor-dev libxi-dev libxfixes-dev libxrandr-dev libxrender-dev libxss-dev \
  libwayland-dev libxkbcommon-dev libdecor-0-dev libegl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev \
  libasound2-dev libpulse-dev libudev-dev >/dev/null
tools/pc/build_all.sh both "/roms/<your-soulsilver-us.nds>"
'
```

Notes:

- Rootless Podman maps the container's root to your user, so `build-pc-*` output stays owned
  by you even though `HOME` inside the container is `/root`.
- `--security-opt label=disable` is needed on SELinux hosts (Fedora) for the bind mounts.
- Only run one `build_all.sh` at a time: concurrent runs share `build-decomp-*` Git
  checkouts and fail on `index.lock`.
- If the repository is on a root-owned or otherwise mismatched mount, add
  `git config --global --add safe.directory '*'` inside the container.

## Recompiled native executable (single game)

The root `setup.sh` is the original ssrecomp path: it fetches the pokeheartgold decompilation,
builds the game from your own ROM, generates the recompiled C++ and compiles a native
executable for that one ROM. It is **not** the all-games launcher, and its output contains
game code: keep it local.

```bash
./setup.sh                 # asks before installing packages
./setup.sh --deps          # install packages without asking (sudo)
./setup.sh --decomp DIR    # use an existing pokeheartgold checkout
./setup.sh --rom FILE      # also check a ROM against the build
./setup.sh --jobs N
./setup.sh --desktop       # add a menu entry
```

The multi-game builds below are a different path: they read the SoulSilver ROM only for table
addresses and ship no game code. `tools/recomp.py`, `runtime/recomp_*` and `layouts/` are the
generator, runtime and ROM layouts behind the single-game path.

## DS / PC build

```bash
tools/pc/build_all.sh linux        "<your SoulSilver US.nds>"
tools/pc/build_all.sh windows      "<your SoulSilver US.nds>"
tools/pc/build_all.sh linux-arm64  "<your SoulSilver US.nds>"
tools/pc/build_all.sh both         "<your SoulSilver US.nds>"
```

The ROM must be SoulSilver US (header game code `IPGE`). It is read only for table addresses;
the downloads contain no game data. Outputs land in `build-pc-dist/`:

- `pokemon-ds-wide-pc-windows-x86_64.zip`
- `pokemon-ds-wide-pc-linux-x86_64.tar.gz`
- `pokemon-ds-wide-pc-linux-x86_64.AppImage` (+ `.zsync`)
- `pokemon-ds-wide-pc-linux-aarch64.*`

The script also writes `build-gen-tables/` (SoulSilver and HeartGold's tables and the portable
function profiles) and `build-pc-data/funcs/` (one function table per game). The Mac and iOS
builds consume `build-gen-tables/`, so build the PC target first on a machine that has the ROM.

Optional environment variables:

- `SSR_PC_ART=<folder>` — game-list box art, `<code>.jpg` (IREO, IRBO, IPGE …)
- `SSR_PC_ICON=<file.ico>` — Windows icon
- `SSR_LINUX_ICON=<file>` — AppImage icon
- `SSR_GBA_DIR=<path>` — include the GBA games
- `GBAW_WORK=<path>` — the GBA work folder (default `$HOME/pokemon-gba-wide`)
- `SSR_POKEMON_HUB=ON` — the Hub's cloud saves and Pokémon moving
- `FULLDEX_RELAY_URL=<url>` — the theme Gallery relay
- `FULLDEX_GOOGLE_CLIENT_FILE=<file>` — the Google sign-in client, two lines; **never commit it**
- `SSR_3DS_LINUX_KIT=<folder>` (x86_64) and `SSR_3DS_WIN_DIR=<folder>` (Windows) — the Omega
  Ruby / Alpha Sapphire program placed in `3ds/program/`; the games' dumps are not part of it

### GBA games

The GBA player source is `gba/` (mGBA with `patches/mgba-wide.patch`, the player frontend, and
`wide-patches/`). `gba/README.md` states:

> The release builds point `SSR_GBA_DIR` at this folder.
> Not included: the games' box art (`art/*.jpg`, Nintendo's artwork).

Prepare the work folder:

```bash
SRC="$PWD/gba"
WORK="$HOME/pokemon-gba-wide"
git init -q "$WORK/mgba"
git -C "$WORK/mgba" remote add origin https://github.com/mgba-emu/mgba.git
git -C "$WORK/mgba" fetch -q --depth 1 origin "$(tr -d '[:space:]' < "$SRC/patches/mgba.commit")"
git -C "$WORK/mgba" checkout -q --force FETCH_HEAD
git -C "$WORK/mgba" apply "$SRC/patches/mgba-wide.patch"
mkdir -p "$WORK/build/patches"
cp "$SRC"/wide-patches/*.ips "$SRC"/wide-patches/*.safety "$WORK/build/patches"/
```

Then build with `SSR_GBA_DIR="$SRC"`. The CMake checks:

- `$HOME/pokemon-gba-wide/mgba/include/mgba/internal/gba/video.h`
- `$HOME/pokemon-gba-wide/build/patches/<game>-wide.ips`

Override with `GBAW_WORK`, `GBAW_MGBA`, `GBAW_PATCHES`.

### Container gotcha

CMake uses `$ENV{HOME}/pokemon-gba-wide`. If you mount the work folder elsewhere (e.g.
`/gba-work`), create it at the expected path inside the container, or pass `GBAW_WORK`,
`GBAW_MGBA` and `GBAW_PATCHES`. An empty `SSR_GBA_DIR` silently produces a DS-only build
(or APK).

## Full release build (PC)

This is the recipe used for the downloadable PC release. It builds Linux (tar.gz and
AppImage) and Windows, with the GBA games, the online features and the 3DS program:

```bash
podman run --rm --security-opt label=disable \
  -v "$PWD:/src" \
  -v "$HOME/.local/share/pokemon-ds-wide/roms:/roms:ro" \
  -v "$HOME/.config/pokemon-ds-wide/fulldex-google-client.txt:/src/fulldex-google-client.txt:ro" \
  -v "$HOME/pokemon-gba-wide:/root/pokemon-gba-wide" \
  -v "/path/to/3ds-kit:/root/3ds-kit:ro" \
  -e FULLDEX_GOOGLE_CLIENT_FILE=/src/fulldex-google-client.txt \
  -e SSR_POKEMON_HUB=ON \
  -e FULLDEX_RELAY_URL=<relay-url> \
  -e SSR_GBA_DIR=/src/gba \
  -e SSR_3DS_LINUX_KIT=/root/3ds-kit \
  -w /src ubuntu:24.04 bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq cmake ninja-build g++ pkg-config git curl python3 python3-pil file zip \
  g++-mingw-w64-x86-64-posix libsdl2-dev libpng-dev zlib1g-dev zsync \
  libx11-dev libxext-dev libxcursor-dev libxi-dev libxfixes-dev libxrandr-dev libxrender-dev libxss-dev \
  libwayland-dev libxkbcommon-dev libdecor-0-dev libegl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev \
  libasound2-dev libpulse-dev libudev-dev >/dev/null
git config --global --add safe.directory "*"
tools/pc/build_all.sh both "/roms/<your-soulsilver-us.nds>"
'
```

Do not commit the client file, the 3DS kit, or the artifacts. If the 3DS kit is not
available, leave `SSR_3DS_LINUX_KIT` out; the rest of the release is unaffected.

## Android build

```bash
ANDROID_SDK=/opt/android-sdk tools/android/build_all.sh "<your SoulSilver US.nds>"
```

Output: `build-android-dist/pokemon-ds-wide-android.apk`.

Optional:

- `SSR_ANDROID_ABIS="arm64-v8a"` — phone-only (default builds `arm64-v8a x86_64`)
- `SSR_ANDROID_KEY=<key.json>` — signing key (default
  `~/.config/pokemon-ds-wide/android-signing-key.json`); keep it, Android only installs
  updates signed with the same key
- `SSR_ANDROID_ICON=<png>` — launcher icon
- `SSR_PC_ART=<folder>` — game-list art
- `SSR_GBA_DIR=<path>` — include GBA

The first start asks for the ROM folder (uses Android's file access); saves live in a `saves`
folder inside it.

## Apple builds

Both need a Mac, and both consume the `build-gen-tables/` folder produced by the PC build
(addresses only; no ROM is needed on the Mac):

```bash
tools/mac/build_all.sh build-gen-tables [art-folder] [icon]
tools/ios/build_all.sh build-gen-tables [art-folder] [icon]
```

The Mac build writes `build-mac-dist/pokemon-ds-wide-pc-macos-<arm64|x86_64>.zip`; the iOS
build writes `build-ios-dist/pokemon-ds-wide-ios.ipa` (unsigned, for sideloading).
`tools/apple/fetch_gba.sh` prepares the GBA work folder on Apple hosts. CI runs the compile
checks; see `.github/workflows/mac.yml` and `ios.yml`.

## Switch build

```bash
tools/switch/build.sh "<your SoulSilver US.nds>"   # one game; output contains game code
```

The all-games Switch download is built with `tools/switch/build_game.sh` (see the header of
that script). The fast build (`SSR_SWITCH_JIT=1`) uses table-only generation; the full build
compiles the game's recompiled code.

## Tests

```bash
python3 -I -m unittest discover -s tests -p 'test_*.py'
```

Notes:

- The Python tests wrap C++ test programs and compile them, so a C/C++ compiler is required.
  Without `cc`, `test_gba_wide_clip` and similar fail for environmental reasons, not a code
  bug.
- Launcher input tests and the VDF/Steam writer tests live in `tests/`.
- Android instrumentation sources are under `tests/android/`; they need a device or emulator
  and the Android SDK.
- `python3 tests/native_smoke.py --build-dir <dir>` builds the ROM-free synthetic fixture,
  links the embedded data and compiles the native frontend. CI runs it on every push.
- Translation coverage is checked by `tests/test_all_languages.py`, which runs the generators
  with `--check`.

## Box art

### DS

`SSR_PC_ART` folder, `<code>.jpg`, named as for the list's `icons/` folder (IREO, IRBO, IPGE,
IPKE, CPUE, AD…). CMake embeds them into the binary.

### GBA

`gba/art/<name>.jpg` (512×512), names from `gba/frontend/games.h`:

`emerald`, `emeraldlegacy`, `firered`, `leafgreen`, `monomyth`, `radicalred`, `ruby`,
`sapphire`, `ultraviolet`, `unbound`, plus regional/hack variants.

`gba/tools/embed_art.py` builds the header. The art is **not in the repository** (Nintendo
artwork) and `gba/art/` is gitignored. To test with it, extract the 512×512 JPEGs from a
release APK's `lib/arm64-v8a/libmain.so` or supply your own — never commit them.

## How the program fits together

- `melonDS/` is the DS hardware runtime and the 3D renderer (`GPU3D_WideGpu*`, with OpenGL
  through EGL on desktop). It is driven from `frontend/main.cpp`.
- `frontend/` knows what the game is doing (which screen, menu, battle, text box) and
  rearranges the two DS screens into one widescreen picture. Game-specific code is split by
  generation: `bw1.h`, `bw2map.h`, `companion_*` and similar. `frontend/funcwatch.h` reads
  the few function addresses it needs from the player's own ROM at runtime, against the
  tables in `data/funcs/`.
- `frontend/pc_launcher.cpp` is the all-games start screen: game list, ROM folder, box art,
  game options, Hub and cloud saves, Steam shortcuts.
- `runtime/recomp_*` and `gen/` belong to the single-game recompilation path; the all-games
  builds do not use recompiled game code.
- Interface text is generated: `localisation/translations.json` and
  `guide_translations.json` become `frontend/ui_translations.h` and
  `frontend/guide_translations.h` through `tools/localisation/`. See
  [`localisation/README.md`](../localisation/README.md).
- Online features are optional build flags: the Hub (`SSR_POKEMON_HUB`), the theme Gallery
  relay (`FULLDEX_RELAY_URL`) and Google Drive cloud saves (`FULLDEX_GOOGLE_CLIENT_FILE`).
  The client file and users' tokens never enter the repository.
- RetroAchievements use rcheevos (vendored under `frontend/thirdparty/rcheevos`).
- The 3DS games use a separate program under `3ds/program/` (`frontend/n3ds.h`); the PC
  program runs its `--prepare` step once from the player's dump and then launches it.

## Coding notes

- C++17, SDL2. Keep changes small and direct; comments in parentheses explain *why*.
- Launcher input: pointer/touch hit-tests the target, focuses it, and activates it; keyboard/
  gamepad navigate focus and activate with A/Enter. Keep `choose()`, the `SDL_FINGER*` handlers
  and the mouse handlers consistent. [`UI-INPUT-MODES.md`](UI-INPUT-MODES.md) describes
  the model.
- Steam `shortcuts.vdf` must stay byte-for-byte identical apart from the edited entry
  (`frontend/vdf.h`, `frontend/steam_shortcuts.h`).
- Do not commit ROMs, saves, keys, the Google client, or Nintendo artwork.

## Branches, issues and PRs

- Work on a branch (`fix/…`, `feature/…`, `docs/…`); never push directly to `main`.
- File bug/feature issues with the templates in `.github/ISSUE_TEMPLATE/`.
- Keep commits focused and describe *why* in the body. Use a short imperative subject.
- Add a `CHANGELOG.md` entry for player-visible changes.
- A PR should say what changed, what was tested, and what was not (no ROM, no device, …).
- Never attach ROMs, saves, credentials or build artifacts to a PR.

## CI

- `.github/workflows/native.yml` runs on every push and PR: unit tests and the native smoke
  build on Ubuntu and macOS, tests on Windows (MSYS2), and a Mac all-in-one compile check.
- `.github/workflows/release.yml` (manual) builds the setup executables for Linux, macOS and
  Windows and their AppImage, and attaches them to a published release.
- `.github/workflows/mac.yml` and `ios.yml` (manual) build the Mac and iOS downloads from
  inputs held in a draft release; no ROM is ever given to a workflow.
- All fixtures are synthetic. Never add a ROM, save, key or game code to CI.

## Release packaging

The release assets are produced by the `tools/*/build_all.sh` scripts (PC, Android, Switch,
Apple):

- `pokemon-ds-wide-pc-windows-x86_64.zip`
- `pokemon-ds-wide-pc-linux-x86_64.tar.gz` and `.AppImage` (+ `.zsync`)
- `pokemon-ds-wide-pc-macos-arm64.zip`, `pokemon-ds-wide-pc-macos-x86_64.zip`
- `pokemon-ds-wide-android.apk`
- `pokemon-ds-wide-ios.ipa`
- `pokemon-ds-wide-all-switch.zip`

`tools/updater/write_checksums.py` writes `SHA256SUMS.txt` next to the PC assets, with a
`# fulldex-build <asset> <epoch>` line for each binary so an unchanged asset keeps its build
ID. The AppImage's update information points at the PC release's `.zsync`
(`gh-releases-zsync|mamesj|pokemon-fulldex|pc-v1|…`), which is how Gear Lever and
AppImageUpdate find new versions. Include the licences the build needs: `LICENSE-mGBA.txt`
when GBA is built in, `LICENSE-SDL2.txt` for the shipped SDL2, and the font licences.

## Testing on a device

- **Steam Deck / Linux handheld:** install the AppImage somewhere stable (Gear Lever, or
  `~/AppImages`), add it as a non-Steam game, and set 3D Resolution to 720P. The program
  writes `game.log` beside the data folder (`~/.local/share/pokemon-ds-wide` when installed
  as an AppImage); watch it over SSH while reproducing a bug. Steam shortcuts point at a
  fixed AppImage path, so replace that file rather than adding a second shortcut.
- **Android:** use `adb logcat` for crashes; app data lives under
  `Android/data/io.github.mamesj.pokemonds/files` unless a saves folder was chosen. Back up
  app data before installing a differently signed test build.
- **Windows:** the data folder is the unzipped folder; `game.log` is written there.

## Troubleshooting

- **`fatal: detected dubious ownership`** — the repo is on a mount owned by another user:
  `git config --global --add safe.directory "$PWD"` (or use `git -c safe.directory=$PWD`).
- **Windows target fails to find MinGW** — install
  `g++-mingw-w64-x86-64-posix`; the script looks for `x86_64-w64-mingw32-g++-posix`.
- **AppImage step fails on Pillow or zsync** — install `python3-pil` and `zsync`.
- **Build says the ROM is not SoulSilver** — the header at offset 12 must be `IPGE`.
- **Two builds fight over `index.lock`** — only run one `build_all.sh` at a time.
- **GBA silently missing** — `SSR_GBA_DIR` was empty or the mGBA work folder was not at
  `$HOME/pokemon-gba-wide` (or `GBAW_WORK`).
- **`/dev/fuse` errors when making the AppImage** — the script sets
  `APPIMAGE_EXTRACT_AND_RUN=1`; if it persists, install AppImage's `appimagetool` on the
  host or run outside the sandbox.
