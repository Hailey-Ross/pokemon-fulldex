// The GBA widescreen renderer (gba/patches/mgba-wide.patch) on a text BG with a 256-pixel map:
// a window at the right of the screen must not show again in the left side (issue #131,
// LeafGreen's level-up box on a 20:9 phone), and a layer that fills the screen still carries on.
#include <mgba/core/log.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/renderers/video-software.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t vram[0x18000 / 2];
static uint16_t palette[0x200];
static union GBAOAM oam;
static mColor out[GBA_VIDEO_MAX_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS];
static int failures;
static int eightBpp, rowMask = 255, mosaicV = 1, sampleY = 80;
static unsigned mapFlags;

static void logNothing(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
	(void) logger; (void) category; (void) level; (void) format; (void) args;
}

// BG1: 256x512 map (screen size 2) at screen block 28, tiles at char block 1; tile 1 is solid
// colour 1 (white), tile 0 is empty. columns: which of the map's 32 columns hold tile 1.
static void drawFrame(struct GBAVideoSoftwareRenderer* r, int extra, int firstColumn, int lastColumn, int hofs) {
	memset(vram, 0, sizeof(vram));
	for (int row = 0; row < 8; ++row) {
		if (rowMask & (1 << row)) {
			const int stride = eightBpp ? 8 : 4;
			memset((uint8_t*)vram + 0x4000 + 8 * stride + row * stride,
			       eightBpp ? 1 : 0x11, stride);
		}
	}
	for (int row = 0; row < 32; ++row) {
		for (int col = firstColumn; col <= lastColumn; ++col) {
			vram[(28 * 0x800) / 2 + row * 32 + col] = 1 | mapFlags;
		}
	}
	for (uint32_t address = 0; address < sizeof(vram); address += 2) {
		r->d.writeVRAM(&r->d, address);
	}
	r->bg[1].yCache = -1; // (the renderer's copy of the map's row)
	r->d.wideExtra = extra;
	r->outputBufferStride = GBA_VIDEO_HORIZONTAL_PIXELS + 2 * extra;
	r->d.writeVideoRegister(&r->d, GBA_REG_DISPCNT, 0x0200); // mode 0, BG1 on
	r->d.writeVideoRegister(&r->d, GBA_REG_BG1CNT, (2 << 14) | (28 << 8) | (1 << 2)
	                       | (eightBpp ? 0x80 : 0) | (mosaicV > 1 ? 0x40 : 0));
	r->d.writeVideoRegister(&r->d, GBA_REG_MOSAIC, (mosaicV - 1) << 4);
	r->d.writeVideoRegister(&r->d, GBA_REG_BG1HOFS, hofs);
	r->d.writeVideoRegister(&r->d, GBA_REG_WIDECNT, 0xA502); // BG1 into the sides
	for (int frame = 0; frame < 2; ++frame) {
		for (int y = 0; y < GBA_VIDEO_VERTICAL_PIXELS; ++y) {
			r->d.drawScanline(&r->d, y);
		}
		r->d.finishFrame(&r->d);
		r->d.writeVideoRegister(&r->d, GBA_REG_WIDECNT, 0xA502);
	}
}

// Whether the output pixel at the game's x (negative: the left side) on line 80 is BG1's white
static int lit(int extra, int x) {
	mColor c = out[sampleY * (GBA_VIDEO_HORIZONTAL_PIXELS + 2 * extra) + extra + x];
	return (c & 0xFFFFFF) == 0xFFFFFF || (c & 0xF8F8F8) == 0xF8F8F8;
}

static void expect(int ok, const char* what) {
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) {
		++failures;
	}
}

int main(void) {
	static struct mLogger logger = { .log = logNothing };
	mLogSetDefaultLogger(&logger);
	struct GBAVideoSoftwareRenderer r;
	GBAVideoSoftwareRendererCreate(&r);
	r.d.vram = vram;
	r.d.palette = palette;
	r.d.oam = &oam;
	r.outputBuffer = out;
	r.outputBufferStride = GBA_VIDEO_MAX_HORIZONTAL_PIXELS;
	r.d.init(&r.d);
	r.d.writePalette(&r.d, 0, 0x0000);
	palette[1] = 0x7FFF;
	r.d.writePalette(&r.d, 2, 0x7FFF);

	for (eightBpp = 0; eightBpp < 2; ++eightBpp) {
	printf("%d-bit text BG\n", eightBpp ? 8 : 4);
	for (int pass = 0; pass < 2; ++pass) {
		int extra = pass ? 22 : 58; // 16:9 and 20:9 at 160 lines
		char what[160];

		// A window in map columns 18-29 (x 144-239), like the level-up box on BG1
		drawFrame(&r, extra, 18, 29, 0);
		expect(lit(extra, 144) && lit(extra, 239), (snprintf(what, sizeof(what), "extra %d: the window is drawn on screen", extra), what));
		int wrapped = 0;
		for (int x = -extra; x < 0; ++x) {
			wrapped |= lit(extra, x);
		}
		expect(!wrapped, (snprintf(what, sizeof(what), "extra %d: the window does not show again in the left side", extra), what));

		// The same at the left (columns 0-9): nothing again in the right side
		drawFrame(&r, extra, 0, 9, 0);
		wrapped = 0;
		for (int x = GBA_VIDEO_HORIZONTAL_PIXELS; x < GBA_VIDEO_HORIZONTAL_PIXELS + extra; ++x) {
			wrapped |= lit(extra, x);
		}
		expect(!wrapped, (snprintf(what, sizeof(what), "extra %d: a window at the left does not show again in the right side", extra), what));

		// A layer that fills the map (a move's background): the sides are filled all the way
		drawFrame(&r, extra, 0, 31, 3);
		int full = 1;
		for (int x = -extra; x < GBA_VIDEO_HORIZONTAL_PIXELS + extra; ++x) {
			full &= lit(extra, x);
		}
		expect(full, (snprintf(what, sizeof(what), "extra %d: a full-screen layer still fills both sides", extra), what));

		// The map's 16 unseen columns (240-255) are still drawn into the right side
		drawFrame(&r, extra, 30, 31, 0);
		expect(lit(extra, 240) && lit(extra, 255) && !lit(extra, 239), (snprintf(what, sizeof(what), "extra %d: the unseen columns 240-255 still show in the right side", extra), what));
	}
	// The sampled tile row must follow vertical flips and vertical mosaic too.
	rowMask = 1 << 7;
	mapFlags = 1 << 11;
	drawFrame(&r, 58, 0, 31, 0);
	int full = 1;
	for (int x = -58; x < GBA_VIDEO_HORIZONTAL_PIXELS + 58; ++x) full &= lit(58, x);
	expect(full, "vertical flip preserves a full-screen row");
	rowMask = 1;
	mapFlags = 0;
	mosaicV = 4;
	sampleY = 83;
	drawFrame(&r, 58, 0, 31, 0);
	full = 1;
	for (int x = -58; x < GBA_VIDEO_HORIZONTAL_PIXELS + 58; ++x) full &= lit(58, x);
	expect(full, "vertical mosaic preserves a full-screen row");
	rowMask = 255; mosaicV = 1; sampleY = 80;
	}
	r.d.deinit(&r.d);
	printf(failures ? "%d failed\n" : "all passed\n", failures);
	return failures ? 1 : 0;
}
