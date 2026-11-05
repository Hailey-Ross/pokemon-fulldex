// Widescreen: object events seen and spawned across the wider view, and battles.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "wide.h"
#include "game.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "gpu_regs.h"
#include "palette.h"
#include "random.h"
#include "scanline_effect.h"
#include "sprite.h"
#if defined(WIDE_EMERALD)
#include "battle.h"
#include "battle_pyramid.h"
#include "trainer_hill.h"
#include "constants/battle_pyramid.h"
#include "constants/trainer_hill.h"
#endif

extern u16 gBattle_BG0_X, gBattle_BG0_Y, gBattle_BG1_X, gBattle_BG1_Y;
extern u16 gBattle_BG2_X, gBattle_BG2_Y, gBattle_BG3_X, gBattle_BG3_Y;
extern u16 gBattle_WIN0H, gBattle_WIN0V, gBattle_WIN1H, gBattle_WIN1V;
void RemoveObjectEvent(struct ObjectEvent *objectEvent);
u8 TrySpawnObjectEventTemplate(const struct ObjectEventTemplate *template, u8 mapNum, u8 mapGroup, s16 cameraX, s16 cameraY);

// Metatiles past each side of the 15-wide view that the wide picture shows.
static s16 WideViewTiles(void)
{
    return (WideMapAmount() + 15) / 16;
}

static u8 ObjectTemplateCount(void)
{
#if defined(WIDE_EMERALD)
    if (CurrentBattlePyramidLocation() != PYRAMID_LOCATION_NONE)
        return GetNumBattlePyramidObjectEvents();
    if (InTrainerHill())
        return HILL_TRAINERS_PER_FLOOR;
#endif
    return gMapHeader.events->objectEventCount;
}

void Wide_TrySpawnObjectEvents(s16 cameraX, s16 cameraY)
{
    u8 i, objectCount;
    s16 extra, left, right, top, bottom;

    if (gMapHeader.events == NULL)
        return;
    extra = WideViewTiles();
    left = gSaveBlock1Ptr->pos.x - 2 - extra;
    right = gSaveBlock1Ptr->pos.x + MAP_OFFSET_W + 2 + extra;
    top = gSaveBlock1Ptr->pos.y;
    bottom = gSaveBlock1Ptr->pos.y + MAP_OFFSET_H + 2;
    objectCount = ObjectTemplateCount();
    for (i = 0; i < objectCount; i++)
    {
        struct ObjectEventTemplate *template = &gSaveBlock1Ptr->objectEventTemplates[i];
        s16 npcX = template->x + MAP_OFFSET;
        s16 npcY = template->y + MAP_OFFSET;

        if (top <= npcY && bottom >= npcY && left <= npcX && right >= npcX && !FlagGet(template->flagId))
            TrySpawnObjectEventTemplate(template, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup, cameraX, cameraY);
    }
}

void Wide_RemoveObjectEventIfOutsideView(struct ObjectEvent *objectEvent)
{
    s16 extra = WideViewTiles();
    s16 left = gSaveBlock1Ptr->pos.x - 2 - extra;
    s16 right = gSaveBlock1Ptr->pos.x + MAP_OFFSET_W + 2 + extra;
    s16 top = gSaveBlock1Ptr->pos.y;
    s16 bottom = gSaveBlock1Ptr->pos.y + MAP_OFFSET_H + 2;

    if (objectEvent->currentCoords.x >= left && objectEvent->currentCoords.x <= right
     && objectEvent->currentCoords.y >= top && objectEvent->currentCoords.y <= bottom)
        return;
    if (objectEvent->initialCoords.x >= left && objectEvent->initialCoords.x <= right
     && objectEvent->initialCoords.y >= top && objectEvent->initialCoords.y <= bottom)
        return;
    RemoveObjectEvent(objectEvent);
}

