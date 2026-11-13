# FullDex expansion roadmap

PokeDaisy 1.5.0 features against FullDex, by phase of the expansion plan.

**State of this file:** first pass, written 11 November 2026 from PokeDaisy's source at `135ac80`
(1.5.0.1), its README and `docs/ROM_FEATURES.md`, and from FullDex's review tree (`4c114df` plus the
uncommitted review work). The "FullDex today" column was filled in from FullDex's source files and
menus, not from playing each feature: rows marked *to verify* were not opened in the code at all.
Nothing here is marked Tested unless a test or a capture is named.

Statuses: Not started, In progress, Implemented untested, Tested, Blocked, Unsupported.

## Licence

PokeDaisy is GPL version 3 with an added term (section 7(b)): anything based on it must keep the
line "Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy" in NOTICE, in the
README, and wherever credits are shown. FullDex already carries PokeDaisy code
(`frontend/pokedaisy/`), so the term applies now: check that NOTICE, the README and the About and
Licences pages all carry that line before the next release. Each ported file names its source in
its header (`frontend/ff_music.h` does).

## Phase 0: audit

| Item | Status | Notes |
|---|---|---|
| FullDex source inspected | In progress | Read: pause menu, GBA player, fast-forward sound, game music, merged edges. Not read: launcher, hub, cloud, companion data, touch layout, platform renderers. |
| PokeDaisy source inspected | In progress | Read: DS music, DS EXPAND compose, fast-forward music (GBA), README, ROM_FEATURES columns. Not read: Kotlin companion, link play, save editor, shaders, skins. |
| APK contents | Not started | The 1.5.0 APK was only listed (dex, two native ABIs, shaders, fonts). |
| Build and regression baseline | In progress | Windows test build compiles. The 118 Linux tests were not re-run after 11 November's changes. |

## Phase 1: interactive companion

| Feature | PokeDaisy | FullDex today | Work | Platforms | Status |
|---|---|---|---|---|---|
| Party, Bag, Dex, Map, Guide, Battle pages | Yes | Yes, as glass pages (`companion_*.h`, pause menu COMPANION) | Keep | All | Implemented untested (not re-checked here) |
| PC from anywhere (DS) | `Pc.kt`, `pk_nds_call` | Yes: `PokeDsActions::OpenPc` | Keep | DS games with a recipe | Implemented untested |
| Other DS field actions (Fly, Bike, Rod, Repel) | `pk_nds_call` actions | Only `PK_NDS_ACTION_PC` is exposed | Expose the remaining actions PokeDaisy's recipes carry, each behind the same idle-state checks | DS games with a recipe | Not started |
| GBA field actions, items, Fly | `FieldItems.kt`, `Fly.kt`, `pk_call.c` | *to verify* | Port per game table | GBA | Not started |
| Party reorder, give and take items, use medicine | Yes (GBA; DS partly) | Read only | Needs write paths with legality and safe-point checks | Per game | Not started |
| Boxes while playing (withdraw, deposit) | GBA Gen 3 and hacks | Hub moves Pokemon between saves with the game closed | Port live box access; keep the Hub | GBA first | Not started |
| Nuzlocke tracker | Yes | Yes (`nuzlocke_tracker.h`) | Keep | All | Implemented untested |

## Phase 2: battle companion

| Feature | PokeDaisy | FullDex today | Status |
|---|---|---|---|
| Foe info, matchups, coverage, recommended move and switch | Yes | Yes (`companion_battle.h`, BATTLE INFO pages) | Implemented untested |
| One-press move picks, battle buttons | `BattleInputController` | Battle buttons exist for the DS layouts; one-press picks *to verify* | Not started |
| Foe's remaining team, capture status | Yes | *to verify* | Not started |

## Phase 3: widescreen camera and graphics

| Feature | PokeDaisy | FullDex today | Status |
|---|---|---|---|
| DS widescreen world | EXPAND (`pk_nds_wide.cpp`): extra slices round the screen | FullDex's own merged widescreen | Keep FullDex's |
| 2D layers blended over the 3D carried into the margins | Drawn from the layer itself with the game's blend registers | Judged from the finished picture (`MergedEdgeContinuation`) | Not started: the likely fix for the Black 2 banner corners, see the handover |
| Zoom, look-around, auto zoom | Yes (DS and GBA) | No | Not started |
| GBA EXPAND past the screen edges | `pk_wide.c`, per game tables | ROM widescreen patches per game | Not started (would help hacks the patches do not fit) |
| Fast-forward frame skip in the DS core | Yes | Yes (`rc_frame_hidden`, `rc_wide_render_skip`) | Keep |

