// The glass pause menu of the DS games (ui.h, ui_settings.h, glass.h), for the GBA player built
// into FullDex: the GBA player says what is on its page in plain words and numbers, and FullDex
// draws it and moves the highlight with the same code as the DS games' menu. (The GBA player has a
// Canvas and a Menu of its own, in its namespace: it cannot include ui.h itself.)
// Defined in gba_glass_impl.h, which main.cpp includes when the GBA games are built in (SSR_GBA).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace GbaGlass
{
// (The numbers of Glass::Icon, glass.h; gba_glass_impl.h checks that they still agree.)
enum { IC_PLAY = 0, IC_SAVE = 1, IC_LOAD = 2, IC_CHEATS = 3, IC_FAST = 4, IC_TROPHY = 5, IC_GEAR = 6, IC_POWER = 7,
       IC_MONITOR = 9, IC_RATIO = 12, IC_EXPAND = 14, IC_FULLSCREEN = 15, IC_GRID = 16, IC_PAD = 18, IC_GAUGE = 19,
       IC_SPARKLE = 22, IC_INFO = 23, IC_SPEAKER = 30, IC_REWIND = 31 };
enum Kind { PLAIN, TOGGLE, PICKER, LINK, SECTION, BACK, SLOT };
enum Style { TILES, PAGE, DIALOG };

struct Item
{
    std::string label, value, sub, desc, tip;
    int kind = PLAIN;
    int icon = -1;
    int slider = -1;       // 0 to 10: drawn as a bar too (VOLUME)
    bool disabled = false; // greyed, cannot be chosen
    bool info = false;     // the value is only shown
    std::vector<std::string> choices;
    int choice = -1;
    // SLOT: a save state's card on SAVE and LOAD (Page::slotGrid), as the DS games have them: its
    // picture (ARGB, thumbW x thumbH; none: an empty card) with `value` (when it was saved) under it.
    std::vector<uint32_t> thumb;
    int thumbW = 0, thumbH = 0;
};

struct Page
{
    int style = TILES;
    int icon = -1;     // the page's (PAGE) or the question's (DIALOG)
    bool tall = false; // PAGE: a few tall rows, a line under each name (SETTINGS)
    bool slotGrid = false; // PAGE: its SLOT items as a grid of cards (SAVE and LOAD)
    std::string title, hint, note, headerNote;
    std::vector<Item> items; // PAGE: the first is its BACK. DIALOG: the action first, CANCEL last.
    int selected = 0;
    float touchRows = 1.0f; // (phones: the rows this much larger)
};

struct Input
{
    bool up = false, down = false, left = false, right = false, accept = false, back = false;
};

// When the save state in that file was saved, as the DS games' slots say it ("Today, 15:40");
// empty: no such state.
std::string SlotWhen(const std::string& statePath);
// A save state's picture beside it (<state>.bmp): written from the paused game as it is on the
// screen (ARGB, w x h), made small; read back for its card (false: none, or older than the state).
bool SaveSlotPicture(const std::string& statePath, const uint32_t* argb, int w, int h);
bool LoadSlotPicture(const std::string& statePath, std::vector<uint32_t>& argb, int& w, int& h);
// The paused game as it is on the screen (ARGB, w x h, the size the menu is drawn at): blurred
// under the menu. Null: none (the menu is drawn over the darkened game).
void Frame(const uint32_t* argb, int w, int h);
// Moves page.selected as the DS menu would. The item chosen this frame, or -1; dir -1 or 1 when it
// was left or right on it, 0 for accept.
int Update(Page& page, const Input& in, int& dir);
// A tap at (x, y) of the picture last drawn: page.selected and `out` as the DS menu takes it.
// slider: 0 to 10 when a slider's bar was tapped, else -1. pick: the choice card tapped in the
// preview (for the chosen row), else -1.
void Tap(Page& page, int x, int y, Input& out, int& slider, int& pick);
// The page drawn at w x h (ARGB, alpha as the DS menu's canvas). changed: false when it is the
// same picture as the last call's.
const uint32_t* Draw(const Page& page, int w, int h, bool& changed);
}
