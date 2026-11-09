#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

enum { GBA_VIDEO_HORIZONTAL_PIXELS = 240 };
struct GBAVideoSoftwareBackground { const void* wideMap; int size; };
struct GBAVideoSoftwareRenderer {
    int xOrigin, wideMask, start, end, wideSavedStart, wideSavedEnd;
    bool wideClipped;
    unsigned dispcnt;
    struct GBAVideoSoftwareBackground bg[4];
};
static inline unsigned GBARegisterDISPCNTGetMode(unsigned value) { return value & 7; }
#include "wide_clip_under_test.h"

static void check(struct GBAVideoSoftwareRenderer* renderer, int layer, int left, int right) {
    const int start = renderer->start, end = renderer->end;
    assert(_wideClip(renderer, layer) == (left < right));
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
}
