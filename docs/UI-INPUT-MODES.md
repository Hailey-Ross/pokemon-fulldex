# UI input modes

Design for Fulldex's UI across touch, mouse, keyboard and gamepad — including several
inputs active at once on the same device (e.g. an Android phone with a Bluetooth controller
and a touchscreen).

Status: **settings pages implemented and verified** — the game pause settings and the
launcher's own Settings pages.

Implemented:
- `frontend/ui_input.h`: `Mode`, `Intent`, `Part`, `Gesture`.
- Game pause settings: parts/intents registered at draw (row name = focus; Off/On pills =
  set; chevrons = step; preview cards = set; links/Back = open/back). The pointer hit-tests
  the parts; the pad keeps row focus + Left/Right; A now only opens links/Back/slots/keys.
  Chevrons dim at the first/last choice. A drag anywhere on the settings page scrolls.
- Launcher settings (the glass page): the first touch on a group row opens it; a pill changes
  the value directly (Off then On, and On then Off); a chevron steps; a preview card selects
  its choice; the row name only selects; a drag scrolls; A opens links/groups only.
- Android defines `SSR_PHONE`, so the phone touch hints are actually compiled in.

Verified:
- Android: tapping the On pill sets the toggle; tapping the row name only selects; an
  upward swipe scrolls the Cheats list.
- Android: every control in Settings -> Display (toggles, pickers, sliders, links, group rows).
- Steam Deck: pad Left/Right changes the focused toggle; A on a toggle does nothing.
- Windows: build produced and pushed to the test box.

Still to do: the launcher's other surfaces (tabs, sidebar, banner, grid, game menu, tab
options, ROMs, hub, about, music bar), the pause tile page, the visual last-input mode
(focus ring vs hover), the simultaneous-input matrix, and the shared gesture classifier.


## Goals

- One UI, one set of widgets, two interaction styles (pointer and focus).
- The visible control is the hit target: a pill is a button, a chevron steps, a card
  chooses. No hidden "the row is the button" behaviour.
- The shown value **is** the selection. There is no separate "select/confirm" step for
  toggles, pickers or sliders.
- Any device can use several inputs simultaneously. Input mode is per event (last used),
  never a device-level lock.
- Scrolling is a drag; only a release without movement is a tap.

## Input sources

- touch (finger), mouse, keyboard, gamepad (Bluetooth, USB, Steam Deck's integrated
  controls), trackpads, the app's on-screen controls.
- Detection: the SDL event source; the last source selects the *presentation* mode.
- All handlers remain active. Connecting a controller must not disable touch, and touching
  must not disable the controller.

## Two presentation modes

| | Pointer / touch | Pad / keys |
| --- | --- | --- |
| Focus | set by the press/hit test; hover shows where | persistent row focus, moved with D-pad/stick |
| Value | tap the visible control (pill, chevron, card, track) | Left/Right on the focused row |
| Activate | tap the action part (link, Back, slot, key) | A/Enter on the focused row |
| Scroll | drag beyond a threshold | D-pad/stick up/down |
| Feedback | hover, pressed | focus ring |

Rules:

- The last event switches only the presentation (ring vs hover), not the layout.
- The focus is shared: a pointer press on a row sets the focus there; a pad event moves it.
- Pointer focus is soft: when the pointer moves away the hover clears, but the row focus
  remains so the pad can take over.
- Touch has no hover; the pressed state appears on finger-down.
- After a pointer timeout, the hover feedback fades; the pad's next event restores the ring.

## Interaction intents

Both modes emit the same intents:

- `Focus(item)` — selection/highlight only
- `Step(item, -1/+1)` — change a choice/toggle value
- `Set(item, value)` — set a value directly (preview card, slider position, pill)
- `Open(item)` — links, Back, slots, keys, folders
- `Back`
- `Scroll(delta)` — drag, wheel, stick at the list edge
- `Drag(item)` — slider knob

