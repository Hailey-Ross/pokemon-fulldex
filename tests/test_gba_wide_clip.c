#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

enum { GBA_VIDEO_HORIZONTAL_PIXELS = 240, GBA_VIDEO_WIDE_UNSEEN_COLUMNS = 16 };
struct GBAVideoSoftwareBackground { const void* wideMap; int size; };
struct GBAVideoSoftwareRenderer {
    int xOrigin, width, wideMask, start, end, wideSavedStart, wideSavedEnd;
    bool wideClipped;
    unsigned dispcnt;
    struct GBAVideoSoftwareBackground bg[4];
};
static inline unsigned GBARegisterDISPCNTGetMode(unsigned value) { return value & 7; }
// Span/restoration contract only: the full-renderer test covers real VRAM sampling.
static bool leftOpaque, rightOpaque;
static bool _wideTextTileRowOpaque(struct GBAVideoSoftwareRenderer* r,
        const struct GBAVideoSoftwareBackground* bg, int y, int x) {
    (void)r; (void)bg; (void)y;
    return x == 0 ? leftOpaque : rightOpaque;
}
#include "wide_clip_under_test.h"

static void check(struct GBAVideoSoftwareRenderer* renderer, int layer, int left, int right) {
    const int start = renderer->start, end = renderer->end;
    assert(_wideClip(renderer, layer, 80) == (left < right));
    assert(renderer->start == left && renderer->end == right);
    _wideUnclip(renderer);
    assert(renderer->start == start && renderer->end == end);
    assert(!renderer->wideClipped);
}

int main(void) {
    for (int extra = 8; extra <= 128; extra += 10) {
        struct GBAVideoSoftwareRenderer renderer = {0};
        renderer.xOrigin = extra;
        renderer.end = 240 + 2 * extra;
        renderer.width = renderer.end;
        renderer.wideMask = 0x1F;
        const int margin = extra < 16 ? extra : 16;
        for (int size = 0; size < 4; ++size) {
            renderer.bg[0].size = size;
            if (size & 1) check(&renderer, 0, 0, renderer.end);
            else check(&renderer, 0, extra - margin, extra + 240 + margin);
        }
        renderer.bg[0].size = 0;
        renderer.bg[0].wideMap = &renderer;
        check(&renderer, 0, 0, renderer.end); // A real wide RAM map must keep drawing.
        renderer.bg[0].wideMap = NULL;
        check(&renderer, 4, 0, renderer.end); // Objects are not text backgrounds.
        renderer.dispcnt = 1;
        check(&renderer, 0, extra - margin, extra + 240 + margin);
        check(&renderer, 2, 0, renderer.end); // Mode 1's affine background.
        renderer.dispcnt = 2;
        check(&renderer, 2, 0, renderer.end);
        renderer.wideMask = 0;
        check(&renderer, 0, extra, extra + 240); // Non-wide layers keep their old clipping.
    }
    struct GBAVideoSoftwareRenderer narrow = {0};
    narrow.end = 240;
    check(&narrow, 0, 0, 240);
    struct GBAVideoSoftwareRenderer empty = {0};
    empty.xOrigin = 58;
    empty.start = 0;
    empty.end = 10;
    check(&empty, 0, 58, 10); // Empty clipped spans are still restored for the next layer.
    struct GBAVideoSoftwareRenderer edge = {0};
    edge.xOrigin = 58;
    edge.width = edge.end = 356;
    edge.wideMask = 1;
    leftOpaque = true;
    check(&edge, 0, 0, 314);
    leftOpaque = false; rightOpaque = true;
    check(&edge, 0, 42, 356);
    leftOpaque = true;
    check(&edge, 0, 0, 356);
}
