// Widescreen: weather sprites spread over the wider picture.
// Fog, ash and sandstorm are rows of 64 px sprites in a ring; wide adds columns to the ring.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "wide.h"
#include "game.h"
#include "field_weather.h"
#include "random.h"
#include "sprite.h"
#include "trig.h"

extern const s16 sRainSpriteMovement[][2];
extern const u16 sRainSpriteFallingDurations[][2];
extern const struct SpriteTemplate sFogHorizontalSpriteTemplate;
extern const struct SpriteTemplate sAshSpriteTemplate;
extern const struct SpriteSheet FOG_D_SHEET;
extern const struct SpriteTemplate sFogDiagonalSpriteTemplate;
extern const struct SpriteTemplate sSandstormSpriteTemplate;
extern const struct SpriteSheet sSandstormSpriteSheet;
extern const u16 gSandstormWeatherPalette[];
extern const u8 gWeatherFogHorizontalTiles[];
void WaitSnowflakeSprite(struct Sprite *sprite);

#define BASE_COLS 5

// Extra pixels per side for weather (only while the field map is wide).
static s32 Amount(void)
{
    return WideMapAmount();
}

// Ring columns needed to cover 240 + 2 * amount plus one sprite: 5 to 9.
static u32 RingCols(void)
{
    u32 cols = (Amount() + 183) / 32;
    return cols < BASE_COLS ? BASE_COLS : cols > 9 ? 9 : cols;
}

// X of a ring sprite in column col, scrolled by baseX.
static s16 RingX(s32 baseX, s32 col, u32 kind)
{
    s32 amount = Amount();
    s32 x;

    if (amount == 0)
    {
        if (col >= BASE_COLS)
            return DISPLAY_WIDTH + 64; // extra column left over from a wide moment: park it
        x = baseX + col * 64 + 32;
        if (x >= DISPLAY_WIDTH + 32)
            x = (baseX + DISPLAY_WIDTH * 2 - (4 - col) * 64) & 0x1FF;
        return x;
    }
    else
    {
        s32 ring = gWide.weatherCols[kind] * 64;
        s32 left = -63 - amount;

        x = (baseX + col * 64 - left) % ring;
        if (x < 0)
            x += ring;
        return (x + left + 32) & 0x1FF;
    }
}

typedef void (*RingSetup)(struct Sprite *sprite, u32 col, u32 row);

static void CreateRingExtras(u32 kind, const struct SpriteTemplate *template, bool32 rowY, u8 subpriority, RingSetup setup)
{
    u32 extraCols, i;

    gWide.weatherCols[kind] = RingCols();
    extraCols = gWide.weatherCols[kind] - BASE_COLS;
    for (i = 0; i < 16; i++)
        gWide.weatherSprites[kind][i] = WIDE_NONE;
    for (i = 0; i < extraCols * 4; i++)
    {
        u32 col = BASE_COLS + i % extraCols, row = i / extraCols;
        u8 spriteId = CreateSpriteAtEnd(template, 0, rowY ? row * 64 : 0, subpriority);

        gWide.weatherSprites[kind][i] = spriteId;
        if (spriteId != MAX_SPRITES)
            setup(&gSprites[spriteId], col, row);
    }
}

static void DestroyRingExtras(u32 kind)
{
    u32 i;

    for (i = 0; i < 16; i++)
    {
        if (gWide.weatherSprites[kind][i] != WIDE_NONE)
            DestroySprite(&gSprites[gWide.weatherSprites[kind][i]]);
        gWide.weatherSprites[kind][i] = WIDE_NONE;
    }
}

// Rain: the 30 x 20 grid of start points is stretched over the wide picture.
#define tCounter data[0]
#define tRandom  data[1]
#define tPosX    data[2]
#define tPosY    data[3]
#define tState   data[4]
#define tActive  data[5]