static void SpriteCorner(struct Sprite *sprite, s16 *x, s16 *y)
{
    *x = sprite->x + sprite->x2 + sprite->centerToCornerVecX;
    *y = sprite->y + sprite->y2 + sprite->centerToCornerVecY;
    if (sprite->coordOffsetEnabled)
    {
        *x += gSpriteCoordOffsetX;
        *y += gSpriteCoordOffsetY;
    }
}

static void UpdateOffscreen(struct ObjectEvent *objectEvent, struct Sprite *sprite, s16 minX)
{
    s16 extra = WideMapAmount();
    const struct ObjectEventGraphicsInfo *graphicsInfo;
    s16 x, y;

    objectEvent->offScreen = FALSE;
    graphicsInfo = GetObjectEventGraphicsInfo(objectEvent->graphicsId);
    SpriteCorner(sprite, &x, &y);
    if (x >= DISPLAY_WIDTH + 16 + extra || (s16)(x + graphicsInfo->width) < minX - extra)
        objectEvent->offScreen = TRUE;
    if (y >= DISPLAY_HEIGHT + 16 || (s16)(y + graphicsInfo->height) < -16)
        objectEvent->offScreen = TRUE;
}

#if defined(WIDE_FIRERED)
#define SS_ANNE_EXTERIOR 0x0401 // location: group 1, map 4
#define LOCALID_SS_ANNE  1

void Wide_CalcWhetherObjectIsOffscreen(struct ObjectEvent *objectEvent, struct Sprite *sprite)
{
    bool32 ssAnne = *(u16 *)&gSaveBlock1Ptr->location == SS_ANNE_EXTERIOR && objectEvent->localId == LOCALID_SS_ANNE;
    UpdateOffscreen(objectEvent, sprite, ssAnne ? -32 : -16);
}
#else
void Wide_UpdateObjectEventOffscreen(struct ObjectEvent *objectEvent, struct Sprite *sprite)
{
    UpdateOffscreen(objectEvent, sprite, -16);
}
#endif

void Wide_UpdateObjectEventSpriteInvisibility(struct Sprite *sprite, bool8 invisible)
{
    s16 extra = WideMapAmount();
    s16 x, y;

    sprite->invisible = invisible;
    SpriteCorner(sprite, &x, &y);
    if (x >= DISPLAY_WIDTH + 16 + extra || (s16)(x - (sprite->centerToCornerVecX >> 1)) < -16 - extra)
        sprite->invisible = TRUE;
    if (y >= DISPLAY_HEIGHT + 16 || (s16)(y - (sprite->centerToCornerVecY >> 1)) < -16)
        sprite->invisible = TRUE;
}

void Wide_VBlankCB_Battle(void)
{
#if defined(WIDE_EMERALD)
    if (!(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_RECORDED)))
        Random();
#else
    Random();
#endif
    SetGpuReg(REG_OFFSET_BG0HOFS, gBattle_BG0_X);
    SetGpuReg(REG_OFFSET_BG0VOFS, gBattle_BG0_Y);
    SetGpuReg(REG_OFFSET_BG1HOFS, gBattle_BG1_X);
    SetGpuReg(REG_OFFSET_BG1VOFS, gBattle_BG1_Y);
    SetGpuReg(REG_OFFSET_BG2HOFS, gBattle_BG2_X);
    SetGpuReg(REG_OFFSET_BG2VOFS, gBattle_BG2_Y);
    SetGpuReg(REG_OFFSET_BG3HOFS, gBattle_BG3_X);
    SetGpuReg(REG_OFFSET_BG3VOFS, gBattle_BG3_Y);
    SetGpuReg(REG_OFFSET_WIN0H, gBattle_WIN0H);
    SetGpuReg(REG_OFFSET_WIN0V, gBattle_WIN0V);
    SetGpuReg(REG_OFFSET_WIN1H, gBattle_WIN1H);
    SetGpuReg(REG_OFFSET_WIN1V, gBattle_WIN1V);
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
    ScanlineEffect_InitHBlankDmaTransfer();
    if (WideOffer())
        WIDE_SIGNAL_REG = WIDE_SIGNAL(WIDE_BATTLE_MASK);
}
