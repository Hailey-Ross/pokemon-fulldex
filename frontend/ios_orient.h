#pragma once
// iPhone and iPad (tools/ios/build_all.sh). The game list turns with the device (upright or
// sideways) and a game is sideways, on a phone or a tablet. SDL reads its orientation hint when a window is
// made, and from iOS 16 a window already on screen does not turn unless asked.

// The list. Call before its window is made, and again once that window exists.
void IosOrientLauncher();
// A DS or GBA game. Call before its window is made. A window made later (the GBA
// game's own) is turned when it is shown.
void IosOrientGame();
// The screen's safe area (clear of the Dynamic Island, the notch and the home bar), in the
// window's points, for the list to keep its tabs and buttons in (#153).
void IosSafeInsets(int* top, int* bottom, int* left, int* right);
// A game controller's buttons also reach UIKit as presses, which SDL turns into keys: the
// D-pad into the arrow keys, A into Return and B into Escape (#166: B opened Fulldex's menu,
// and one press on the D-pad moved a menu's cursor twice, once for the pad and once for the
// key). Those keys are dropped, unless a keyboard is connected or text is being typed (SDL
// then takes keys from the keyboard itself, and presses are not turned into keys). Keeps any
// event filter already set. Call once a window exists; calling again does nothing.
void IosIgnoreControllerPresses();
// Whether a hardware keyboard is connected. The list's own keyboard (a new tab, a name, the
// search) asks SDL for text only then: SDL_StartTextInput also brings up iOS's keyboard, which
// covered the list's own (the bottom half of an iPad's screen).
bool IosHardwareKeyboard();