Widgets expose **parts** with rects and intents at draw time. The pointer hit-tests the
parts; the pad uses the focused row and the same intents. A toggle/picker row has no
`Open` intent; A on it does nothing.

## Gesture classifier (shared)

One routine for the pause menu and the launcher:

- down: record x/y and the part/row.
- move beyond `max(12px, 1/60 of the short screen side)`: drag → `Scroll`, no tap.
- release without movement: `Tap` → the part's intent.
- pointer presses on a control do not block a drag: dragging anywhere scrolls.

## Widget catalogue

### Launcher

| Element | Pointer / touch | Pad / keys |
| --- | --- | --- |
| Top tabs (Home, All, DS, GBA, 3DS, Romhacks, custom, +) | tap to switch | LB/RB or up from the grid; Left/Right; A |
| Search, Sort buttons | tap | focus + A (on the tab bar) |
| Sidebar (Home, Hub, ROMs, Settings, Launcher, About) | tap | D-pad left/up; A |
| Banner Continue (primary) | tap to play | up to the banner; A |
| Banner Game Options | tap: focus the button, open the selected game's menu under it | up/Right; A |
| Game card | tap to focus; tap again to play; double-tap; drag on the grid scrolls | focus; A to play; Y for the game menu |
| Game menu (Play, Manage Saves, Remove From Home, Add To Steam/Desktop, Rename, Change Picture, Add To Tab, Remove Game) | tap a row | focus + A |
| Tab options (Move Left/Right, Hide, Rename/Delete) | tap | focus + A |
| ROM list (ROMs page) | tap a row; drag scrolls | focus + A |
| Choose ROMs Folder | tap | focus + A |
| Hide From List / hidden toggle | tap the pill | focus + Left/Right |
| Hub cards/actions | tap | focus + A |
| Settings category page | tap a category | focus + A |
| Settings rows: toggle pair | tap the Off or On pill; label selects | Left/Right changes the shown value; A no-op |
| Settings rows: picker `< value >` | tap the left/right chevron; tap a preview card | Left/Right; preview cards are pointer-only |
| Settings rows: slider | drag the knob/track; tap the track | Left/Right with repeat |
| Settings rows: link/key/folder | tap | focus + A |
| Settings header Back | tap | B/Esc; focus + A |
| Music bar (previous/play/next) | tap | down from the tiles; Left/Right; A |
| About links | tap | focus + A |

### Game (pause menu)

| Element | Pointer / touch | Pad / keys |
| --- | --- | --- |
| Pause tile page (Resume, Save, Load, Fast Forward, Cheats, Pokédex, Companion, Achievements, Settings, Quit) | tap a tile; drag on the tiles scrolls when the rows overflow | focus; A |
| Fast Forward tile (toggle tile) | tap the tile (on/off) | focus; A |
| Settings pages | as the launcher's settings rows (pills, chevrons, sliders, links) | as above |
| Cheats page: SEARCH / ON NOW / DATABASE INFO | tap | focus + A |
| Cheats folders | tap to open in place | focus + A |
| Cheat rows (Off/On) | tap the pill; label selects; drag scrolls | Left/Right changes the value; A no-op |
| Cheat search keyboard | tap keys; system text input | focus + A; text input |
| Companion party/bag/dex lists | tap rows; drag scrolls | focus; A |
| Save/Load slot grid | tap a slot; drag scrolls when needed | focus; A |
| Backup/thumbnail rows | tap | focus + A |
| Dialog buttons (Quit, Save problem, Restore, Remove, Rename, slot replace) | tap | focus; A (confirm), B (cancel) |
| Notes/toasts | none (status only) | none |

### In-game

| Element | Pointer / touch | Pad / keys |
| --- | --- | --- |
| On-screen D-pad, A/B, X/Y, L/R, START/SELECT, MENU, FAST, NAV | touch; pressed states | physical pad buttons work simultaneously |
| DS touch screen / GBA (no touch) | touch/drag | D-pad/stick; the game's own input |
| Companion overlay | tap | bound button |
| Pause/MENU | on-screen MENU | START/Esc (per settings) |

