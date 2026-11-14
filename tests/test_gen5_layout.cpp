// ROM-free regression: build with C++17 and SDL2, then run with SDL_VIDEODRIVER=dummy.
#include <cassert>
#include <cstdio>
#include "../frontend/gpucompose.h"
#include "../frontend/gen5strip.h"
#include <cmath>

int main()
{
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    constexpr uint32_t blue = 0xFF103870, white = 0xFFF8F8F8, ink = 0xFF303030;
    std::vector<uint32_t> top(256 * 192, blue), bottom(256 * 192, blue);
    // An item's description and a distinct colour in the last row of its move stats.
    for (int y = 40; y < 50; y++)
        for (int x = 16; x < 96; x++) top[y * 256 + x] = white;
    for (int x = 8; x < 248; x++) top[191 * 256 + x] = 0xFF00E000;
    ScreenImage screens[2] = { { top.data(), nullptr }, { bottom.data(), nullptr } };
    for (int width : { 1024, 1280, 1920, 2560 })
        for (bool message : { false, true })
        {
            std::fill(bottom.begin(), bottom.end(), blue);
            if (message)
            {
                for (int y = 2; y < 44; y++)
                    for (int x = 2; x < 254; x++) bottom[y * 256 + x] = white;
                for (int y = 8; y < 18; y++)
                    for (int x = 8; x < 90; x++) bottom[y * 256 + x] = ink;
            }
            const MergedView view = MergedMakeView(width, 768);
            const Scene sc = BagScene(view.width, screens);
            // An item chosen: both screens side by side, as on the DS, over the Bag's blue.
            assert(sc.items.size() == 3);
            const Element& top = sc.items[1];
            const Element& touch = sc.items[2];
            assert(top.screen == 0 && touch.screen == 1);
            assert(top.sw == 256 && top.sh == 192 && touch.sw == 256 && touch.sh == 192);
            assert(top.scale == touch.scale && top.scale <= 1);
            assert(top.dx + 256 * top.scale <= touch.dx + 0.01f); // No overlap.
            assert(top.dx >= 128 - view.width / 2 - 0.01f); // Inside the window.
            assert(touch.dx + 256 * touch.scale <= 128 + view.width / 2 + 0.01f);

            std::vector<uint32_t> cpu;
            MergedCompose(sc, screens, width, 768, cpu, false, nullptr);
            SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, 768, 32, SDL_PIXELFORMAT_ARGB8888);
            assert(surface);
            SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
            assert(renderer);
            {
                GpuCompositor compositor(renderer);
                compositor.Draw(sc, screens, width, 768, false, nullptr);
                SDL_RenderPresent(renderer);
                if (message)
                {
                    // The touch screen's message is shown whole by both composition paths.
                    const int x = (int)(view.originX + (touch.dx + 30 * touch.scale) * view.unit);
                    const int y = (int)(view.originY + (touch.dy + 12 * touch.scale) * view.unit);
                    assert((cpu[y * width + x] & 0xFFFFFF) == (ink & 0xFFFFFF));
                    const auto* row = reinterpret_cast<const uint32_t*>(
                        static_cast<const uint8_t*>(surface->pixels) + y * surface->pitch);
                    assert((row[x] & 0xFFFFFF) == (ink & 0xFFFFFF));
                }
            }
            SDL_DestroyRenderer(renderer);
            SDL_FreeSurface(surface);
        }
    // Empty pockets and missing screen images must not create a description card.
    std::fill(top.begin(), top.end(), blue);
    assert(BagScene(341.33f, screens).items.size() == 1);
    assert(BagScene(341.33f, nullptr).items.size() == 1);

    // Battle questions: the question at full size, both answers in the message bar,
    // right of the text and inside the window.
    for (float width : { 256.0f, 341.33f, 455.11f, 512.0f })
    {
        const Scene question = BattleChoiceScene(width);
        assert(question.items.size() == 3);
        const Element& text = question.items[0];
        assert(text.screen == 0 && text.sy == 0 && text.sh == 192 && text.scale == 1);
        float last = 0;
        for (int i = 1; i <= 2; i++)
        {
            const Element& b = question.items[i];
            assert(b.screen == 1 && b.sw == 252 && b.sh == 47);
            assert(b.dy >= 147 && b.dy + b.sh * b.scale <= 190); // In the bar.
            assert(b.dy >= last);                                // Stacked, no overlap.
            assert(b.dx >= 168);                                 // Clear of the question.
            assert(b.dx + b.sw * b.scale <= 128 + width / 2);    // Inside the window.
            last = b.dy + b.sh * b.scale;
        }
    }
    // A battle's question with BATTLE BUTTONS at STRIP (Gen5Strip::ChoiceBars): the two bars
    // (touch screen rows 32-127, without the backdrop around them) on the message box's band,
    // right of the question and inside the window, once they are in their places; before that
    // (the Ball turning into them) no piece of the touch screen at all.
    {
        std::vector<uint32_t> q(256 * 192, 0xFF204020), t(256 * 192, 0xFF101010);
        std::vector<uint8_t> tf(256 * 192, 0); // the backdrop everywhere
        for (int y = 160; y < 172; y++)
            for (int x = 8; x < 150; x += 3) q[y * 256 + x] = 0xFFF8F8F8; // the question, to x 149
        const ScreenImage choice[2] = { { q.data(), nullptr }, { t.data(), tf.data() } };
        for (float width : { 256.0f, 307.2f, 341.33f, 455.11f })
        {
            const float right = 128 + width / 2 - 2;
            Scene sc = BattleChoiceScene(width);
            assert(!Gen5Strip::ChoiceBars(sc, right, choice, 147, 42));
            for (const Element& e : sc.items) assert(e.screen != 1);
            for (int top : { 34, 82 })
                for (int y = top; y < top + 44; y++)
                    for (int x = 9; x < 247; x++) tf[y * 256 + x] = LAYER_BG1;
            sc = BattleChoiceScene(width);
            assert(Gen5Strip::ChoiceBars(sc, right, choice, 147, 42));
            int bars = 0;
            for (const Element& e : sc.items)
                if (e.screen == 1)
                {
                    bars++;
                    assert(e.sx == 0 && e.sy == 32 && e.sw == 256 && e.sh == 96 && e.drop == LAYER_BACKDROP);
                    assert(e.dy >= 146.5f && e.dy + e.sh * e.scale <= 189.5f);     // On the band.
                    assert(e.dx >= 150 + 4 - 0.01f || e.scale <= 0.36f + 1e-4f); // Clear of the question.
                    assert(e.dx + e.sw * e.scale <= right + 0.01f);                // Inside the window.
                }
            assert(bars == 1);
            std::fill(tf.begin(), tf.end(), 0);
        }
    }

    // The battle's HP boxes stay where the battle had them in a scene made without them (a
    // question, the party fading in: Gen5Strip::KeepBoxes): its holes in the top screen and the
    // boxes' pieces right after it. A battle whose boxes were not moved (no holes) changes nothing.
    {
        Element battleTop = FullScreen(0);
        battleTop.holeCount = 2;
        const int holes[2][4] = { { 0, 26, 116, 56 }, { 138, 106, 256, 142 } };
        std::copy(&holes[0][0], &holes[0][0] + 8, &battleTop.holes[0][0]);
        Element foe = Crop(0, 0, 26, 116, 30), mine = Crop(0, 138, 106, 118, 36);
        foe.dx = -25.6f;
        mine.dx = 281.6f - 118;
        foe.wideOnly = mine.wideOnly = true;
        Scene sc = BattleChoiceScene(307.2f);
        const size_t before = sc.items.size();
        Gen5Strip::KeepBoxes(sc, battleTop, { foe, mine }, 1);
        assert(sc.items.size() == before + 2);
        assert(sc.items[0].holeCount == 2 && sc.items[0].holes[1][0] == 138 && sc.items[0].holes[1][3] == 142);
        assert(sc.items[1].wideOnly && sc.items[1].dx == foe.dx && sc.items[2].dx == mine.dx);
        Scene plain = BattleChoiceScene(307.2f);
        Gen5Strip::KeepBoxes(plain, FullScreen(0), { foe, mine }, 1);
        assert(plain.items.size() == before && plain.items[0].holeCount == 0);
    }
    // The strip's command menu (Gen5Strip::Commands): FIGHT!, BAG, RUN and POKEMON as rounded
    // buttons on the moves' grid, on the band and inside the window, a click anywhere on each
    // landing inside the game's own button; FIGHT!'s label beside the icons of a double battle,
    // which go with it; the cursor's brackets (sprites) at the corners of the button it is on.
    {
        std::vector<uint32_t> t(256 * 192, 0xFF303030);
        std::vector<uint8_t> tf(256 * 192, LAYER_BG0);
        const ScreenImage touch{ t.data(), tf.data() };
        for (const Gen5Strip::CommandLook* look : { &Gen5Strip::Black2Commands(), &Gen5Strip::BlackCommands() })
        {
            const float s = 0.44f, right = 279.6f, left = right - 256 * s, top = 148 + (40 - 96 * s) / 2 - 32 * s;
            Scene sc;
            Gen5Strip::Commands(sc, touch, *look, left, top, s);
            int fills = 0, areas = 0;
            for (const Element& e : sc.items)
            {
                if (e.screen < 0)
                {
                    fills++;
                    assert(e.border && e.borderRadius > 0);
                    assert(e.dy >= 146.5f && e.dy + e.sh * e.scale <= 189.5f);                  // On the band.
                    assert(e.dx >= left - 0.01f && e.dx + e.sw * e.scale <= right + 0.01f);     // Inside the window.
                }
                if (e.alpha == 0) areas++;
            }
            assert(fills == 4 && areas == 4);
            for (int i = 0; i < 4; i++)
            {
                const int* c = look->cell[i];
                const int* area = look->button[i].touch;
                for (float fx : { 0.05f, 0.5f, 0.95f })
                    for (float fy : { 0.05f, 0.5f, 0.95f })
                    {
                        int tx, ty;
                        assert(MergedTouch(sc, left + (c[0] + c[2] * fx) * s, top + (c[1] + c[3] * fy) * s, tx, ty));
                        assert(tx >= area[0] && tx < area[0] + area[2] && ty >= area[1] && ty < area[1] + area[3]);
                    }
            }
            // A double battle's icons (sprites, rows 109-125): FIGHT!'s label moves left of the
            // button's middle to make room for them, right of it.
            for (int y = 109; y <= 125; y++)
                for (int x : { 95, 100, 111, 140, 150, 156 }) tf[y * 256 + x] = 0x10;
            Scene two;
            Gen5Strip::Commands(two, touch, *look, left, top, s);
            const int* fight = look->cell[0];
            const float middle = left + (fight[0] + fight[2] / 2.0f) * s;
            const Element* label = nullptr;
            const Element* icons = nullptr;
            for (const Element& e : two.items)
            {
                if (e.screen == 1 && e.sx == look->button[0].label[0] && e.sy == look->button[0].label[1]) label = &e;
                if (e.screen == 1 && e.sx == 95 && e.sy == 109 && e.sw == 62) icons = &e;
            }
            assert(label && icons);
            assert(label->dx + label->sw * label->scale / 2 < middle);
            assert(icons->dx > label->dx + label->sw * label->scale && icons->dx + icons->sw * icons->scale <= left + (fight[0] + fight[2]) * s);
            std::fill(tf.begin(), tf.end(), LAYER_BG0);
            // The cursor on BAG (its brackets' colour from x 2 to 76, rows 150 to 188).
            t[150 * 256 + 2] = t[188 * 256 + 76] = 0xFF000000 | look->cursor;
            Scene cur;
            Gen5Strip::Commands(cur, touch, *look, left, top, s);
            const int* bag = look->cell[1];
            int brackets = 0;
            for (const Element& e : cur.items)
                if (e.screen == 1 && e.alpha && e.drop == (LAYER_BG0 | LAYER_BG1 | LAYER_BG2 | LAYER_BG3 | LAYER_BACKDROP) && e.sw == 16)
                {
                    brackets++;
                    const float cx = e.dx + 8 * s, cy = e.dy + 8 * s;
                    assert(cx > left + (bag[0] - 16) * s && cx < left + (bag[0] + bag[2] + 16) * s);
                    assert(cy > top + (bag[1] - 16) * s && cy < top + (bag[1] + bag[3] + 16) * s);
                }
            assert(brackets == 4);
            t[150 * 256 + 2] = t[188 * 256 + 76] = 0xFF303030;
        }
    }

    // The buttons below the moves (Gen5Strip::Below), right to left from the moves in the touch
    // screen's order (SHIFT, then back), apart, as tall as the rows given, each a click onto the
    // game's own button.
    {
        std::vector<uint32_t> t(256 * 192, 0xFF303030);
        std::vector<uint8_t> tf(256 * 192, LAYER_BG0);
        const ScreenImage touch{ t.data(), tf.data() };
        const Gen5Strip::BelowButton* below[2] = { &Gen5Strip::Black2Shift(), &Gen5Strip::Black2Back() };
        const float s = 0.4f, from = 180, y = 150, h = 36;
        Scene sc;
        Gen5Strip::Below(sc, touch, below, 2, from, y, h, s, 3, 0xC31028);
        std::vector<const Element*> fills;
        for (const Element& e : sc.items)
            if (e.screen < 0) fills.push_back(&e);
        assert(fills.size() == 2);
        // (back first, against the moves; SHIFT left of it)
        assert(std::fabs(fills[0]->dx + fills[0]->sw * fills[0]->scale - (from - 3)) < 0.01f);
        assert(fills[1]->dx + fills[1]->sw * fills[1]->scale < fills[0]->dx);
        for (int i = 0; i < 2; i++)
        {
            const Element& f = *fills[i];
            assert(std::fabs(f.dy - y) < 0.01f && std::fabs(f.sh * f.scale - h) < 0.05f);
            const int* area = below[1 - i]->look.touch;
            int tx, ty;
            assert(MergedTouch(sc, f.dx + f.sw * f.scale / 2, f.dy + h / 2, tx, ty));
            assert(tx >= area[0] && tx < area[0] + area[2] && ty >= area[1] && ty < area[1] + area[3]);
        }
    }
    SDL_Quit();
    puts("Gen 5 layouts: side-by-side Bag, battle questions, the strip's answers and buttons, kept HP boxes and compositors passed.");
}