void Wide_StartRainSpriteFall(struct Sprite *sprite)
{
    s32 amount = Amount();
    u32 rand;
    u16 numFallingFrames;
    s32 tileX, tileY;

    if (sprite->tRandom == 0)
        sprite->tRandom = 361;
    rand = ISO_RANDOMIZE2(sprite->tRandom);
    sprite->tRandom = ((rand & 0x7FFF0000) >> 16) % 600;
    numFallingFrames = sRainSpriteFallingDurations[gWeatherPtr->isDownpour][0];
    tileX = sprite->tRandom % 30;
    tileY = sprite->tRandom / 30;
    if (amount)
        sprite->tPosX = (tileX * (DISPLAY_WIDTH + amount * 2) / 30 - amount) << 4;
    else
        sprite->tPosX = tileX << 7;
    sprite->tPosY = tileY << 7;
    sprite->tPosX -= sRainSpriteMovement[gWeatherPtr->isDownpour][0] * numFallingFrames;
    sprite->tPosY -= sRainSpriteMovement[gWeatherPtr->isDownpour][1] * numFallingFrames;
    StartSpriteAnim(sprite, 0);
    sprite->tState = 0;
    sprite->coordOffsetEnabled = FALSE;
    sprite->tCounter = numFallingFrames;
}

void Wide_UpdateRainSprite(struct Sprite *sprite)
{
    if (sprite->tState == 0)
    {
        s32 amount = Amount();

        sprite->tPosX += sRainSpriteMovement[gWeatherPtr->isDownpour][0];
        sprite->tPosY += sRainSpriteMovement[gWeatherPtr->isDownpour][1];
        sprite->x = sprite->tPosX >> 4;
        sprite->y = sprite->tPosY >> 4;
        if (sprite->tActive
            && sprite->x >= -8 - amount && sprite->x <= DISPLAY_WIDTH + 8 + amount
            && sprite->y >= -16 && sprite->y <= DISPLAY_HEIGHT + 16)
            sprite->invisible = FALSE;
        else
            sprite->invisible = TRUE;
        if (--sprite->tCounter == 0)
        {
            StartSpriteAnim(sprite, gWeatherPtr->isDownpour + 1);
            sprite->tState = 1;
            sprite->x -= gSpriteCoordOffsetX;
            sprite->y -= gSpriteCoordOffsetY;
            sprite->coordOffsetEnabled = TRUE;
        }
    }
    else if (sprite->animEnded)
    {
        sprite->invisible = TRUE;
        Wide_StartRainSpriteFall(sprite);
    }
}

#undef tCounter
#undef tRandom
#undef tPosX
#undef tPosY
#undef tState
#undef tActive

// Snow
#define tPosY         data[0]
#define tDeltaY       data[1]
#define tWaveDelta    data[2]
#define tWaveIndex    data[3]
#define tSnowflakeId  data[4]
#define tFallCounter  data[5]
#define tFallDuration data[6]
#define tDeltaY2      data[7]

void Wide_InitSnowflakeSpriteMovement(struct Sprite *sprite)
{
    s32 amount = Amount();
    u16 rand;
    s32 x = ((sprite->tSnowflakeId * 5) & 7) * 30 + (Random() % 30);

    if (amount)
        x = x * (DISPLAY_WIDTH + amount * 2) / DISPLAY_WIDTH - amount;
    sprite->y = -3 - (gSpriteCoordOffsetY + sprite->centerToCornerVecY);
    sprite->x = x - (gSpriteCoordOffsetX + sprite->centerToCornerVecX);
    sprite->tPosY = sprite->y * 128;
    sprite->x2 = 0;
    rand = Random();
    sprite->tDeltaY = (rand & 3) * 5 + 64;
    sprite->tDeltaY2 = sprite->tDeltaY;
    StartSpriteAnim(sprite, (rand & 1) ? 0 : 1);
    sprite->tWaveIndex = 0;
    sprite->tWaveDelta = ((rand & 3) == 0) ? 2 : 1;
    sprite->tFallDuration = (rand & 0x1F) + 210;
    sprite->tFallCounter = 0;
}

