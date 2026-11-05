// Emerald widescreen: the Match Call window. The wide map's BG1 screen block covers the window's usual tiles
// (BG0 tile 0x200 on), so on a wide map the window and the PokeNav icon use free tiles lower down.
// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(WIDE_EMERALD)
#include "wide.h"
#include "bg.h"
#include "menu.h"
#include "palette.h"
#include "task.h"
#include "window.h"

#define tWindowId   data[2]
#define tIconTaskId data[5]

extern const struct WindowTemplate sMatchCallTextWindow;
extern const u16 sMatchCallWindow_Pal[];
extern const u8 sMatchCallWindow_Gfx[];
extern const u16 sPokenavIcon_Pal[];
extern const u32 sPokenavIcon_Gfx[];
void DrawMatchCallTextBoxBorder_Internal(u32 windowId, u32 tileOffset, u32 paletteId);
void Task_SpinPokenavIcon(u8 taskId);

#define MC_WINDOW_GFX_SIZE 0x100

static u16 WindowBaseBlock(void) { return gWide.mapWide ? 0x008 : 0x200; }
static u16 WindowTiles(void)     { return gWide.mapWide ? 0x078 : 0x270; }
static u16 IconTiles(void)       { return gWide.mapWide ? 0x126 : 0x279; }

bool32 Wide_MatchCall_LoadGfx(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    struct WindowTemplate template = sMatchCallTextWindow;

    template.baseBlock = WindowBaseBlock();
    tWindowId = AddWindow(&template);
    if (tWindowId == WINDOW_NONE)
    {
        DestroyTask(taskId);
        return FALSE;
    }
    if (LoadBgTiles(0, sMatchCallWindow_Gfx, MC_WINDOW_GFX_SIZE, WindowTiles()) == 0xFFFF
     || !DecompressAndCopyTileDataToVram(0, sPokenavIcon_Gfx, 0, IconTiles(), 0))
    {
        RemoveWindow(tWindowId);
        DestroyTask(taskId);
        return FALSE;
    }
    FillWindowPixelBuffer(tWindowId, PIXEL_FILL(8));
    LoadPalette(sMatchCallWindow_Pal, BG_PLTT_ID(14), PLTT_SIZE_4BPP);
    LoadPalette(sPokenavIcon_Pal, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    ChangeBgY(0, -0x2000, BG_COORD_SET);
    return TRUE;
}

bool32 Wide_MatchCall_DrawWindow(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (FreeTempTileDataBuffersIfPossible())
        return FALSE;
    PutWindowTilemap(tWindowId);
    DrawMatchCallTextBoxBorder_Internal(tWindowId, WindowTiles(), 14);
    WriteSequenceToBgTilemapBuffer(0, (0xF << 12) | IconTiles(), 1, 15, 4, 4, 17, 1);
    tIconTaskId = CreateTask(Task_SpinPokenavIcon, 10);
    CopyWindowToVram(tWindowId, COPYWIN_GFX);
    CopyBgTilemapBufferToVram(0);
    return TRUE;
}

#undef tWindowId
#undef tIconTaskId

#define tTimer     data[0]
#define tSpinStage data[1]
#define tTileNum   data[2]

void Wide_Task_SpinPokenavIcon(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (++tTimer > 8)
    {
        tTimer = 0;
        if (++tSpinStage > 7)
            tSpinStage = 0;
        tTileNum = (tSpinStage * 16) + IconTiles();
        WriteSequenceToBgTilemapBuffer(0, tTileNum | ~0xFFF, 1, 15, 4, 4, 17, 1);
        CopyBgTilemapBufferToVram(0);
    }
}
#endif
