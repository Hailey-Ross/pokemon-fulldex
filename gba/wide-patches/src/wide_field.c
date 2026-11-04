// Widescreen: overworld BGs and the field camera.
// Each Wide_X replaces the game function X (tools/mkips.py writes the jumps).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "wide.h"
#include "game.h"
#include "bg.h"
#include "gpu_regs.h"
#include "malloc.h"
#include "field_camera.h"
#include "overworld.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sprite.h"
#if defined(WIDE_FIRERED)
#include "new_menu_helpers.h"
#include "field_message_box.h"
#else
#include "menu.h"
#endif

struct FieldCameraOffset
{
    u8 xPixelOffset;
    u8 yPixelOffset;
    u8 xTileOffset;
    u8 yTileOffset;
    bool8 copyBGToVRAM;
};

// Game statics (addresses from the version file)
extern struct FieldCameraOffset sFieldCameraOffset;
extern s16 sHorizontalCameraPan;
extern s16 sVerticalCameraPan;
extern const struct BgTemplate sOverworldBgTemplates[4];
void DrawMetatileAt(const struct MapLayout *mapLayout, u16 offset, int x, int y);
void DrawMetatile(s32 layerType, const u16 *tiles, u16 offset);
void RedrawMapSliceNorth(struct FieldCameraOffset *cameraOffset, const struct MapLayout *mapLayout);
void RedrawMapSliceSouth(struct FieldCameraOffset *cameraOffset, const struct MapLayout *mapLayout);
void RedrawMapSliceEast(struct FieldCameraOffset *cameraOffset, const struct MapLayout *mapLayout);
void RedrawMapSliceWest(struct FieldCameraOffset *cameraOffset, const struct MapLayout *mapLayout);
void TransferTilesetAnimsBuffer(void);
#if defined(WIDE_FIRERED)
void MoveSaveBlocks_ResetHeap_(void);
void ResetScreenForMapLoad(void);
#endif