void Wide_UpdateSnowflakeSprite(struct Sprite *sprite)
{
    s32 amount = Amount();
    s16 x, y;

    sprite->tPosY += sprite->tDeltaY;
    sprite->y = sprite->tPosY >> 7;
    sprite->tWaveIndex += sprite->tWaveDelta;
    sprite->tWaveIndex &= 0xFF;
    sprite->x2 = gSineTable[sprite->tWaveIndex] / 64;

    x = (sprite->x + sprite->centerToCornerVecX + gSpriteCoordOffsetX) & 0x1FF;
    if (amount == 0 ? (x & 0x100) : x >= 256 + amount)
        x -= 0x200;
    if (x < -3 - amount)
        sprite->x = 242 + amount - (gSpriteCoordOffsetX + sprite->centerToCornerVecX);
    else if (x > 242 + amount)
        sprite->x = -3 - amount - (gSpriteCoordOffsetX + sprite->centerToCornerVecX);

    y = (sprite->y + sprite->centerToCornerVecY + gSpriteCoordOffsetY) & 0xFF;
    if (y > 163 && y < 171)
    {
        sprite->y = 250 - (gSpriteCoordOffsetY + sprite->centerToCornerVecY);
        sprite->tPosY = sprite->y * 128;
        sprite->tFallCounter = 0;
        sprite->tFallDuration = 220;
    }
    else if (y > 242 && y < 250)
    {
        sprite->y = 163;
        sprite->tPosY = sprite->y * 128;
        sprite->tFallCounter = 0;
        sprite->tFallDuration = 220;
        sprite->invisible = TRUE;
        sprite->callback = WaitSnowflakeSprite;
    }
    if (++sprite->tFallCounter == sprite->tFallDuration)
    {
        Wide_InitSnowflakeSpriteMovement(sprite);
        sprite->y = 250;
        sprite->invisible = TRUE;
        sprite->callback = WaitSnowflakeSprite;
    }
}

#undef tPosY
#undef tDeltaY
#undef tWaveDelta
#undef tWaveIndex
#undef tSnowflakeId
#undef tFallCounter
#undef tFallDuration
#undef tDeltaY2

// Horizontal fog
#define tSpriteColumn data[0]

static void SetupFogH(struct Sprite *sprite, u32 col, u32 row)
{
    sprite->tSpriteColumn = col;
    sprite->x = col * 64 + 32;
    sprite->y = row * 64 + 32;
}

void Wide_FogHorizontalSpriteCallback(struct Sprite *sprite)
{
    sprite->y2 = (u8)gSpriteCoordOffsetY;
    sprite->x = RingX(gWeatherPtr->fogHScrollPosX, sprite->tSpriteColumn, WIDE_FOG_H);
}

void Wide_CreateFogHorizontalSprites(void)
{
    u16 i;
    u8 spriteId;

    if (gWeatherPtr->fogHSpritesCreated)
        return;
    {
        struct SpriteSheet sheet = { .data = gWeatherFogHorizontalTiles, .size = 0x0800, .tag = GFXTAG_FOG_H };
        LoadSpriteSheet(&sheet);
    }
    for (i = 0; i < NUM_FOG_HORIZONTAL_SPRITES; i++)
    {
        spriteId = CreateSpriteAtEnd(&sFogHorizontalSpriteTemplate, 0, 0, 0xFF);
        if (spriteId != MAX_SPRITES)
        {
            SetupFogH(&gSprites[spriteId], i % 5, i / 5);
            gWeatherPtr->sprites.s2.fogHSprites[i] = &gSprites[spriteId];
        }
        else
        {
            gWeatherPtr->sprites.s2.fogHSprites[i] = NULL;
        }
    }
    CreateRingExtras(WIDE_FOG_H, &sFogHorizontalSpriteTemplate, FALSE, 0xFF, SetupFogH);
    gWeatherPtr->fogHSpritesCreated = TRUE;
}

