// gba_glass.h's functions: the GBA player's pause menu drawn by the DS games' Menu.
// Included once, by main.cpp, after ui.h.
#pragma once
#include "gba_glass.h"
#include "state_slots.h"

namespace GbaGlass
{
static_assert(IC_PLAY == Glass::I_PLAY && IC_SAVE == Glass::I_SAVE && IC_LOAD == Glass::I_LOAD && IC_CHEATS == Glass::I_CHEATS &&
              IC_FAST == Glass::I_FAST && IC_TROPHY == Glass::I_TROPHY && IC_GEAR == Glass::I_GEAR && IC_POWER == Glass::I_POWER &&
              IC_MONITOR == Glass::I_MONITOR && IC_RATIO == Glass::I_RATIO && IC_EXPAND == Glass::I_EXPAND &&
              IC_FULLSCREEN == Glass::I_FULLSCREEN && IC_GRID == Glass::I_GRID && IC_PAD == Glass::I_PAD && IC_GAUGE == Glass::I_GAUGE &&
              IC_SPARKLE == Glass::I_SPARKLE && IC_INFO == Glass::I_INFO && IC_SPEAKER == Glass::I_SPEAKER && IC_REWIND == Glass::I_REWIND,
              "gba_glass.h's icon numbers are Glass::Icon's");

namespace
{
struct State
{
    Canvas canvas;
    Menu menu;
    std::vector<uint32_t> shot; // the paused game, w x h
    int shotW = 0, shotH = 0;
    bool shotNew = false;
    std::string key, pageKey;
};
State& S() { static State s; return s; }

void Sync(const Page& p)
{
    State& s = S();
    Menu& m = s.menu;
    m.open = true;
    m.columns = p.style == TILES ? 4 : 0;
    m.style = p.style == PAGE ? 1 : p.style == DIALOG ? 2 : 0;
    m.pageIcon = p.style == PAGE ? p.icon : -1;
    m.dialogIcon = p.style == DIALOG ? p.icon : -1;
    m.tallRows = p.style == PAGE && p.tall;
    m.slotGrid = p.style == PAGE && p.slotGrid;
    m.headerNote = p.headerNote;
    m.touchRows = p.touchRows;
    m.items.resize(p.items.size());
    for (size_t i = 0; i < p.items.size(); i++)
    {
        const Item& a = p.items[i];
        MenuItem& b = m.items[i];
        b.label = a.label;
        b.value = a.value;
        b.sub = a.sub;
        b.desc = a.desc;
        b.tip = a.tip;
        b.kind = a.kind == TOGGLE ? Menu::K_TOGGLE : a.kind == PICKER ? Menu::K_PICKER : a.kind == LINK ? Menu::K_LINK
               : a.kind == SECTION ? Menu::K_SECTION : a.kind == BACK ? Menu::K_BACK : a.kind == SLOT ? Menu::K_SLOT : Menu::K_PLAIN;
        b.thumb = a.thumb;
        b.thumbW = a.thumbW;
        b.thumbH = a.thumbH;
        b.icon = a.icon;
        b.slider = a.slider;
        b.disabled = a.disabled;
        b.info = a.info;
        b.choices = a.choices;
        b.choice = a.choice;
        b.preview = Menu::P_NONE;
    }
    // (Another page: its list from the top.)
    const std::string pk = std::to_string(p.style) + "|" + p.title;
    if (pk != s.pageKey)
    {
        s.pageKey = pk;
        m.scroll = 0;
        m.scrollPx = 0;
        m.freeScroll = false;
        m.pickWanted = -1;
    }
    m.selected = std::max(0, std::min(p.selected, (int)m.items.size() - 1));
}
}

std::string SlotWhen(const std::string& statePath) { return StateSlots::Friendly(statePath); }

bool SaveSlotPicture(const std::string& statePath, const uint32_t* argb, int w, int h)
{
    if (!argb || w <= 0 || h <= 0) return false;
    // (256 wide, as the DS games' pictures are; the height by the picture's shape.)
    const int tw = 256, th = std::max(1, std::min(192, (int)((long long)h * tw / w)));
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, tw, th, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return false;
    for (int y = 0; y < th; y++)
    {
        uint32_t* row = (uint32_t*)((uint8_t*)s->pixels + (size_t)y * s->pitch);
        const uint32_t* from = argb + (size_t)std::min(h - 1, (int)((long long)y * h / th)) * w;
        for (int x = 0; x < tw; x++) row[x] = from[std::min(w - 1, (int)((long long)x * w / tw))] | 0xFF000000u;
    }
    const bool ok = SDL_SaveBMP(s, (statePath + ".bmp").c_str()) == 0;
    SDL_FreeSurface(s);
    return ok;
}

bool LoadSlotPicture(const std::string& statePath, std::vector<uint32_t>& argb, int& w, int& h)
{
    argb.clear();
    w = h = 0;
    if (!StateSlots::PreviewCurrent(statePath)) return false;
    SDL_Surface* bmp = SDL_LoadBMP((statePath + ".bmp").c_str());
    if (!bmp) return false;
    SDL_Surface* s = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(bmp);
    if (!s) return false;
    w = std::min(s->w, 256);
    h = std::min(s->h, 192);
    argb.resize((size_t)w * h);
    for (int y = 0; y < h; y++) memcpy(&argb[(size_t)y * w], (const uint8_t*)s->pixels + (size_t)y * s->pitch, (size_t)w * 4);
    SDL_FreeSurface(s);
    return true;
}

void Frame(const uint32_t* argb, int w, int h)
{
    State& s = S();
    if (!argb || w <= 0 || h <= 0)
    {
        s.shot.clear();
        s.shotW = s.shotH = 0;
    }
    else
    {
        s.shot.assign(argb, argb + (size_t)w * h);
        for (uint32_t& px : s.shot) px |= 0xFF000000u;
        s.shotW = w;
        s.shotH = h;
    }
    s.shotNew = true;
}

int Update(Page& page, const Input& in, int& dir)
{
    Sync(page);
    MenuInput mi;
    mi.up = in.up; mi.down = in.down; mi.left = in.left; mi.right = in.right; mi.accept = in.accept; mi.back = in.back;
    const int hit = S().menu.Update(mi, dir);
    page.selected = S().menu.selected;
    return hit;
}

void Tap(Page& page, int x, int y, Input& out, int& slider, int& pick)
{
    Sync(page);
    Menu& m = S().menu;
    MenuInput mi;
    slider = -1;
    m.pickWanted = -1;
    m.Tap(x, y, mi, slider);
    pick = m.pickWanted;
    m.pickWanted = -1;
    page.selected = m.selected;
    out.up = mi.up; out.down = mi.down; out.left = mi.left; out.right = mi.right; out.accept = mi.accept; out.back = mi.back;
}

const uint32_t* Draw(const Page& page, int w, int h, bool& changed)
{
    State& s = S();
    Sync(page);
    Canvas& c = s.canvas;
    const bool resized = c.w != w || c.h != h;
    c.Resize(w, h);
    if (resized || s.shotNew)
    {
        s.shotNew = false;
        if (s.shotW == w && s.shotH == h && !s.shot.empty())
        {
            Glass::MakeBackdrop(c.backdrop, s.shot.data(), w, h);
            c.sharp = s.shot;
        }
        else
        {
            c.backdrop.assign(1, 0); // (nothing to blur: the game darkened)
            c.sharp.clear();
        }
        s.key.clear();
    }
    std::string key = std::to_string(w) + "x" + std::to_string(h) + "|" + std::to_string(page.style) + "|" + std::to_string(page.icon) + "|" +
        std::to_string(page.tall) + "|" + page.title + "|" + page.hint + "|" + page.note + "|" + page.headerNote + "|" +
        std::to_string(s.menu.selected) + "|" + std::to_string(s.menu.scrollPx) + "|" + std::to_string(UiLang::lang) + "|" + std::to_string(page.touchRows);
    for (const Item& it : page.items)
        key += "|" + it.label + "=" + it.value + "," + std::to_string(it.kind) + "," + std::to_string(it.choice) + "," + std::to_string(it.slider) + "," +
               std::to_string(it.disabled) + "," + it.sub + "," + it.desc + "," + std::to_string(it.thumb.size()) + "," +
               std::to_string(it.thumb.empty() ? 0u : it.thumb[it.thumb.size() / 2]);
    changed = key != s.key;
    if (changed)
    {
        s.key = key;
        std::fill(c.px.begin(), c.px.end(), 0u);
        c.x0d = 0; c.y0d = 0; c.x1d = w; c.y1d = h;
        s.menu.Draw(c, page.title, page.hint, page.note);
        // (Drawing can move the list to keep the chosen row in view: the next key then says so.)
    }
    return c.px.data();
}
}
