// Force-included before PokeDaisy's EXPAND (third_party/pokedaisy): it is written for mGBA 0.10.5, ours is newer.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <mgba/core/core.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/memory.h>

// (Windows headers, through mGBA's, make these empty macros; EXPAND has variables of these names.)
#undef near
#undef far

// mGBA 0.10.5's names for the GBA memory map
#define BASE_CART0 GBA_BASE_ROM0
#define BASE_CART_SRAM GBA_BASE_SRAM
#define BASE_PALETTE_RAM GBA_BASE_PALETTE_RAM
#define SIZE_CART0 GBA_SIZE_ROM0
#define SIZE_WORKING_RAM GBA_SIZE_EWRAM