## Phase 4: shaders and effects

| Feature | PokeDaisy | FullDex today | Status |
|---|---|---|---|
| LCD, scanlines, CRT, GBA colours, edge smoothing | Yes | GBA: HD filter (xBRZ), LCD effect, colour correction. DS: screen filter | Not started for the rest |
| RetroArch overlays and bezels | Yes | No | Not started |

## Phase 5: gameplay and speed

| Feature | PokeDaisy | FullDex today | Status |
|---|---|---|---|
| Fast forward, smart fast forward, slow motion, rewind, turbo A | Yes | Yes | Keep |
| Fast-forward music at normal speed, DS | `pk_nds_music` | Ported as `frontend/ff_music.h` | Implemented, measured headless in Black 2 and HeartGold on Windows; not yet heard by James; off on Switch, Apple and Android |
| Fast-forward music at normal speed, GBA | Clips recorded per song (`FfMusicRenderer`) | Stretches (`fast_forward_audio.h`) | Not started |
| Scene speeds (battle, text, menu, bike, surf) | Yes | Smart fast forward only | Not started |
| Instant text, reusable TMs, auto-run, run indoors, wild encounters off, shiny odds | Per game tables | Some as cheats (GBA built-in cheats: no wild encounters and others) | Not started as options |

## Phase 6: Dex, maps, guides

| Feature | PokeDaisy | FullDex today | Status |
|---|---|---|---|
| Dex search and filters, learnsets, evolutions | Yes | Yes (Dex pages, filter, reference search) | Implemented untested |
| Encounters here, map, guide with hints first | Yes | Yes (`companion_guides.h`, encounters, map) | Implemented untested |
| Hack-aware data | Per hack tables | *to verify* | Not started |

## Phase 7: appearance

Following Pokemon, player and rival looks, day and night tints: PokeDaisy has them per game; FullDex has none. Not started.

## Phase 8: save editing and link play

| Feature | PokeDaisy | FullDex today | Status |
|---|---|---|---|
| Save-to-save trades, trade evolutions | Gen 3 | Hub (`pokemon_hub.cpp`, `trade_evolutions.cpp`), friend rooms | Keep |
| PC box, bag and move editing | Gen 3 | *to verify* | Not started |
| GBA link cable, 2 to 4 games | `pk_link.c`, co-op | No | Not started |

## Phase 9: GB/GBC and hacks

Game Boy and Game Boy Color play, and PokeDaisy's hack tables (Unbound, Radical Red and others): FullDex has neither the GB cores nor those tables. ROM patching and hack save separation exist. Not started.

## Phase 10: Android, controls, devices

| Feature | PokeDaisy | FullDex today | Status |
|---|---|---|---|
| Touch layout editor | Yes | Yes (`touchlayout.h`) | Keep |
| Delta skins, haptics, idle fade | Yes | No | Not started |
| Quick menu (ring or grid) | Yes | Quick actions toggle only | Not started |
| Two-screen and folding devices | Yes | Second screen support on Android (`android_second_screen.h`) | *to verify* |
| Fullscreen recovery on the Retroid Pocket 6 | Tested there (`goImmersive`, focus change) | Candidate fix, unverified on hardware | Compare the two; Not started |

## Phase 11: launcher

Multiple ROM folders, zipped ROMs, per-game names, shortcuts: FullDex has the launcher, ROM folder choice, Home tabs and custom names planned; zip and 7z import *to verify*. Not started here.

## Phase 12: release testing

Not started. Platforms actually built for the 11 November work: Windows only.

## Order of work from here

1. James's verdict on the DS fast-forward music, then Android and Switch decisions for it.
2. Black 2 banner corners with the layer-based method.
3. Expose the remaining DS field actions (the recipes are already in the tree).
4. GBA fast-forward music clips.
5. The rest in the plan's order.