void Wide_DestroyFogHorizontalSprites(void)
{
    u16 i;

    if (!gWeatherPtr->fogHSpritesCreated)
        return;
    for (i = 0; i < NUM_FOG_HORIZONTAL_SPRITES; i++)
    {
        if (gWeatherPtr->sprites.s2.fogHSprites[i] != NULL)
            DestroySprite(gWeatherPtr->sprites.s2.fogHSprites[i]);
    }
    DestroyRingExtras(WIDE_FOG_H);
    FreeSpriteTilesByTag(GFXTAG_FOG_H);
    gWeatherPtr->fogHSpritesCreated = FALSE;
}

#undef tSpriteColumn

// Volcanic ash
#define tOffsetY      data[0]
#define tCounterY     data[1]
#define tSpriteColumn data[2]
#define tSpriteRow    data[3]

static void SetupAsh(struct Sprite *sprite, u32 col, u32 row)
{
    sprite->tCounterY = 0;
    sprite->tSpriteColumn = (u8)col;
    sprite->tSpriteRow = (u8)row;
    sprite->tOffsetY = sprite->tSpriteRow * 64 + 32;
}

void Wide_CreateAshSprites(void)
{
    u8 i, spriteId;

    if (gWeatherPtr->ashSpritesCreated)
        return;
    for (i = 0; i < NUM_ASH_SPRITES; i++)
    {
        spriteId = CreateSpriteAtEnd(&sAshSpriteTemplate, 0, 0, 0x4E);
        if (spriteId != MAX_SPRITES)
        {
            SetupAsh(&gSprites[spriteId], i % 5, i / 5);
            gWeatherPtr->sprites.s2.ashSprites[i] = &gSprites[spriteId];
        }
        else
        {
            gWeatherPtr->sprites.s2.ashSprites[i] = NULL;
        }
    }
    CreateRingExtras(WIDE_ASH, &sAshSpriteTemplate, FALSE, 0x4E, SetupAsh);
    gWeatherPtr->ashSpritesCreated = TRUE;
}

void Wide_DestroyAshSprites(void)
{
    u16 i;

    if (!gWeatherPtr->ashSpritesCreated)
        return;
    for (i = 0; i < NUM_ASH_SPRITES; i++)
    {
        if (gWeatherPtr->sprites.s2.ashSprites[i] != NULL)
            DestroySprite(gWeatherPtr->sprites.s2.ashSprites[i]);
    }
    DestroyRingExtras(WIDE_ASH);
    FreeSpriteTilesByTag(GFXTAG_ASH);
    gWeatherPtr->ashSpritesCreated = FALSE;
}

void Wide_UpdateAshSprite(struct Sprite *sprite)
{
    if (++sprite->tCounterY > 5)
    {
        sprite->tCounterY = 0;
        sprite->tOffsetY++;
    }
    sprite->y = gSpriteCoordOffsetY + sprite->tOffsetY;
    sprite->x = RingX(gWeatherPtr->ashBaseSpritesX, sprite->tSpriteColumn, WIDE_ASH);
}

#undef tOffsetY
#undef tCounterY
#undef tSpriteColumn
#undef tSpriteRow

// Diagonal fog and sandstorm share their column/row data
#define tSpriteColumn data[0]
#define tSpriteRow    data[1]

static void SetupColRow(struct Sprite *sprite, u32 col, u32 row)
{
    sprite->tSpriteColumn = col;
    sprite->tSpriteRow = row;
}

