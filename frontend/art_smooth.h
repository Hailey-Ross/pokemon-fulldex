// The album art, drawn smoothly. The art comes at up to 512 pixels square and is drawn into a square of
// any size, so each screen pixel takes the average of the part of the picture it covers (a few samples
// across it, each blended from its four nearest pixels) instead of one nearest pixel, which was blocky.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ArtSmooth
{
// The picture's colour (opaque ARGB) for pixel (px, py) of a dst x dst square; the picture is aw x ah.
inline uint32_t At(const std::vector<uint32_t>& art, int aw, int ah, int px, int py, int dst)
{
    if (art.empty() || aw <= 0 || ah <= 0 || dst <= 0) return 0xFF000000u;
    const int n = std::max(1, std::min(3, (std::max(aw, ah) + dst - 1) / dst));
    float r = 0, g = 0, b = 0;
    auto at = [&](int x, int y) { x = std::clamp(x, 0, aw - 1); y = std::clamp(y, 0, ah - 1); return art[(size_t)y * aw + x]; };
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
        {
            const float fx = (px + (i + 0.5f) / n) * aw / dst - 0.5f, fy = (py + (j + 0.5f) / n) * ah / dst - 0.5f;
            const int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
            const float tx = fx - x0, ty = fy - y0;
            const uint32_t p00 = at(x0, y0), p10 = at(x0 + 1, y0), p01 = at(x0, y0 + 1), p11 = at(x0 + 1, y0 + 1);
            auto ch = [&](int s) {
                const float top = (float)((p00 >> s) & 255) * (1 - tx) + (float)((p10 >> s) & 255) * tx;
                const float bot = (float)((p01 >> s) & 255) * (1 - tx) + (float)((p11 >> s) & 255) * tx;
                return top * (1 - ty) + bot * ty;
            };
            r += ch(16); g += ch(8); b += ch(0);
        }
    const float k = 1.0f / (float)(n * n);
    return 0xFF000000u | ((uint32_t)(r * k + 0.5f) << 16) | ((uint32_t)(g * k + 0.5f) << 8) | (uint32_t)(b * k + 0.5f);
}
}
