#pragma once
#include <cstdlib>

// The shared input model for the launcher and the game's menus (see docs/UI-INPUT-MODES.md).
//
// The same widgets are driven two ways:
//   Pointer: the visible control (pill, chevron, card, track) is the hit target.
//   Pad/key: the focused row changes its shown value with Left/Right; A only opens links.
//
// A widget registers its parts while drawing; the pointer path hit-tests them and emits an
// intent. The pad path emits the same intents from the focused row. The shown value is the
// selection: there is no separate select/confirm step for toggles, pickers or sliders.

namespace UiInput
{
enum class Mode
{
    Pointer,
    Pad
};

enum class Intent
{
    Focus, // select/highlight only
    Step,  // value = -1 (left/back) or +1 (right/forward)
    Set,   // value = the choice (toggle pill, preview card, slider position)
    Open,  // links, Back, slots, keys, folders
    Back,
    Drag   // a slider knob
};

struct Part
{
    int x = 0, y = 0, w = 0, h = 0;
    Intent intent = Intent::Focus;
    int item = -1;  // the row the part belongs to
    int value = 0;  // Step: -1/+1; Set: the choice; Drag: -1
};

inline bool InPart(const Part& p, int px, int py)
{
    return px >= p.x && px < p.x + p.w && py >= p.y && py < p.y + p.h;
}

// The tap-vs-drag classifier used by both the launcher and the pause menu.
struct Gesture
{
    bool active = false, dragging = false;
    int startX = 0, startY = 0, x = 0, y = 0;
    int finger = -1;
    int page = -1;

    void Begin(int px, int py, int f, int pg)
    {
        active = true;
        dragging = false;
        startX = x = px;
        startY = y = py;
        finger = f;
        page = pg;
    }

    bool Move(int px, int py, int threshold)
    {
        x = px;
        y = py;
        if (std::abs(y - startY) > threshold || std::abs(x - startX) > threshold) dragging = true;
        return dragging;
    }

    // True when the release was a tap (no movement beyond the threshold).
    bool End()
    {
        const bool tap = active && !dragging;
        active = false;
        return tap;
    }
};
} // namespace UiInput
