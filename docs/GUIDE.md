# The Guide

The Guide is the pause menu's COMPANION tab: hints for the moments you'd otherwise
open a walkthrough. It is written to spoil as little as possible: an entry starts as a
title and a hint, and A or a tap shows its answer in the preview beside the list (and
hides it again). An answer longer than the preview scrolls with Left/Right, the mouse
wheel while the pointer is over it, or a finger's drag on it; the bar on its right shows
how much of it is there. The list keeps which answers you have shown for the session.
NEXT BOSS and HERE work the same way.

Pokémon Black (IRBO) is the first game with the full set of sections; the other DS
games keep their hand-written entries under one QUESTIONS heading.

## Sections (Black)

- **NEXT BOSS** — the gym the save's badges point at next, or the Elite Four after the
  eighth badge. The hint gives the place, the highest level on their team and the level
  of your strongest Pokémon; A or a tap on the row shows the answer in the preview: the
  badge and TM, then the team `Lv 18 HERDIER (NORMAL) ...`. The Striaton leader is the one whose type beats
  your starter (Snivy → Chili, Tepig → Cress, Oshawott → Cilan); with no starter
  recognised, all three teams are shown. At eight badges the four Elite Four rooms are
  summarised by their aces; their full teams, and N's and Ghetsis's, are in GYMS.
- **HERE** — the place you are in now, with the wild Pokémon the game's own encounter
  tables list for it. Each way they are met (grass, dark grass, rustling grass, surf,
  rippling water, fishing, ...) gets its own line, and each Pokémon with its level range
  a line under it. A long list scrolls (Left/Right, the wheel over the preview, or a drag
  on it; the bar on the right shows the part in view); LOCATION has the same table. A
  town says it has none and points at the routes around it.
- **NEXT STEPS** — the main story by milestone: Nuvema → Striaton → Nacrene → Castelia
  → Nimbasa → Driftveil → Mistralton → Icirrus → Opelucid → Victory Road/League → the
  post-game.
- **GYMS** — the type to bring, the badge and TM, and each Elite Four leader's team
  (including N and Ghetsis) with their aces.
- **WHERE IS** — the HMs and key items. An entry whose item is in the bag says HAVE
  and carries a ball icon, so WHERE IS doubles as a checklist.
- **EVOLUTIONS** — trade, stone, friendship and late level-up evolutions.
- **LEGENDARIES** — where each one waits and what it needs (including the event-only
  three).
- **POST-GAME** — the National Dex, eastern Unova, Black City, the Battle Subway, the
  rematches, the Relic Castle and the sages.
- **STUCK?** — the blockers people hit (the Drawbridge, the Cold Storage, Chargestone
  Cave's magnetic rocks, Dragonspiral Tower, the Relic Castle, the way to the League,
  N's Castle) with the step that clears each.
- **TIPS** — seasons and encounter types, Audino, repels, critical captures, version
  differences, the Dream World, training, triple/rotation battles and hidden items.

## Data and sources

- The hand-written pages are adapted from PokeDaisy (GPL-3.0, Lidor Itzhari), as the
  earlier games' pages are; the About page credits it.
- The boss teams in `frontend/companion_black_bosses.h` are generated from the cart data
  PokeDaisy's `scripts/gen_gen5_guide.py` reads (`BLACK_BOSSES_RAW`: levels, species ids
  and moves), with the species names and types from the same cart's tables. Facts only:
  levels, species and types; no game text is copied. The Striaton choice and the badge
  order (`badgeIndex`) are this file's.
- HERE reads the Companion's own metadata (`PlaceName`, `EncounterLines`) for the map
  the save says the player is on; the item HAVE marks use the bag's item ids (HM01-HM06
  420-425, BICYCLE 450, DOWSING MCHN 471, SUPER ROD 447).

## Limits and next steps

- The Elite Four room flags are not read yet, so at eight badges NEXT BOSS shows the
  four rooms rather than which ones are done. Black's flags are 0x969-0x96C (Shauntal,
  Grimsley, Marshal, Caitlin) in the cart's save; reading them would let the card move
  to the next room and then to N and Ghetsis.
- The answer preview scrolls with Left/Right (a line a press, three when it is long), the
  wheel, or a finger's drag on the preview, with a bar on its right. The Elite Four's
  full teams stay in separate GYMS entries because they read better as entries than as a
  wall of lines.
- White (IRIS in Opelucid, N's Reshiram) and Black 2/White 2 can follow the same shape:
  a boss table and, for B2W2, the Challenge Mode teams.

## Test hooks

- `SSR_TEST_PAGE=<frame>,28` opens the pause menu on the GUIDE page (28 is PAGE_CGUIDE
  in the PAGE_ list) at that frame, counted from the run's start.
- `SSR_TEST_GUIDE=1` selects NEXT BOSS with its answer shown; `SSR_TEST_GUIDE=here`
  selects HERE hidden, and appending `1` (`here1`) shows its answer.
- Pair with `--state-load <slot> --relative --frames <n> --shot-dir <dir> --shot-at <f>`
  for repeatable pictures of the Guide.