void Wide_CreateFogDiagonalSprites(void)
{
    u16 i;
    u8 spriteId;
    struct SpriteSheet sheet;

    if (gWeatherPtr->fogDSpritesCreated)
        return;
    sheet = FOG_D_SHEET;
    LoadSpriteSheet(&sheet);
    for (i = 0; i < NUM_FOG_DIAGONAL_SPRITES; i++)
    {
        spriteId = CreateSpriteAtEnd(&sFogDiagonalSpriteTemplate, 0, (i / 5) * 64, 0xFF);
        if (spriteId != MAX_SPRITES)
        {
            SetupColRow(&gSprites[spriteId], i % 5, i / 5);
            gWeatherPtr->sprites.s2.fogDSprites[i] = &gSprites[spriteId];
        }
        else
        {
            gWeatherPtr->sprites.s2.fogDSprites[i] = NULL;
        }
    }
    CreateRingExtras(WIDE_FOG_D, &sFogDiagonalSpriteTemplate, TRUE, 0xFF, SetupColRow);
    gWeatherPtr->fogDSpritesCreated = TRUE;
}

void Wide_DestroyFogDiagonalSprites(void)
{
    u16 i;

    if (!gWeatherPtr->fogDSpritesCreated)
        return;
    for (i = 0; i < NUM_FOG_DIAGONAL_SPRITES; i++)
    {
        if (gWeatherPtr->sprites.s2.fogDSprites[i])
            DestroySprite(gWeatherPtr->sprites.s2.fogDSprites[i]);
    }
    DestroyRingExtras(WIDE_FOG_D);
    FreeSpriteTilesByTag(GFXTAG_FOG_D);
    gWeatherPtr->fogDSpritesCreated = FALSE;
}

void Wide_UpdateFogDiagonalSprite(struct Sprite *sprite)
{
    sprite->y2 = gWeatherPtr->fogDPosY;
    sprite->x = RingX(gWeatherPtr->fogDBaseSpritesX, sprite->tSpriteColumn, WIDE_FOG_D);
}

void Wide_CreateSandstormSprites(void)
{
    u16 i;
    u8 spriteId;

    if (gWeatherPtr->sandstormSpritesCreated)
        return;
    LoadSpriteSheet(&sSandstormSpriteSheet);
    LoadCustomWeatherSpritePalette(gSandstormWeatherPalette);
    for (i = 0; i < NUM_SANDSTORM_SPRITES; i++)
    {
        spriteId = CreateSpriteAtEnd(&sSandstormSpriteTemplate, 0, (i / 5) * 64, 1);
        if (spriteId != MAX_SPRITES)
        {
            gWeatherPtr->sprites.s2.sandstormSprites1[i] = &gSprites[spriteId];
            SetupColRow(&gSprites[spriteId], i % 5, i / 5);
        }
        else
        {
            gWeatherPtr->sprites.s2.sandstormSprites1[i] = NULL;
        }
    }
    CreateRingExtras(WIDE_SAND, &sSandstormSpriteTemplate, TRUE, 1, SetupColRow);
    gWeatherPtr->sandstormSpritesCreated = TRUE;
}

void Wide_DestroySandstormSprites(void)
{
    u16 i;

    if (gWeatherPtr->sandstormSpritesCreated)
    {
        for (i = 0; i < NUM_SANDSTORM_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.sandstormSprites1[i])
                DestroySprite(gWeatherPtr->sprites.s2.sandstormSprites1[i]);
        }
        DestroyRingExtras(WIDE_SAND);
        gWeatherPtr->sandstormSpritesCreated = FALSE;
        FreeSpriteTilesByTag(GFXTAG_SANDSTORM);
    }
    if (gWeatherPtr->sandstormSwirlSpritesCreated)
    {
        for (i = 0; i < NUM_SWIRL_SANDSTORM_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.sandstormSprites2[i] != NULL)
                DestroySprite(gWeatherPtr->sprites.s2.sandstormSprites2[i]);
        }
        gWeatherPtr->sandstormSwirlSpritesCreated = FALSE;
    }
}

void Wide_UpdateSandstormSprite(struct Sprite *sprite)
{
    sprite->y2 = gWeatherPtr->sandstormPosY;
    sprite->x = RingX(gWeatherPtr->sandstormBaseSpritesX, sprite->tSpriteColumn, WIDE_SAND);
}
