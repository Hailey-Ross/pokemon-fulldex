// Widescreen patches for the GBA games: shared definitions.
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GUARD_WIDE_H
#define GUARD_WIDE_H

#include "global.h"

// The emulator offers wide by answering 0x57nn at 0x0400005C (nn = extra pixels per side).
#define WIDE_OFFER_REG   (*(vu16 *)(REG_BASE + 0x5C))
// The game asks for wide each frame: 0xA500 | layers (bits 0-3 BG0-3, bit 4 OBJ, bit 5 stretch text boxes).
#define WIDE_SIGNAL_REG  (*(vu16 *)(REG_BASE + 0x5E))
#define WIDE_SIGNAL(m)   (0xA500 | (m))
#define WIDE_MAX         112
#define WIDE_FIELD_MASK  0x1E
#define WIDE_BATTLE_MASK 0x3E

#define WIDE_MAP_COLS    32 // metatiles drawn across: 8 left of the view, 15, then 9 right
#define WIDE_MAP_LEFT    8
#define WIDE_NONE        64 // MAX_SPRITES: no sprite

enum { WIDE_FOG_H, WIDE_ASH, WIDE_FOG_D, WIDE_SAND, WIDE_WEATHER_KINDS };

// Kept in free EWRAM past the game's own data (gWide comes from the version file).
struct WideState
{
    u8 weatherCols[WIDE_WEATHER_KINDS];
    u8 unused[4];
    u8 weatherSprites[WIDE_WEATHER_KINDS][16];
    bool8 mapWide;
    u8 unused2[3];
    u16 cameraX;
};
extern struct WideState gWide;

static inline u32 WideOffer(void)
{
    u32 v = WIDE_OFFER_REG;
    if ((v & 0xFF00) != 0x5700)
        return 0;
    v &= 0xFF;
    return v > WIDE_MAX ? WIDE_MAX : v;
}

// Extra pixels per side while the overworld uses the wide map.
static inline u32 WideMapAmount(void)
{
    return gWide.mapWide ? WideOffer() : 0;
}

#endif