// BG1-3 as 64x32 maps (screen blocks 25, 27, 29); BG0 (text) as the game has it.
static const struct BgTemplate sWideBgTemplates[] = {
    { .bg = 0, .charBaseIndex = 2, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
    { .bg = 1, .charBaseIndex = 0, .mapBaseIndex = 25, .screenSize = 1, .paletteMode = 0, .priority = 1, .baseTile = 0 },
    { .bg = 2, .charBaseIndex = 0, .mapBaseIndex = 27, .screenSize = 1, .paletteMode = 0, .priority = 2, .baseTile = 0 },
    { .bg = 3, .charBaseIndex = 0, .mapBaseIndex = 29, .screenSize = 1, .paletteMode = 0, .priority = 3, .baseTile = 0 },
};

static void InitFieldBgs(void)
{
    u32 size = BG_SCREEN_SIZE;

    gWide.mapWide = WideOffer() != 0;
    if (gWide.mapWide)
    {
        InitBgsFromTemplates(0, sWideBgTemplates, NELEMS(sWideBgTemplates));
        size *= 2;
    }
    else
    {
        InitBgsFromTemplates(0, sOverworldBgTemplates, 4);
    }
    SetBgAttribute(1, BG_ATTR_MOSAIC, TRUE);
    SetBgAttribute(2, BG_ATTR_MOSAIC, TRUE);
    SetBgAttribute(3, BG_ATTR_MOSAIC, TRUE);
    TILEMAP_BG1 = AllocZeroed(size);
    TILEMAP_BG2 = AllocZeroed(size);
    TILEMAP_BG3 = AllocZeroed(size);
    SetBgTilemapBuffer(1, TILEMAP_BG1);
    SetBgTilemapBuffer(2, TILEMAP_BG2);
    SetBgTilemapBuffer(3, TILEMAP_BG3);
    InitStandardTextBoxWindows();
#if defined(WIDE_FIRERED)
    InitTextBoxGfxAndPrinters();
    InitFieldMessageBox();
#endif
}

#if defined(WIDE_FIRERED)
void Wide_InitOverworldBgs(void)
{
    MoveSaveBlocks_ResetHeap_();
    ResetScreenForMapLoad();
    ResetBgsAndClearDma3BusyFlags(FALSE);
    InitFieldBgs();
}

void Wide_InitOverworldBgs_NoResetHeap(void)
{
    ResetBgsAndClearDma3BusyFlags(FALSE);
    InitFieldBgs();
}
#else
void Wide_InitOverworldBgs(void)
{
    InitFieldBgs();
}
#endif

void Wide_VBlankCB_Field(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    ScanlineEffect_InitHBlankDmaTransfer();
    FieldUpdateBgTilemapScroll();
    TransferPlttBuffer();
    TransferTilesetAnimsBuffer();
    if (WideMapAmount())
        WIDE_SIGNAL_REG = WIDE_SIGNAL(WIDE_FIELD_MASK);
}

// Camera: a wide map is 64 tiles (512 px) across, so x wraps at 64 tiles and keeps a 9-bit pixel offset.
static void ResetCamera(struct FieldCameraOffset *cameraOffset)
{
    gWide.cameraX = 0;
    cameraOffset->xTileOffset = 0;
    cameraOffset->yTileOffset = 0;
    cameraOffset->xPixelOffset = 0;
    cameraOffset->yPixelOffset = 0;
    cameraOffset->copyBGToVRAM = TRUE;
}

static void AddTileOffset(struct FieldCameraOffset *cameraOffset, u32 x, u32 y)
{
    cameraOffset->xTileOffset += x;
    cameraOffset->xTileOffset %= gWide.mapWide ? 64 : 32;
    cameraOffset->yTileOffset += y;
    cameraOffset->yTileOffset %= 32;
}

static void AddPixelOffset(struct FieldCameraOffset *cameraOffset, u32 x, u32 y)
{
    gWide.cameraX = (gWide.cameraX + x) & 0x1FF;
    cameraOffset->xPixelOffset += x;
    cameraOffset->yPixelOffset += y;
}

static u16 CameraHofs(void)
{
    if (gWide.mapWide)
        return (gWide.cameraX + sHorizontalCameraPan) & 0x1FF;
    return sFieldCameraOffset.xPixelOffset + sHorizontalCameraPan;
}

static void GetPanOffset(s16 *x, s16 *y)
{
    *x = CameraHofs();
    *y = sFieldCameraOffset.yPixelOffset + sVerticalCameraPan + 8;
}

#if defined(WIDE_FIRERED)
void Wide_move_tilemap_camera_to_upper_left_corner_(struct FieldCameraOffset *c) { ResetCamera(c); }
void Wide_tilemap_move_something(struct FieldCameraOffset *c, u32 x, u32 y) { AddTileOffset(c, x, y); }
void Wide_coords8_add(struct FieldCameraOffset *c, u32 x, u32 y) { AddPixelOffset(c, x, y); }
void Wide_FieldCameraGetPixelOffsetAtGround(s16 *x, s16 *y) { GetPanOffset(x, y); }
#else
void Wide_ResetCameraOffset(struct FieldCameraOffset *c) { ResetCamera(c); }
void Wide_AddCameraTileOffset(struct FieldCameraOffset *c, u32 x, u32 y) { AddTileOffset(c, x, y); }
void Wide_AddCameraPixelOffset(struct FieldCameraOffset *c, u32 x, u32 y) { AddPixelOffset(c, x, y); }
void Wide_GetCameraOffsetWithPan(s16 *x, s16 *y) { GetPanOffset(x, y); }
#endif

void Wide_FieldUpdateBgTilemapScroll(void)
{
    s16 x, y;

    GetPanOffset(&x, &y);
    SetGpuReg(REG_OFFSET_BG1HOFS, x);
    SetGpuReg(REG_OFFSET_BG1VOFS, y);
    SetGpuReg(REG_OFFSET_BG2HOFS, x);
    SetGpuReg(REG_OFFSET_BG2VOFS, y);
    SetGpuReg(REG_OFFSET_BG3HOFS, x);
    SetGpuReg(REG_OFFSET_BG3VOFS, y);
}

// Tilemap entry for view metatile (col, row); col may be -8..23. The 64x32 map is two 32x32 blocks.
static u16 WideTileOffset(const struct FieldCameraOffset *cameraOffset, s32 col, s32 row)
{
    u32 x = cameraOffset->xTileOffset + col * 2;
    u32 y = (cameraOffset->yTileOffset + row * 2) & 0x1F;
    return (x & 0x20) * 32 + y * 32 + (x & 0x1F);
}

// Past the map's 7-tile border the backup map ends: read east/west neighbours from their own layouts.
static bool8 GetConnectedBlock(s32 x, s32 y, u16 *block)
{
    const struct MapConnections *connections = gMapHeader.connections;
    s32 i;

    if (connections == NULL || y < 0 || y >= BACKUP_MAP_H || (x >= 0 && x < BACKUP_MAP_W))
        return FALSE;
    for (i = 0; i < connections->count; i++)
    {
        const struct MapConnection *connection = &connections->connections[i];
        const struct MapHeader *header;
        const struct MapLayout *layout;
        s32 lx, ly;

        if (connection->direction != (x < 0 ? CONNECTION_WEST : CONNECTION_EAST))
            continue;
        header = GetMapHeaderFromConnection(connection);
        if (header == NULL)
            continue;
        layout = header->mapLayout;
        if (x < 0)
            lx = x + layout->width - MAP_OFFSET;
        else
            lx = x - MAP_OFFSET - gMapHeader.mapLayout->width;
        ly = y - MAP_OFFSET - (s32)connection->offset;
        if (lx >= 0 && lx < layout->width && ly >= 0 && ly < layout->height)
        {
            *block = layout->map[lx + layout->width * ly];
            return TRUE;
        }
    }
    return FALSE;
}

static void DrawWideMetatileAt(const struct MapLayout *mapLayout, u16 offset, s32 x, s32 y)
{
    const u16 *metatiles;
    u16 block, id, metatileId;

    if (!GetConnectedBlock(x, y, &block))
    {
        DrawMetatileAt(mapLayout, offset, x, y);
        return;
    }
    id = metatileId = block & MAPGRID_METATILE_ID_MASK;
    if (metatileId > NUM_METATILES_TOTAL)
        id = metatileId = 0;
    if (metatileId < NUM_METATILES_IN_PRIMARY)
    {
        metatiles = mapLayout->primaryTileset->metatiles;
    }
    else
    {
        metatiles = mapLayout->secondaryTileset->metatiles;
        metatileId -= NUM_METATILES_IN_PRIMARY;
    }
    DrawMetatile(LayerTypeOf(id), metatiles + metatileId * NUM_TILES_PER_METATILE, offset);
}

static void DrawWideColumn(const struct MapLayout *mapLayout, s32 x, s32 y, s32 col)
{
    s32 row;

    for (row = 0; row < 16; row++)
        DrawWideMetatileAt(mapLayout, WideTileOffset(&sFieldCameraOffset, col, row), x + col, y + row);
}

static void DrawWideRow(const struct MapLayout *mapLayout, s32 x, s32 y, s32 row)
{
    s32 col;

    for (col = -WIDE_MAP_LEFT; col < WIDE_MAP_COLS - WIDE_MAP_LEFT; col++)
        DrawWideMetatileAt(mapLayout, WideTileOffset(&sFieldCameraOffset, col, row), x + col, y + row);
}

void Wide_DrawWholeMapViewInternal(int x, int y, const struct MapLayout *mapLayout)
{
    u8 i, j, temp;
    s32 row;

    if (gWide.mapWide)
    {
        for (row = 0; row < 16; row++)
            DrawWideRow(mapLayout, x, y, row);
        return;
    }
    for (i = 0; i < 32; i += 2)
    {
        u32 r6;

        temp = sFieldCameraOffset.yTileOffset + i;
        if (temp >= 32)
            temp -= 32;
        r6 = temp * 32;
        for (j = 0; j < 32; j += 2)
        {
            temp = sFieldCameraOffset.xTileOffset + j;
            if (temp >= 32)
                temp -= 32;
            DrawMetatileAt(mapLayout, r6 + temp, x + j / 2, y + i / 2);
        }
    }
}

void Wide_RedrawMapSlicesForCameraUpdate(struct FieldCameraOffset *cameraOffset, int x, int y)
{
    const struct MapLayout *mapLayout = gMapHeader.mapLayout;
    s32 px = gSaveBlock1Ptr->pos.x, py = gSaveBlock1Ptr->pos.y;

    if (gWide.mapWide)
    {
        if (x > 0)
            DrawWideColumn(mapLayout, px, py, WIDE_MAP_COLS - WIDE_MAP_LEFT - 1);
        if (x < 0)
            DrawWideColumn(mapLayout, px, py, -WIDE_MAP_LEFT);
        if (y > 0)
            DrawWideRow(mapLayout, px, py, 14);
        if (y < 0)
            DrawWideRow(mapLayout, px, py, 0);
    }
    else
    {
        if (x > 0)
            RedrawMapSliceWest(cameraOffset, mapLayout);
        if (x < 0)
            RedrawMapSliceEast(cameraOffset, mapLayout);
        if (y > 0)
            RedrawMapSliceNorth(cameraOffset, mapLayout);
        if (y < 0)
            RedrawMapSliceSouth(cameraOffset, mapLayout);
    }
    cameraOffset->copyBGToVRAM = TRUE;
}

s32 Wide_MapPosToBgTilemapOffset(struct FieldCameraOffset *cameraOffset, s32 x, s32 y)
{
    x -= gSaveBlock1Ptr->pos.x;
    y -= gSaveBlock1Ptr->pos.y;
    if (gWide.mapWide)
    {
        if (x < -WIDE_MAP_LEFT || x >= WIDE_MAP_COLS - WIDE_MAP_LEFT || y < 0 || y >= 16)
            return -1;
        return WideTileOffset(cameraOffset, x, y);
    }
    x *= 2;
    if (x >= 32 || x < 0)
        return -1;
    x += cameraOffset->xTileOffset;
    if (x >= 32)
        x -= 32;
    y *= 2;
    if (y >= 32 || y < 0)
        return -1;
    y += cameraOffset->yTileOffset;
    if (y >= 32)
        y -= 32;
    return y * 32 + x;
}
