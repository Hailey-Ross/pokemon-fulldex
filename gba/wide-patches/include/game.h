// Names that differ between the FireRed/LeafGreen and Emerald decompilations.
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GUARD_WIDE_GAME_H
#define GUARD_WIDE_GAME_H

#include "global.h"
#include "fieldmap.h"

#if defined(WIDE_EMERALD)
#define BACKUP_MAP        gBackupMapLayout
#define BACKUP_MAP_W      gBackupMapLayout.width
#define BACKUP_MAP_H      gBackupMapLayout.height
#define TILEMAP_BG1       gOverworldTilemapBuffer_Bg1
#define TILEMAP_BG2       gOverworldTilemapBuffer_Bg2
#define TILEMAP_BG3       gOverworldTilemapBuffer_Bg3
#define FOG_D_SHEET       sFogDiagonalSpriteSheet
#define LayerTypeOf(id)   ((GetMetatileAttributesById(id) & METATILE_ATTR_LAYER_MASK) >> METATILE_ATTR_LAYER_SHIFT)
#elif defined(WIDE_FIRERED)
#define BACKUP_MAP        VMap
#define BACKUP_MAP_W      VMap.Xsize
#define BACKUP_MAP_H      VMap.Ysize
#define TILEMAP_BG1       gBGTilemapBuffers2
#define TILEMAP_BG2       gBGTilemapBuffers1
#define TILEMAP_BG3       gBGTilemapBuffers3
#define FOG_D_SHEET       gFogDiagonalSpriteSheet
u32 GetAttributeByMetatileIdAndMapLayout(const struct MapLayout *mapLayout, u16 metatile, u8 attributeType);
#define LayerTypeOf(id)   GetAttributeByMetatileIdAndMapLayout(gMapHeader.mapLayout, id, METATILE_ATTRIBUTE_LAYER_TYPE)
#else
#error "build with -DWIDE_FIRERED or -DWIDE_EMERALD"
#endif

#endif
