#pragma once
// Official box art for the list, downloaded when first needed from libretro-thumbnails (not shipped with us).
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstddef>
#include <string>

namespace GbaWide
{

// Starts fetching the art for a list name (firered, leafgreen, emerald) that isn't cached yet; waits at most
// `waitMs` in all for this call's downloads, the rest carry on in the background.
void FetchBoxArt(const char* const* names, size_t count, int waitMs);
// The cached picture (PNG) for a list name.
bool CachedBoxArt(const std::string& name, const unsigned char*& data, size_t& size);

}