## Simultaneous inputs

### Rules

- All sources are always active. No mode lock, no "controller detected, disable touch".
- Shared focus: a touch sets the row focus; a pad event moves it; the presentation follows
  the last event.
- Avoid double activation: the policy is set per window, in one place each. The launcher
  turns SDL's synthesized touch→mouse on (its taps are clicks, and a phone with a controller
  still works); the game turns it off and takes `SDL_FINGER*` itself for its menus and touch
  screen. `SSR_PHONE` is defined by the Android build, so the phone hints and layout are
  compiled in. No event is acted on twice.
- On-screen controls and a physical pad feed the same game input state (OR of held
  buttons); the touch screen and the pad do not conflict.
- The gamepad focus ring and a touch press can coexist; touching a row updates the focus
  the pad will continue from.

### Scenarios

| Device | Inputs | Expected |
| --- | --- | --- |
| Android phone | touch + Bluetooth/USB controller | both work at any time; touch sets focus; pad continues from it; game input merges |
| Steam Deck | integrated controller + touchscreen + trackpads/mouse | same; trackpad acts as a pointer |
| Desktop | mouse + keyboard + controller | same |
| iOS | touch + controller | same |
| Any | controller only | focus navigation everywhere; no pointer needed |

## Visual feedback

- Pad focus: a bright row/part outline (the existing focus ring).
- Pointer hover: a subtle surface/edge, no ring.
- Pressed: a filled/raised state; release without movement commits.
- Active value: the filled pill, the selected preview card, the slider knob.
- Disabled: dimmed; pointer no-op; pad skips the row.
- Chevrons dim at the first/last choice.
- No hover on touch; no focus ring in pointer-only use until the pad is used.

## Implementation

- New `frontend/ui_input.h`: `InputMode` (Pointer/Pad), `Intent`, `Part { rect, intent }`,
  `Gesture`.
- `Menu` (pause) and the launcher register parts while drawing; the pointer handler
  hit-tests parts and emits intents; `Menu::Update` emits the same intents from the focused
  row.
- `MenuItem` holds `kind`, `choices`, `value`, `enabled`; the shown value is the state.
- Delete `SettingsTap`'s x-position heuristics; keep a single `Step`/`Set`/`Open` dispatch.
- One gesture classifier shared by the launcher and the pause menu (today they duplicate).
- The touch→mouse policy lives where each window is created: the launcher synthesizes
  clicks; the game takes fingers itself. (Done; see Simultaneous inputs.)
- Tests: extend `tests/local_menu_touch.cpp` for part hit-tests, tap-vs-drag, pills,
  chevrons, sliders, and the shared focus; add the simultaneous-input cases (synthetic
  events).

## Test matrix

- Phone: touch only; touch + Bluetooth controller; touch + USB controller.
- Deck: integrated controller; touchscreen; trackpads/mouse; external controller.
- Desktop: mouse + keyboard; controller; both.
- iOS: touch; touch + controller.
- Cases: tap vs drag; pills direct; chevron ends disabled; slider drag; row label selects;
  preview cards; long lists; dialogs; on-screen controls + physical pad; focus persistence
  across input switches.

## Roadmap (commits at each milestone; no PR until fully tested and authorized)

1. **Design** — this document. Commit on `ui/input-modes`.
2. **Primitives** — `ui_input.h`, the gesture classifier, the shared focus state; no UI
   change yet.
3. **Settings page** — the worst offender: pills/chevrons/sliders/label/preview under both
   modes; phone + Deck tests.
4. **Game menus** — pause tiles, cheats, companion, save/load, dialogs, keyboard; phone +
   Deck.
5. **Launcher** — tabs, sidebar, banner, grid, game menu, ROMs, settings, hub, about,
   music bar; phone + Deck + desktop.
6. **Multi-input matrix** — controller + touch + mouse on Android, Deck, desktop, iOS;
   docs; tests.
7. **Review and authorization** — only then consider a PR.
