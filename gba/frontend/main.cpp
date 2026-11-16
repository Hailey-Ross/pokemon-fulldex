// Pokemon GBA Widescreen: the player program. A game list of the player's ROMs, then the game
// on mGBA (with this project's wide renderer) and the widescreen patch put on it as it starts:
// one picture as wide as the screen, 60 frames a second, scaled to any size from 720p to 4K.
//
//   pokemon-gba-wide                    the game list
//   pokemon-gba-wide --game emerald     straight into a game (emerald, ultraviolet, firered, leafgreen)
//   pokemon-gba-wide --rom FILE         straight into any ROM
// Tests: --window WxH (a window of that size), --script FILE (key presses and screenshots by
// frame, for headless runs with SDL_VIDEODRIVER=offscreen; with GBAW_TEST_REALTIME set, at the
// game's real pace on the graphics card, reporting how steadily the frames came), --data DIR
// (the folder for roms, saves and settings).
#include <sys/stat.h>
#include <ctime>
#include <cctype>
#include <mgba/flags.h>
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <mgba/internal/gba/gba.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>
#include <mgba-util/vfs.h>

#include <SDL.h>

#include "art.h"
#include "fast_forward_button.h"
#include "fast_forward_audio.h"
#ifdef GBAW_EMBEDDED
#include "embed.h"
#include "gba_glass.h" // (the pause menu as the DS games draw it; Fulldex's frontend folder)
#include "gba_dex.h"       // (POKEDEX: the game's own Pokedex data and pictures, read from its ROM)
#include "gba_companion.h" // (COMPANION: the party, bag and place, read from the paused game)
#include "gba_guides.h"    // (COMPANION's GUIDE)
#include "companion_types.h"
// Cheats (CHEATS in the pause menu): Fulldex's, from its frontend folder (on the include path when
// built into it). All off until the player turns one on.
#if __has_include("gba_cheats.h")
#include "gba_cheats.h"
#define GBAW_CHEATS 1
#endif
// (The launcher has stb_image too: this one's kept to this file.)
#define STB_IMAGE_STATIC
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "third_party/stb_image.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include "third_party/xbrz/xbrz.h"
// ZOOM and the look-around: PokeDaisy's WIDE (GPL-3.0), the overworld past the picture rebuilt from the game's memory.
extern "C" {
#include "pokedaisy/pk_wide.h"
}
extern "C" int pk_wide_take_fail;   // pk_wide.c, test log only
#include "pokedaisy/wide_camera.h"
#if defined(__SWITCH__)
#include <switch.h>
#endif

#include "android.h"
#include "games.h"
#include "gameplay.h"
#include "ff_music_gba.h"
#include "follower_hgss.h"
#include "fly_gba.h"
#include "flight_records.h"
#include "sinnoh_names.h"
#include "boxes_gba.h"
#include "items_gba.h"
#include <atomic>
#include <filesystem>
#include "touch.h"
#include "ui.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
// A phone: on-screen controls, always full screen. Android's folder picker stays __ANDROID__.
#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define GBAW_PHONE 1
#endif

using namespace gbaw;

namespace
{

constexpr int GbaW = 240, GbaH = 160;
constexpr int MaxExtra = 112;                 // the most the game patches support a side
constexpr int MaxW = GbaW + 2 * MaxExtra;     // 464
constexpr double Fps = 60.0;                  // the GBA's 59.73 is run at the screen's 60

// GBA keys, as mGBA numbers them.
enum GbaKey { K_A, K_B, K_SELECT, K_START, K_RIGHT, K_LEFT, K_UP, K_DOWN, K_R, K_L };

void QuietLog(struct mLogger*, int, enum mLogLevel level, const char* format, va_list args)
{
    if (level & mLOG_FATAL)
    {
        vfprintf(stderr, format, args);
        fputc('\n', stderr);
    }
}
struct mLogger gQuiet = { QuietLog, nullptr };

// The window's icon: the same picture as the program's (tools/pc/make_icon.py, keep them alike).
uint32_t IconPixel(int x, int y, int s)
{
    const double fx = (x + 0.5) / s, fy = (y + 0.5) / s, r = 0.17;
    const double cx = std::min(std::max(fx, r), 1 - r), cy = std::min(std::max(fy, r), 1 - r);
    if ((fx - cx) * (fx - cx) + (fy - cy) * (fy - cy) > r * r) return 0;
    if (fx >= 0.08 && fx <= 0.92 && fy >= 0.27 && fy <= 0.73)
    {
        if (!(fx >= 0.12 && fx <= 0.88 && fy >= 0.31 && fy <= 0.69)) return 0xFFF0F0F0;
        const double d = fabs(fx - 0.5) / 0.14 + fabs(fy - 0.5) / 0.15;
        if (d <= 1) return fy < 0.5 ? 0xFFFFFFFF : 0xFFC8D0E0;
        return 0xFF0B0F18;
    }
    const int top[3] = { 0x1E, 0x8C, 0x55 }, bottom[3] = { 0x6A, 0x3F, 0xC0 };
    uint32_t c = 0xFF000000;
    for (int i = 0; i < 3; i++) c |= (uint32_t)(int)(top[i] + (bottom[i] - top[i]) * fy) << (16 - 8 * i);
    return c;
}

// ---------------------------------------------------------------------------------------------
// Settings: settings.ini beside the saves, one "key=value" per line.

struct Setting
{
    const char* key;
    const char* name;
    std::vector<std::pair<std::string, std::string>> values; // stored, shown
    const char* def;
    const char* text;
};

const std::vector<Setting>& SettingsList()
{
    static const std::vector<std::pair<std::string, std::string>> onOff = { { "0", "OFF" }, { "1", "ON" } };
    static const std::vector<Setting> l = {
        { "widescreen", "ASPECT RATIO",
          { { "auto", "AUTO" }, { "16:9", "16:9" }, { "16:10", "16:10" }, { "3:2", "3:2" }, { "4:3", "4:3" } },
          "auto",
          "AUTO: AS WIDE AS YOUR SCREEN. 3:2 AND 4:3: THE GBA'S OWN SHAPE, WITH BLACK AT THE SIDES. THE WORLD AND "
          "BATTLES FILL THE WIDTH; MENUS AND THE BAG STAY IN THE MIDDLE." },
#if !defined(__SWITCH__) && !defined(GBAW_PHONE)
        { "fullscreen", "BORDERLESS FULLSCREEN", onOff, "1",
          "ON: FILLS YOUR SCREEN AS A BORDERLESS WINDOW AT ITS OWN RESOLUTION. F11 SWITCHES BETWEEN IT AND A WINDOW." },
#endif
        { "scaling", "SCALING", { { "sharp", "SHARP" }, { "smooth", "SMOOTH" }, { "whole", "PIXEL PERFECT" } }, "sharp",
          "HOW THE GBA'S SMALL PICTURE IS ENLARGED TO YOUR SCREEN. SHARP: CRISP SQUARE PIXELS THAT FILL THE SCREEN. "
          "SMOOTH: THE PIXELS' EDGES SOFTENED. PIXEL PERFECT: ONLY WHOLE-NUMBER ZOOMS (4X, 5X, NEVER 4.5X), SO EVERY "
          "PIXEL IS EXACTLY THE SAME SIZE, WITH A THIN BLACK BORDER." },
        { "hd_filter", "HD FILTER", onOff, "0",
          "ON: THE PIXEL ART IS REDRAWN WITH SMOOTH EDGES AND CURVES (XBRZ). OFF: THE GAME'S OWN PIXELS." },
        { "lcd", "LCD EFFECT", onOff, "0", "ON: A FAINT GRID BETWEEN THE PIXELS, LIKE A GBA'S OWN SCREEN." },
        { "colour", "COLOUR CORRECTION", onOff, "0",
          "ON: THE COLOURS AS THEY LOOKED ON A GBA'S SCREEN (THE GAMES WERE MADE EXTRA BRIGHT FOR IT). "
          "OFF: THE GAME'S OWN COLOURS." },
        { "zoom", "ZOOM AND LOOK AROUND", onOff, "1",
          "ON: A CLICK OF THE RIGHT STICK (C ON A KEYBOARD) ZOOMS OUT IN STEPS TO SHOW MORE OF THE MAP, AND THE RIGHT STICK "
          "(I, J, K, L) LOOKS AROUND IT. MENUS AND BATTLES COME BACK CLOSE BY THEMSELVES. RUBY, SAPPHIRE, EMERALD, FIRERED, "
          "LEAFGREEN, ULTRA VIOLET AND MONOMYTH." },
        { "zoom_text", "AUTO ZOOM IN", onOff, "1",
          "ON: A TEXT BOX BRINGS A ZOOMED-OUT VIEW BACK CLOSE WHILE YOU READ IT. MENUS AND BATTLES ALWAYS DO." },
        { "day_night", "DAY AND NIGHT", onOff, "0",
          "ON: OUTDOORS FOLLOWS YOUR CLOCK: A WARM DAWN, PLAIN DAY, AN ORANGE DUSK AND A BLUE NIGHT. ONLY THE COLOURS AS THEY ARE "
          "DRAWN CHANGE: NOTHING IN THE GAME OR ITS SAVE. TEXT BOXES, MENUS AND BATTLES KEEP THEIR OWN. NEEDS ZOOM AND LOOK AROUND'S GAMES." },
        { "nav_style", "NAV STYLE",
          { { "modern", "MODERN" }, { "classic", "CLASSIC" }, { "simple", "SIMPLE" }, { "wheel", "WHEEL" } },
          "modern",
          "FULLDEX NAV (A CLICK OF THE LEFT STICK, N, OR NAV ON A PHONE): A GRAPHITE WATCH, A RED ONE, A GLASS CARD OR THE APPS IN A RING. ON A PHONE IT IS SMALLER, AT THE RIGHT (THE RING IN THE MIDDLE)." },
        { "follower", "FOLLOWING POKEMON", onOff, "0",
          "ON: THE FIRST POKEMON OF YOUR PARTY WALKS BEHIND YOU, AS IN HEARTGOLD AND SOULSILVER. ITS SPRITES COME FROM YOUR OWN "
          "HEARTGOLD OR SOULSILVER IN THE ROMS FOLDER (THE VERY BIG ONES HAVE NONE). NEEDS ZOOM AND LOOK AROUND'S GAMES." },
        { "show_fps", "FPS COUNTER", onOff, "0", "SHOWS THE FRAMES A SECOND IN A CORNER." },
        { "volume", "VOLUME",
          { { "0", "OFF" }, { "10", "10%" }, { "20", "20%" }, { "30", "30%" }, { "40", "40%" }, { "50", "50%" },
            { "60", "60%" }, { "70", "70%" }, { "80", "80%" }, { "90", "90%" }, { "100", "100%" } },
          "100", "THE GAME'S SOUND." },
        { "ff_mute", "FAST FORWARD SOUND", { { "0", "ON" }, { "1", "OFF" } }, "0",
          "ON: THE MUSIC CARRIES ON AT ITS NORMAL SPEED AND PITCH WHILE THE GAME FAST FORWARDS. IN THE POKEMON GAMES EACH SONG "
          "PLAYS IN FULL: A HIDDEN COPY OF THE GAME RECORDS IT THE FIRST TIME YOU HEAR IT. OFF: FAST FORWARD IS SILENT." },
        { "instant_text", "INSTANT TEXT", onOff, "0",
          "ON: A TEXT BOX'S WORDS APPEAR AT ONCE, WHATEVER THE GAME'S OWN TEXT SPEED. NOTHING ELSE SPEEDS UP. "
          "RUBY, SAPPHIRE, EMERALD, FIRERED AND LEAFGREEN IN EVERY LANGUAGE, AND HACKS THAT KEPT THEIR TEXT CODE." },
        { "reusable_tms", "REUSABLE TMS", onOff, "0",
          "ON: EVERY TM STAYS IN THE BAG AFTER TEACHING ITS MOVE, AS AN HM DOES. NOTHING ELSE CHANGES." },
        { "run_indoors", "RUN INDOORS", onOff, "0",
          "ON: THE RUNNING SHOES WORK ON EVERY MAP, INDOORS TOO. EMERALD, FIRERED AND LEAFGREEN (RUBY AND SAPPHIRE DECIDE IT ANOTHER WAY)." },
        { "wild_encounters", "WILD ENCOUNTERS",
          { { "100", "NORMAL" }, { "75", "75%" }, { "50", "50%" }, { "25", "25%" }, { "0", "OFF" } }, "100",
          "HOW MANY OF THE WILD POKEMON YOU WOULD MEET IN GRASS, CAVES AND WATER STILL TURN UP. FISHING, SWEET SCENT "
          "AND THE GAME'S OWN BATTLES ARE NOT CHANGED." },
        { "fly_on", "FLY ON",
          { { "bird", "BIRD" }, { "pokemon", "YOUR FLY POKEMON" }, { "any", "ANY POKEMON" }, { "latias", "LATIAS" }, { "latios", "LATIOS" }, { "rayquaza", "RAYQUAZA" },
            { "lugia", "LUGIA" }, { "hooh", "HO-OH" }, { "deoxys", "DEOXYS" }, { "articuno", "ARTICUNO" }, { "zapdos", "ZAPDOS" }, { "moltres", "MOLTRES" }, { "balloon", "AIR BALLOON" }, { "helicopter", "HELICOPTER" }, { "zeppelin", "ZEPPELIN" }, { "plane", "PLANE" } }, "bird",
          "WHAT FREE FLIGHT CARRIES YOU ON. BIRD: THE GAME'S OWN FLY BIRD. YOUR FLY POKEMON: THE POKEMON IN YOUR PARTY THAT KNOWS FLY, "
          "ITS SPRITE FROM YOUR OWN HEARTGOLD OR SOULSILVER IN THE ROMS FOLDER (THE BIRD WHEN THERE IS NONE). ANY POKEMON: THE ONE "
          "YOU PICK IN COMPANION, FLY, FLY ON. LATIAS, LATIOS, "
          "RAYQUAZA, LUGIA, HO-OH AND DEOXYS ARE EMERALD'S OWN SPRITES (EMERALD ONLY). THE BALLOON, HELICOPTER, "
          "ZEPPELIN AND PLANE ARE POKEDAISY'S OWN PIXEL ART." },
        { "flight_clouds", "CLOUDS OVER UNEXPLORED", onOff, "1",
          "ON: WHILE YOU FLY, CLOUDS COVER THE PLACES YOU HAVEN'T BEEN YET, AND CLEAR AS YOU REACH THEM. FULLDEX NOTES EACH MAP "
          "YOU STAND ON; FOR A SAVE FROM BEFORE, THE TOWNS YOU HAVE VISITED AND THE ROUTES BESIDE THEM COUNT AS SEEN." },
        { "battle_speed", "BATTLE SPEED", { { "1", "NORMAL" }, { "2", "2X" }, { "3", "3X" }, { "4", "4X" } }, "1",
          "HOW FAST BATTLES RUN, WITHOUT FAST FORWARD: THE WHOLE BATTLE, ITS MENUS TOO. FAST FORWARD STILL GOES AT ITS OWN SPEED." },
        { "text_speed", "TEXT SPEED", { { "1", "NORMAL" }, { "2", "2X" }, { "3", "3X" }, { "4", "4X" } }, "1",
          "HOW FAST THE GAME RUNS WHILE A TEXT BOX IS UP OUTSIDE BATTLES (PEOPLE TALKING, SIGNS, THE START MENU)." },
        { "bike_speed", "BIKE SPEED", { { "1", "NORMAL" }, { "2", "2X" }, { "3", "3X" }, { "4", "4X" } }, "1",
          "HOW FAST THE GAME RUNS WHILE YOU RIDE A BIKE." },
        { "surf_speed", "SURF SPEED", { { "1", "NORMAL" }, { "2", "2X" }, { "3", "3X" }, { "4", "4X" } }, "1",
          "HOW FAST THE GAME RUNS WHILE YOU SURF OR DIVE." },
        { "shiny_odds", "SHINY ODDS",
          { { "8192", "NORMAL" }, { "4096", "1 IN 4096" }, { "1024", "1 IN 1024" }, { "512", "1 IN 512" }, { "128", "1 IN 128" } }, "8192",
          "HOW OFTEN A WILD POKEMON TURNS UP SHINY. NORMAL IS THE GAME'S OWN 1 IN 8192. POKEMON ALREADY CAUGHT DON'T CHANGE, "
          "AND ONE CAUGHT SHINY STAYS SHINY WITH THIS BACK AT NORMAL. RUBY, SAPPHIRE, EMERALD, FIRERED AND LEAFGREEN." },
        { "gba_start", "START BUTTON", { { "start", "GBA START" }, { "menu", "PAUSE MENU" } }, "start",
#if defined(__SWITCH__)
          "PAUSE MENU: + OPENS THIS MENU, AND + AND - TOGETHER ARE THE GBA'S START. GBA START: + IS THE GBA'S START, "
          "AND + AND - TOGETHER OPEN THIS MENU." },
#else
          "PAUSE MENU: A CONTROLLER'S START OPENS THIS MENU, AND START AND SELECT TOGETHER ARE THE GBA'S START. "
          "GBA START: START IS THE GBA'S START, AND START AND SELECT TOGETHER OPEN THIS MENU." },
#endif
#ifdef GBAW_EMBEDDED
        // (FAST FORWARD on or off, in CONTROLS: the pause menu's tiles are the DS games' eight.)
        { "fast_lock", "FAST FORWARD", { { "0", "OFF" }, { "1", "ON" } }, "0",
          "FAST FORWARD ON OR OFF UNTIL IT IS CHANGED AGAIN, AS THE FAST FORWARD BUTTON TOGGLES IT." },
#endif
        { "fast_button", "FAST FORWARD BUTTON", { { "hold", "HOLD" }, { "toggle", "TOGGLE" } }, "hold",
          "HOLD: RELEASE TO STOP. TOGGLE: PRESS ONCE TO START AND AGAIN TO STOP. FOR THE KEYBOARD AND A CONTROLLER; THE >> BUTTON ON THE SCREEN IS ALWAYS A TAP ON, A TAP OFF." },
        { "ff_speed", "FAST FORWARD SPEED", { { "2", "2X" }, { "3", "3X" }, { "4", "4X" }, { "8", "8X" } }, "3",
#if defined(GBAW_PHONE)
          "HOW FAST FAST FORWARD GOES (THE >> BUTTON ON THE SCREEN, OR HOLD A CONTROLLER'S RIGHT TRIGGER)." },
#elif defined(__SWITCH__)
          "HOW FAST FAST FORWARD GOES (HOLD ZR)." },
#else
          "HOW FAST FAST FORWARD GOES (HOLD LEFT SHIFT, OR A CONTROLLER'S RIGHT TRIGGER)." },
#endif
#if !defined(__SWITCH__)
        // (Not on the Switch: its buttons are where their labels say, as on a GBA.)
        { "buttons", "CONTROLLER BUTTONS", { { "labels", "BY LABEL" }, { "positions", "BY POSITION" } }, "labels",
          "BY LABEL: THE BUTTON PRINTED A IS THE GBA'S A. BY POSITION: THE RIGHT-HAND BUTTON IS A AND THE BOTTOM ONE IS "
          "B, AS ON A GBA, WHATEVER THEY SAY." },
#endif
    };
    return l;
}

struct Settings
{
    std::string path;
    std::map<std::string, std::string> values;

    void Load(const std::string& p)
    {
        path = p;
        values.clear();
        std::vector<uint8_t> data;
        if (!Games::ReadFile(path, data, 1 << 20)) return;
        std::string line;
        data.push_back('\n');
        for (uint8_t c : data)
        {
            if (c == '\n' || c == '\r')
            {
                const size_t eq = line.find('=');
                if (eq != std::string::npos) values[line.substr(0, eq)] = line.substr(eq + 1);
                line.clear();
            }
            else
                line += (char)c;
        }
    }
    void Save() const
    {
        std::string s;
        for (const auto& kv : values) s += kv.first + "=" + kv.second + "\n";
        Games::WriteFile(path, std::vector<uint8_t>(s.begin(), s.end()));
    }
    std::string Get(const char* key) const
    {
        auto it = values.find(key);
        if (it != values.end()) return it->second;
        const char* deck = std::getenv("SteamDeck");
        if (!strcmp(key, "gba_start") && deck && !strcmp(deck, "1")) return "menu";
        for (const Setting& s : SettingsList())
            if (!strcmp(s.key, key)) return s.def;
        if (!strcmp(key, "fullscreen")) return "1";
        return "";
    }
    int Int(const char* key) const { return atoi(Get(key).c_str()); }
    // The next (d 1) or previous (d -1) value of a setting.
    void Step(const Setting& s, int d)
    {
        const std::string v = Get(s.key);
        int i = 0;
        for (size_t j = 0; j < s.values.size(); j++)
            if (s.values[j].first == v) i = (int)j;
        const int n = (int)s.values.size();
        values[s.key] = s.values[(i + d + n) % n].first;
        Save();
        ToHost(s.key);
    }
    // The same setting in Fulldex's own settings file (its names and values), if Fulldex gave one.
    void ToHost(const char* key) const
    {
        auto h = values.find("host_settings");
        if (h == values.end() || h->second.empty()) return;
        const std::string v = Get(key);
        std::string hk, hv;
        if (!strcmp(key, "scaling")) { hk = "smooth"; hv = v == "smooth" ? "1" : "0"; }
        else if (!strcmp(key, "show_fps") || !strcmp(key, "volume") || !strcmp(key, "buttons") || !strcmp(key, "ff_mute") || !strcmp(key, "fast_lock") || !strcmp(key, "fast_button")) { hk = key; hv = v; }
        else if (!strcmp(key, "lcd")) { hk = "gba_lcd"; hv = v; }
        else if (!strcmp(key, "widescreen")) { hk = "aspect"; hv = v == "off" ? "3:2" : v; }
        else if (!strcmp(key, "fullscreen")) { hk = key; hv = v; }
        else return;
        std::vector<uint8_t> data;
        Games::ReadFile(h->second, data, 1 << 20);
        std::string out, line;
        bool done = false;
        data.push_back('\n');
        for (size_t i = 0; i < data.size(); i++)
        {
            const char c = (char)data[i];
            if (c != '\n' && c != '\r') { line += c; continue; }
            if (c == '\r' && i + 1 < data.size() && data[i + 1] == '\n') continue;
            if (!line.compare(0, hk.size() + 1, hk + "=")) { line = hk + "=" + hv; done = true; }
            if (!line.empty()) out += line + "\n";
            line.clear();
        }
        if (!done) out += hk + "=" + hv + "\n";
        Games::WriteFile(h->second, std::vector<uint8_t>(out.begin(), out.end()));
    }
    std::string Shown(const Setting& s) const
    {
        const std::string v = Get(s.key);
        for (const auto& kv : s.values)
            if (kv.first == v) return kv.second;
        return v;
    }
};

// ---------------------------------------------------------------------------------------------
// Input: the keyboard, any number of controllers, and test scripts (which push key events).

// What the menus understand, edge-triggered.
struct MenuIn
{
    bool up = false, down = false, left = false, right = false, accept = false, back = false;
    bool menu = false, settings = false, folder = false, escape = false, erase = false;
};

// A controller button as printed on it: SDL_CONTROLLER_BUTTON_A is the button labelled A. On a PC
// SDL does that for Nintendo controllers itself; SDL on the Switch numbers the Switch's own by
// Xbox positions (its "A" is the bottom button, labelled B; its "X" the left one, labelled Y), so
// those two pairs are swapped back there, as Pokemon DS Widescreen does. (Its own inverse.)
inline int Labelled(int b)
{
#if defined(__SWITCH__)
    switch (b)
    {
    case SDL_CONTROLLER_BUTTON_A: return SDL_CONTROLLER_BUTTON_B;
    case SDL_CONTROLLER_BUTTON_B: return SDL_CONTROLLER_BUTTON_A;
    case SDL_CONTROLLER_BUTTON_X: return SDL_CONTROLLER_BUTTON_Y;
    case SDL_CONTROLLER_BUTTON_Y: return SDL_CONTROLLER_BUTTON_X;
    default: break;
    }
#endif
    return b;
}

struct Pads
{
    std::vector<SDL_GameController*> list;
    // (Held directions on any controller, with auto-repeat for the menus.)
    int heldDir = -1;
    Uint32 heldSince = 0, lastRepeat = 0;

    void Open(int index)
    {
        if (!SDL_IsGameController(index)) return;
        if (SDL_GameController* c = SDL_GameControllerOpen(index)) list.push_back(c);
    }
    void CloseAll()
    {
        for (SDL_GameController* c : list) SDL_GameControllerClose(c);
        list.clear();
    }
    void Close(SDL_JoystickID id)
    {
        for (size_t i = 0; i < list.size(); i++)
            if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(list[i])) == id)
            {
                SDL_GameControllerClose(list[i]);
                list.erase(list.begin() + (long)i);
                return;
            }
    }
    // (b: as labelled, see Labelled.)
    bool Button(SDL_GameControllerButton b) const
    {
        for (SDL_GameController* c : list)
            if (SDL_GameControllerGetButton(c, (SDL_GameControllerButton)Labelled(b))) return true;
        return false;
    }
    int Axis(SDL_GameControllerAxis a) const
    {
        int best = 0;
        for (SDL_GameController* c : list)
        {
            const int v = SDL_GameControllerGetAxis(c, a);
            if (abs(v) > abs(best)) best = v;
        }
        return best;
    }
    // Directions on the D-pads and left sticks: bits up, down, left, right.
    int Dirs() const
    {
        int d = 0;
        const int dead = 16000;
        if (Button(SDL_CONTROLLER_BUTTON_DPAD_UP) || Axis(SDL_CONTROLLER_AXIS_LEFTY) < -dead) d |= 1;
        if (Button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || Axis(SDL_CONTROLLER_AXIS_LEFTY) > dead) d |= 2;
        if (Button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || Axis(SDL_CONTROLLER_AXIS_LEFTX) < -dead) d |= 4;
        if (Button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || Axis(SDL_CONTROLLER_AXIS_LEFTX) > dead) d |= 8;
        return d;
    }
    // The menus' directions from the controllers, repeating while held.
    void MenuDirs(MenuIn& in)
    {
        const int d = Dirs();
        int dir = (d & 1) ? 0 : (d & 2) ? 1 : (d & 4) ? 2 : (d & 8) ? 3 : -1;
        const Uint32 now = SDL_GetTicks();
        bool fire = false;
        if (dir != heldDir)
        {
            heldDir = dir;
            heldSince = lastRepeat = now;
            fire = dir >= 0;
        }
        else if (dir >= 0 && now - heldSince > 400 && now - lastRepeat > 90)
        {
            lastRepeat = now;
            fire = true;
        }
        if (!fire) return;
        if (dir == 0) in.up = true;
        if (dir == 1) in.down = true;
        if (dir == 2) in.left = true;
        if (dir == 3) in.right = true;
    }
};

// A note for whoever is looking: stderr, or Android's log (logcat) there.
void Note(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
#if defined(__ANDROID__)
    SDL_Log("%s", buf);
#else
    fprintf(stderr, "%s\n", buf);
#endif
}

// A test script: "key FRAME NAME [HOLD]", "touch FRAME X,Y [HOLD]" (0 to 1 across the window),
// "shot FRAME FILE.bmp", "quit FRAME". Frames are the program's own, one GBA frame each; in
// real-time runs (GBAW_TEST_REALTIME) they are the game's frames from its start, which come at
// the screen's pace.
struct ScriptLine
{
    int frame;
    std::string what, arg;
    int hold;
    int done = 0; // 1: pressed (or done), 2: let go
};

// ---------------------------------------------------------------------------------------------

// RetroAchievements' map of a GBA: 32 KB of internal work RAM from 0, 256 KB of external work
// RAM from 0x8000, and 64 KB of the save RAM currently mapped from 0x48000.
#ifdef GBAW_CHEATS
// The running game for the cheats: mGBA's own reads and writes (a write to the ROM is a patch).
struct CheatBus : GbaCheats::Bus
{
    struct mCore* core = nullptr;
    uint8_t R8(uint32_t a) override { return (uint8_t)core->rawRead8(core, a, -1); }
    uint16_t R16(uint32_t a) override { return (uint16_t)core->rawRead16(core, a, -1); }
    uint32_t R32(uint32_t a) override { return core->rawRead32(core, a, -1); }
    void W8(uint32_t a, uint8_t v) override { core->rawWrite8(core, a, -1, v); }
    void W16(uint32_t a, uint16_t v) override { core->rawWrite16(core, a, -1, v); }
    void W32(uint32_t a, uint32_t v) override { core->rawWrite32(core, a, -1, v); }
};
#endif

uint32_t ReadRa(void* p, uint32_t address, uint8_t* buffer, uint32_t bytes)
{
    auto* core = (struct mCore*)p;
    if (!core || !core->board || !buffer) return 0;
    auto* gba = (struct GBA*)core->board;
    const uint8_t* iw = (const uint8_t*)gba->memory.iwram;
    const uint8_t* ew = (const uint8_t*)gba->memory.wram;
    const uint8_t* save = nullptr;
    size_t saveN = 0;
    if (gba->memory.savedata.currentBank)
    {
        // Flash: the 64 KB bank the bus shows (Pokemon's save is 128 KB of flash, one bank at a time).
        save = gba->memory.savedata.currentBank;
        saveN = 0x10000;
    }
    else if (gba->memory.savedata.data)
    {
        save = gba->memory.savedata.data;
        const size_t n = GBASavedataSize(&gba->memory.savedata);
        saveN = n < 0x10000 ? n : 0x10000;
    }
    uint32_t done = 0;
    for (; done < bytes; done++)
    {
        const uint32_t a = address + done;
        if (a < 0x8000)
        {
            if (!iw) break;
            buffer[done] = iw[a];
        }
        else if (a < 0x48000)
        {
            if (!ew) break;
            buffer[done] = ew[a - 0x8000];
        }
        else if (a < 0x58000)
        {
            const uint32_t o = a - 0x48000;
            if (!save || o >= saveN) break;
            buffer[done] = save[o];
        }
        else
            break;
    }
    return done;
}

struct App
{
    // Where things are
    std::string data, roms, saves, states;
    Settings set;

    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* gameTex = nullptr;   // the GBA picture, MaxW x 160
    SDL_Texture* zoomTex = nullptr;   // ZOOM / LOOK AROUND: the window shown, up to 800 x 800
    SDL_Texture* zoomHdTex = nullptr; // ... through the HD filter (grown to the largest asked for)
    int zoomHdW = 0, zoomHdH = 0;
    SDL_Texture* bigTex = nullptr;    // the same scaled up by a whole number (SCALING: SHARP)
    SDL_Texture* hdTex = nullptr;     // HD FILTER: the picture redrawn by xBRZ, HdFactor times as big
    SDL_Texture* lcdTex = nullptr;    // LCD EFFECT: the grid, the size the picture is drawn at
    int lcdW = 0, lcdH = 0, lcdCols = 0, lcdRows = 0;
    std::vector<uint32_t> fxSrc, fxOut, colourLut;
    // Helpers for HD FILTER: xBRZ shares the picture out among the processor's cores by rows.
    struct Workers
    {
        std::vector<std::thread> threads;
        std::mutex m;
        std::condition_variable cv, done;
        std::function<void(int)> job;
        int jobs = 0, next = 0, left = 0;
        bool quit = false;
        void Start(int n)
        {
            for (int i = 0; i < n; i++) threads.emplace_back([this, i] { Loop(i); });
        }
        void Loop(int index)
        {
#if defined(__SWITCH__)
            // (A thread starts on the program's own core there: each helper to a core of its own.)
            svcSetThreadCoreMask(CUR_THREAD_HANDLE, 1 + index % 2, 1u << (1 + index % 2));
#else
            (void)index;
#endif
            std::unique_lock<std::mutex> l(m);
            for (;;)
            {
                cv.wait(l, [&] { return quit || next < jobs; });
                if (quit) return;
                const int i = next++;
                l.unlock();
                job(i);
                l.lock();
                if (--left == 0) done.notify_all();
            }
        }
        void Run(int n, const std::function<void(int)>& f)
        {
            std::unique_lock<std::mutex> l(m);
            job = f;
            jobs = n;
            next = 0;
            left = n;
            cv.notify_all();
            while (next < jobs)
            {
                const int i = next++;
                l.unlock();
                job(i);
                l.lock();
                if (--left == 0) done.notify_all();
            }
            done.wait(l, [&] { return left == 0; });
            jobs = next = 0;
        }
        ~Workers()
        {
            {
                std::lock_guard<std::mutex> l(m);
                quit = true;
            }
            cv.notify_all();
            for (std::thread& t : threads) t.join();
        }
    } workers;
    bool workersStarted = false;
    int bigW = 0, bigH = 0;
    SDL_Texture* uiTex = nullptr;
    SDL_Texture* glassTex = nullptr; // (built into Fulldex: the glass pause menu, the window's size)
    int glassTexW = 0, glassTexH = 0;
    int uiTexW = 0, uiTexH = 0;
    Canvas ui;
    bool testMode = false;
    // Built into the all-in-one launcher (embed.h): no list of its own; QUIT GAME ends Run.
    bool embedded = false;
    std::string listCache;   // what each ROM is (Games::Scan): saves/rom-list.txt
    bool realTime = true;  // paced and drawn as for play (tests: only with GBAW_TEST_REALTIME)
    int maxTex = 8192;
    unsigned long long gameFrames = 0;
    bool shotTaken = false;

    enum Screen { LIST, SETTINGS, PLAY } screen = LIST;
    Screen settingsBack = LIST;
    std::vector<Games::Game> games;
    int listSel = 0;
    int listCols = 3; // (the game list's columns, as last drawn: up and down move by a row)
    int settingsSel = 0;
#if defined(__SWITCH__)
    bool padLast = true; // the last input was a controller's (the hints follow it)
#else
    bool padLast = false;
#endif
    bool leftTrigger = false;
    bool needFolder = false;      // Android: no ROM folder chosen yet
    bool pickingFolder = false;   // Android: the folder picker is open
    // Touch: the on-screen controls (shown while the last input was a touch), the buttons each
    // finger is on, and where the menus' rows were drawn (for taps), in canvas pixels.
#if defined(GBAW_PHONE)
    bool touchUi = true;
#else
    bool touchUi = false;
#endif
    Touch::Layout touchLayout;
    TouchLayout::Profile touchProfile;
    std::map<SDL_FingerID, uint32_t> fingers;
    uint32_t touchHeld = 0;
    // A button touched and let go before the game's next frame still reaches the game, for a
    // few frames (a quick tap would otherwise come and go between two of its frames).
    uint32_t touchTap = 0;
    int touchTapFrames = 0;
    int uiScale = 1, uiOffX = 0, uiOffY = 0;
    std::vector<std::pair<UiRect, int>> tapRects; // (where, which item)
    // The games' art on the list: drawn at the screen's own resolution over the menus' canvas.
    struct ArtSpot { UiRect at; std::string code; bool chosen; };
    std::vector<ArtSpot> artSpots;
    std::map<std::string, SDL_Texture*> artTex;
    UiRect backRect, settingsRect;

    // The game
    struct mCore* core = nullptr;
    Games::Game game;
    std::vector<uint32_t> video = std::vector<uint32_t>((size_t)MaxW * GbaH);
    // ZOOM and the look-around (pokedaisy/): the game known to WIDE (by its ROM's CRC-32 before the widescreen
    // patch), the camera, the frame's middle 240 x 160 as WIDE wants it, and the window drawn after the last frame
    // (shown instead of the picture while zoomShown: past what the widescreen patch draws).
    bool pkWide = false;
    // GAMEPLAY (gameplay.h): the ROM's writes for INSTANT TEXT and REUSABLE TMS, which of them are in, the game's wild
    // encounter switch, and whether pk_wide.c knows this game's field (RUN INDOORS, WILD ENCOUNTERS).
    std::vector<GbaGameplay::Write> gpText, gpTms;
    bool gpTextOn = false, gpTmsOn = false, gpField = false;
    uint32_t gpWild = 0;
    GbaGameplay::ShinyRoller shiny;
    int gpState = 0; // (the last frame's PK_FIELD_* bits)
    float tintNow[3] = { 256, 256, 256 };
    int tintSent[3] = { 256, 256, 256 }, tintFrames = 0, dnLog = 0;
    PkWideCamera::Camera camera;
    std::vector<uint32_t> frame240 = std::vector<uint32_t>((size_t)GbaW * GbaH);
    std::vector<uint32_t> zoomBuf = std::vector<uint32_t>((size_t)PkWideCamera::Camera::MaxW * PkWideCamera::Camera::MaxH);
    PkWideCamera::Window zoomWin;
    bool zoomShown = false;
    int wideExtra = -1;
    bool paused = false;
    Menu pause;
    bool fastToggle = false;
    FastForwardButton fastButton;
    bool fastInputFocused = true;
    uint32_t tapDirs = 0;
    Uint32 tapSince = 0;
    bool plusHeld = false, plusCombo = false; // START (+) held, and SELECT (-) pressed with it
    bool comboOn = false;                     // START and SELECT pressed together, till both are let go
    Uint32 plusAt = 0, minusAt = 0;           // when START, SELECT went down
    Uint32 plusPulse = 0, minusPulse = 0;     // a short tap of one alone: given to the game till then
#ifdef GBAW_EMBEDDED
    // RetroAchievements, from the program this is built into (embed.h). None in the standalone game.
    const GbaWide::AchievementsHost* ra = nullptr;
    bool raReady = false;
    enum PausePage { P_ROOT, P_RA, P_LIST, P_LOGIN, P_CHEATS, P_DISPLAY, P_SOUND, P_CONTROLS, P_SETTINGS, P_QUIT, P_RESET, P_SAVE, P_LOAD, P_DEX, P_DEXENTRY, P_COMP, P_PARTY, P_BAG, P_PLACE, P_BATTLE, P_GUIDE, P_GAMEPLAY, P_FLY, P_FLYMON, P_BOXES, P_BOXMON, P_PARTYMON, P_ITEM, P_ITEMMON, P_ITEMMOVE } pausePage = P_ROOT;
    // POKEDEX (gba_dex.h): the ROM as it was before the widescreen patch, read when first opened;
    // the species of each of its list's rows, the one whose page is open, and that one's pictures.
    std::vector<uint8_t> dexRom;
    GbaDex::Data dex;
    bool dexTried = false;
    std::vector<int> dexRows;
    int dexSpecies = 0, dexPicFor = -1;
    GbaDex::Picture dexPic, dexShiny;
    // COMPANION (gba_companion.h): this game's addresses (none: not a game it knows), what was read
    // when the pause menu opened, and each party Pokemon's picture.
    const GbaCompanion::Config* compConfig = nullptr;
    GbaCompanion::Snapshot comp;
    std::vector<GbaDex::Picture> compPics;
    GbaDex::Picture foePic;
    std::vector<bool> guideShown; // (GUIDE: the answers chosen, while this game runs)
    std::string gameCode;
    // The pause menu as the DS games draw it (gba_glass.h): the page made from `pause` (BuildGlass),
    // which of pause's rows each of its items is (-1: the page's own BACK), the item chosen on it
    // (-1: the one for pause.selected), the paused game's picture it is drawn over, and a tap's
    // slider value or choice card for the chosen setting.
    GbaGlass::Page glass;
    // FULLDEX NAV (gba_glass.h, fulldex_nav.h): the DS games' watch over the running game, L3 (a click of the left
    // stick) or N. Its ENCOUNTERS count is kept beside the save states (<game>.counter: the count, then whether shown).
    bool navOpen = false, navToggle = false, navCountShown = false, navBattleWas = false, navCountLoaded = false;
    int navSel = 0, navCount = 0;
    Uint32 navResetAsk = 0;
    uint32_t navKeysWas = 0;
    unsigned long long navBattleAt = 0;
    SDL_Texture* navTex = nullptr;
    int navTexW = 0, navTexH = 0;
    std::vector<int> glassRow;
    int glassSel = -1;
    PausePage glassPage = P_ROOT;
    std::vector<uint32_t> glassShot;
    int glassShotW = 0, glassShotH = 0;
    int glassSlider = -1, glassPick = -1;
    std::vector<const Setting*> pageSettings; // the settings on the DISPLAY, SOUND or CONTROLS page, row by row
    bool typing = false;
    int raField = 0; // 0 the user name, 1 the password
    std::string raUser, raPass;
    std::vector<std::string> achDesc; // the achievements list's descriptions, beside its rows
#endif
#ifdef GBAW_CHEATS
    GbaCheats::Engine cheats;
    CheatBus cheatBus;
    std::string cheatsFile; // which are on: <cheats folder>/<game>-on.txt
#endif
    // After a menu closes, the game gets no keys until all are let go (Enter or A choosing in
    // the pause menu would otherwise press the game's START or A).
    bool holdKeys = false;

    // Sound
    SDL_AudioDeviceID audio = 0;
    SDL_AudioSpec have{};
    struct mAudioResampler resampler{};
    struct mAudioBuffer audioOut{};
    std::vector<int16_t> audioTmp;

    // Input
    Pads pads;
    bool key[SDL_NUM_SCANCODES] = {};

    // Messages and the FPS counter
    std::string toast;
    Uint32 toastUntil = 0;
    int fpsFrames = 0, fpsShown = 0;
    Uint32 fpsSince = 0;

    // Tests
    std::vector<ScriptLine> script;
    int frame = 0;
    bool quit = false;

    void Toast(const std::string& t, Uint32 ms = 2500)
    {
        toast = t;
        toastUntil = SDL_GetTicks() + ms;
        fprintf(stderr, "%s\n", t.c_str());
    }

    // -----------------------------------------------------------------------------------------
    // Folders

    void Folders(const std::string& dataArg)
    {
#if defined(__SWITCH__)
        data = "/switch/pokemon-gba-wide";
#elif defined(__ANDROID__)
        // The folder the player chose for their ROMs holds everything (saves, states,
        // settings); until one is chosen, the app's own folder.
        data = Android::RomFolder();
        needFolder = data.empty();
        if (needFolder)
        {
            const char* ext = SDL_AndroidGetExternalStoragePath();
            data = ext ? ext : ".";
        }
#else
        if (!dataArg.empty())
            data = dataArg;
        else if (getenv("APPIMAGE"))
        {
            // (An AppImage's own folder can't be written to.)
            char* pref = SDL_GetPrefPath("", "pokemon-gba-wide");
            data = pref ? pref : ".";
            SDL_free(pref);
        }
        else
        {
            char* base = SDL_GetBasePath();
            data = base ? base : ".";
            SDL_free(base);
        }
#endif
        if (!dataArg.empty()) data = dataArg;
        while (data.size() > 1 && (data.back() == '/' || data.back() == '\\')) data.pop_back();
        roms = data + "/roms";
#if defined(__ANDROID__)
        if (!needFolder) roms = data;
#endif
        saves = data + "/saves";
        states = data + "/states";
        Games::MakeDir(data);
        Games::MakeDir(roms);
        Games::MakeDir(saves);
        Games::MakeDir(states);
        set.Load(data + "/settings.ini");
        touchProfile.Load(data + "/touch-layout-gba.cfg");
        listCache = saves + "/rom-list.txt";
    }

    // The launcher's folders (embed.h): its roms and saves, and this one's settings and save
    // states in data, named so as not to be taken for the launcher's own.
    void UseFolders(const std::string& romsDir, const std::string& savesDir, const std::string& dataDir)
    {
        data = dataDir;
        roms = romsDir;
        saves = savesDir;
        states = data + "/states";
        needFolder = false;
        Games::MakeDir(saves);
        Games::MakeDir(states);
        set.Load(data + "/gba-settings.ini");
        touchProfile.Load(data + "/touch-layout-gba.cfg");
        listCache = saves + "/gba-rom-list.txt";
    }

    // -----------------------------------------------------------------------------------------
    // The window

    // fullscreen: -1 as the settings say (or as the window size: none, the settings'), else as given.
    bool OpenWindow(int w, int h, int fullscreen = -1)
    {
        Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#if defined(__SWITCH__) || defined(GBAW_PHONE)
        flags |= SDL_WINDOW_FULLSCREEN;
        (void)fullscreen;
#else
        if (fullscreen > 0 || (fullscreen < 0 && !w && set.Int("fullscreen"))) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
        if (!w)
        {
            w = 1280;
            h = 720;
        }
        win = SDL_CreateWindow(embedded ? "Pokemon Widescreen" : "Pokemon GBA Widescreen", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
        if (!win) return false;
#if !defined(__SWITCH__) && !defined(GBAW_PHONE)
        if (SDL_Surface* icon = SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_ARGB8888))
        {
            for (int y = 0; y < 64; y++)
                for (int x = 0; x < 64; x++) ((uint32_t*)((uint8_t*)icon->pixels + y * icon->pitch))[x] = IconPixel(x, y, 64);
            SDL_SetWindowIcon(win, icon);
            SDL_FreeSurface(icon);
        }
#endif
        if (!realTime)
            // (Tests draw in software, unless GBAW_TEST_GPU asks for the graphics card's path.)
            ren = SDL_CreateRenderer(win, -1, (getenv("GBAW_TEST_GPU") ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE) | SDL_RENDERER_TARGETTEXTURE);
        else
        {
            ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
            if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
            if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
#if defined(__linux__) && !defined(__ANDROID__)
            // Linux on a screen at 120, 180 or 240 Hz (a Steam Deck docked at 120, #173): the driver
            // holds each picture for 2, 3 or 4 refreshes, so they come back 60 a second and Run()
            // settles on a frame each. Showing every refresh and running a frame every 2nd, the
            // pictures came back unevenly there, the rate check gave up and the clock ran it at 40.
            // A 60 Hz screen is as before. SSR_SWAP_PACE=0 keeps the old way.
            SDL_DisplayMode dm;
            const int hz = ren && !SDL_GetWindowDisplayMode(win, &dm) ? dm.refresh_rate : 0;
            const int n = (hz + 30) / 60;
            if (n > 1 && std::abs(hz - n * 60) <= 1 && !(getenv("SSR_SWAP_PACE") && getenv("SSR_SWAP_PACE")[0] == '0'))
                fprintf(stderr, "[gba] %d Hz: swap interval %d: %s\n", hz, n,
                        SDL_GL_SetSwapInterval(n) == 0 ? "the driver paces the pictures" : SDL_GetError());
#endif
        }
        if (!ren) return false;
        SDL_RendererInfo info;
        if (!SDL_GetRendererInfo(ren, &info) && info.max_texture_width > 0)
            maxTex = std::min(info.max_texture_width, info.max_texture_height);
        gameTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, MaxW, GbaH);
        if (!gameTex) return false;
        SDL_SetTextureScaleMode(gameTex, SDL_ScaleModeNearest);
        SDL_ShowCursor(SDL_ENABLE);
        return true;
    }

    void ToggleFullscreen()
    {
#if !defined(__SWITCH__) && !defined(GBAW_PHONE)
        const bool full = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_SetWindowFullscreen(win, full ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        set.values["fullscreen"] = full ? "0" : "1";
        set.Save();
#endif
    }

    // -----------------------------------------------------------------------------------------
    // Sound: the game's samples, made at 60 frames a second, resampled to the device's rate,
    // with the rate nudged to keep about four frames queued, or two of the device's own buffers
    // if those are bigger (the Switch plays sound in larger pieces than a PC: with only four
    // frames queued it ran dry between them, and anything over ten frames was thrown away,
    // both heard as crackling).

    void OpenAudio()
    {
        SDL_AudioSpec want{};
        want.freq = 48000;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
#if defined(__SWITCH__)
        // (The Switch: with 512-sample buffers its sound thread ran dry between them and played
        // about 41,000 samples a second instead of 48,000, so sound piled up and was thrown away:
        // crackly and sped up. The DS games use 1024.)
        want.samples = 2048;
#else
        want.samples = 512;
#endif
        audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        if (!audio) return;
        fprintf(stderr, "[gba] sound: %d Hz, %d samples a buffer\n", have.freq, (int)have.samples);
        mAudioBufferInit(&audioOut, 16384, 2);
        audioTmp.resize(16384 * 2);
    }

    bool ffWas = false;
    FastForwardAudio ffAudio; // sound made while fast forwarding, played at its own speed
    // FAST FORWARD MUSIC (ff_music_gba.h): each song recorded beforehand on a hidden copy of the game, played at its
    // own speed while the game fast forwards; ffKey is the song playing now, from ffKeyAt (SDL ticks).
    GbaFfMusic::Renderer ffMusic;
    GbaFfMusic::Player ffPlayer;
    uint32_t ffKey = 0, ffKeyAt = 0;
    void Sound(bool queue, bool ff = false)
    {
        if (!core) return;
        struct mAudioBuffer* buf = core->getAudioBuffer(core);
        if (!audio)
        {
            mAudioBufferClear(buf);
            return;
        }
        const double ratio = mCoreCalculateFramerateRatio(core, Fps); // (59.73 / 60)
        const double frameBytes = have.freq / Fps * 4;
        const double queued = SDL_GetQueuedAudioSize(audio);
        const double deviceBytes = have.samples * 4.0;
        const double target = std::max(frameBytes * 4, deviceBytes * 2);
        if (!ff) ffPlayer.key = 0; // (the next fast forward picks the song up where it is then)
        if (ff != ffWas)
        {
            // (Letting go drops what waited to be heard; the short device queue plays on.)
            ffAudio.Clear();
            if (ff && set.Get("ff_mute") == "1") SDL_ClearQueuedAudio(audio);
            ffWas = ff;
        }
        if (ff && set.Get("ff_mute") == "1") queue = false;
        double nudge = ff ? 0 : (target - queued) / target * 0.005;
        nudge = std::max(-0.005, std::min(0.005, nudge));
        mAudioResamplerSetSource(&resampler, buf, core->audioSampleRate(core) / ratio, true);
        mAudioResamplerSetDestination(&resampler, &audioOut, have.freq * (1 + nudge));
        mAudioResamplerProcess(&resampler);
        size_t n = mAudioBufferRead(&audioOut, audioTmp.data(), audioTmp.size() / 2);
        std::shared_ptr<const GbaFfMusic::Clip> clip;
        bool waitClip = false;
        if (ff && queue && ffMusic.Supported() && ffKey)
        {
            clip = ffMusic.Get(ffKey);
            waitClip = !clip && ffMusic.WillHave(ffKey);
        }
        if (clip)
        {
            // The song's own recording, from where it would be had it played at 1x since it started.
            const int want = (int)std::max(0.0, (target * 1.5 - queued) / 4);
            if (ffPlayer.key != ffKey || ffPlayer.clip != clip)
                ffPlayer.Begin(clip, ffKey, (size_t)((uint64_t)(SDL_GetTicks() - ffKeyAt) * (uint64_t)have.freq / 1000));
            n = (size_t)std::min(want, (int)(audioTmp.size() / 2));
            ffPlayer.Fill(audioTmp.data(), (int)n, std::max(0, std::min(256, set.Int("volume") * 256 / 100)));
        }
        else if (waitClip)
            n = 0; // (still being recorded: silence rather than the music rushed)
        else if (ff)
        {
            // Fast forward: the sound at its own speed and pitch, a stretch at a time, never more
            // than about two seconds behind the picture (FastForwardAudio). The device's queue is
            // kept topped up from it, so that queue is the clock.
            if (queue) ffAudio.Push(audioTmp.data(), (int)n, have.freq);
            const int want = (int)std::max(0.0, (target * 1.5 - queued) / 4);
            n = queue ? (size_t)ffAudio.PullFrames(audioTmp.data(), std::min(want, (int)(audioTmp.size() / 2)), have.freq) : 0;
        }
        const double room = std::max(frameBytes * 10, target * 3);
        if (queue && n && queued < room)
        {
            SDL_QueueAudio(audio, audioTmp.data(), (Uint32)(n * 4));
            // (Tests: GBAW_AUDIO_DUMP=file, everything sent to the device, raw 16-bit stereo.)
            static FILE* dump = getenv("GBAW_AUDIO_DUMP") ? fopen(getenv("GBAW_AUDIO_DUMP"), "wb") : nullptr;
            if (dump) fwrite(audioTmp.data(), 4, n, dump);
        }
    }

    // -----------------------------------------------------------------------------------------
    // The game

    bool Start(const Games::Game& g)
    {
        std::vector<uint8_t> rom, sold;
        std::string error;
#ifdef GBAW_EMBEDDED
        const bool wantRa = ra && ra->begin;
#else
        const bool wantRa = false;
        (void)sold;
#endif
        // (A hack whose widescreen patch doesn't fit: tried anyway, unless the GBA's own shape
        // was chosen, which is how to play it as it is when it glitches.)
        const std::string shape = set.Get("widescreen");
        const bool ownShape = shape == "off" || shape == "3:2" || shape == "4:3";
        // (sold: the ROM before the widescreen patch, also what ZOOM knows the game by.)
        if (!Games::Prepare(g, rom, error, &sold, !ownShape))
        {
            Toast(error, 5000);
            return false;
        }
        core = mCoreCreate(mPLATFORM_GBA);
        if (!core || !core->init(core))
        {
            Toast("CAN'T START THE EMULATOR");
            core = nullptr;
            return false;
        }
        mCoreInitConfig(core, nullptr);
        struct mCoreOptions opts{};
        opts.useBios = false;
        opts.skipBios = true;
        opts.volume = 0x100;
        opts.sampleRate = 48000;
        opts.audioBuffers = 4096;
        mCoreConfigLoadDefaults(&core->config, &opts);
        mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "detect");
        wideExtra = WantedExtra();
        mCoreConfigSetIntValue(&core->config, "gba.wideExtra", wideExtra);
        mCoreLoadConfig(core);
        core->setVideoBuffer(core, video.data(), MaxW);
        core->setAudioBufferSize(core, 4096);
        struct VFile* vf = VFileMemChunk(rom.data(), rom.size());
        if (!vf || !core->loadROM(core, vf))
        {
            Toast("CAN'T LOAD THE ROM");
            core->deinit(core);
            core = nullptr;
            return false;
        }
        // The save: saves/<game>.sav, or one put beside the ROM, copied in the first time.
        const std::string save = saves + "/" + g.key + ".sav";
        ExploredLoad(saves + "/" + g.key + ".explored");
        if (!Games::Exists(save) && Games::Exists(Games::SaveBeside(g)))
        {
            std::vector<uint8_t> s;
            if (Games::ReadFile(Games::SaveBeside(g), s, 1 << 20)) Games::WriteFile(save, s);
        }
        if (struct VFile* sv = VFileOpen(save.c_str(), O_CREAT | O_RDWR)) core->loadSave(core, sv);
        core->reset(core);
#ifdef GBAW_CHEATS
        cheatBus.core = core;
        cheatsFile = GbaCheats::FolderFor(saves) + "/" + g.key + "-on.txt";
        // (Test scripts play test builds of the games, which no one else has: taken as known.)
        if (cheats.Start(cheatBus, Games::Header(rom, 0xAC, 4), rom.size() > 0xBC ? rom[0xBC] : 0, g.known || testMode))
        {
            cheats.Load(cheatsFile);
            fprintf(stderr, "[gba] cheats: %s, %d on\n", cheats.GameName(), cheats.OnCount());
        }
        CheatsToHost();
#endif
        SetVolume();
        dexRom = sold.empty() ? rom : sold;
        objGfxTable = 0;
        objPalTable = 0;
        objPalCount = 0;
        iconTable = 0;
        compConfig = rom.size() > 0xBD ? GbaCompanion::ConfigFor((const char*)rom.data() + 0xAC, rom[0xBC]) : nullptr;
        gameCode = rom.size() > 0xB0 ? std::string((const char*)rom.data() + 0xAC, 4) : std::string();
        guideShown.assign(GbaGuides::For(gameCode.c_str()).size(), false);
        comp = GbaCompanion::Snapshot();
        dex = GbaDex::Data();
        dexTried = false;
        dexPicFor = -1;
        pkWide = pk_wide_configure(doCrc32(sold.data(), sold.size())) != 0;
        {
            // (GAMEPLAY: what this ROM's own code lets through; INSTANT TEXT and REUSABLE TMS written now.)
            const std::vector<uint8_t>& id = sold.empty() ? rom : sold;
            gpText = GbaGameplay::InstantText(rom);
            gpTms = GbaGameplay::ReusableTms(rom);
            gpWild = GbaGameplay::WildSwitch(rom);
            gpTextOn = gpTmsOn = false;
            shiny.Reset();
            gpState = 0;
            for (int c = 0; c < 3; c++) { tintNow[c] = 256; tintSent[c] = 256; }
            followSpecies = -1;
            followShown = false;
            followTick = 0;
            pk_follower_set(nullptr, nullptr);
            gpField = pk_field_configure(doCrc32(id.data(), id.size())) != 0;
            fly.Find(core);
            flyStage = 0;
            FlightFind(sold.empty() ? rom : sold);
            boxes.Find(core, sold.empty() ? rom : sold, fly.Table());
            items.Find(core, sold.empty() ? rom : sold, fly.Table());
            if (gpField) pk_field_wild(gpWild, WildKeep() == 0);
            ApplyGameplay();
            ffKey = 0;
            ffPlayer = GbaFfMusic::Player();
            if (audio)
            {
                char sub[24];
                snprintf(sub, sizeof sub, "/ffmusic/%08x", (unsigned)doCrc32(rom.data(), rom.size()));
                Games::MakeDir(data + "/ffmusic");
                Games::MakeDir(data + sub);
                ffMusic.Start(rom, have.freq, data + sub);
            }
            fprintf(stderr, "[gba] gameplay: text %zu, tms %zu, wild %08x, field %d\n", gpText.size(), gpTms.size(), gpWild, (int)gpField);
        }
        camera.Reset();
        camera.level = 0;
        zoomShown = false;
        fprintf(stderr, "[gba] zoom: %s\n", pkWide ? "this game is known" : "off for this game");
        game = g;
        screen = PLAY;
        paused = false;
        fastToggle = set.Get("fast_lock") == "1";
        holdKeys = true;
        mAudioResamplerInit(&resampler, mINTERPOLATOR_SINC);
        if (audio)
        {
            SDL_ClearQueuedAudio(audio);
            SDL_PauseAudioDevice(audio, 0);
        }
        if (g.patch.empty()) Toast("NO WIDESCREEN FOR THIS ROM: " + g.why, 4000);
        else if (g.forced && !ownShape) Toast("ROM HACK: GLITCHY SIDES? TRY ASPECT RATIO 3:2", 6000);
#ifdef GBAW_EMBEDDED
        // The ROM RetroAchievements knows is the one sold (sold), not the widescreen patch.
        if (wantRa)
        {
            const std::vector<uint8_t>& id = sold.empty() ? rom : sold; // (the ROM as sold: before the widescreen patch)
            raReady = ra->begin(ra->ud, id.data(), id.size(), core, ReadRa);
        }
#endif
        return true;
    }

    void Stop()
    {
        if (!core) return;
#ifdef GBAW_EMBEDDED
        if (raReady && ra->end) ra->end(ra->ud);
        raReady = false;
        if (typing) SDL_StopTextInput();
        typing = false;
#endif
        if (audio)
        {
            SDL_PauseAudioDevice(audio, 1);
            SDL_ClearQueuedAudio(audio);
        }
        mAudioResamplerDeinit(&resampler);
        ffMusic.Stop();
        fly.Forget();
        boxes.Forget();
        items.Forget();
        ExploredSave();
        pk_flight_configure(nullptr, 0);
        pk_flight_reset();
        flightRec = nullptr;
        flyStage = 0;
        ffPlayer = GbaFfMusic::Player();
        if (followFind.joinable()) followFind.join(); // (the look for HeartGold: a second or so at most)
        core->deinit(core); // (writes the save)
        core = nullptr;
        if (embedded)
        {
            quit = true; // (back to the launcher's list)
            return;
        }
        screen = LIST;
        Rescan();
    }

    // A test script (--script, or GBAW_TEST_SCRIPT when built into the launcher): test mode.
    bool LoadScript(const std::string& path)
    {
        testMode = true;
        realTime = getenv("GBAW_TEST_REALTIME") != nullptr;
        std::vector<uint8_t> s;
        if (!Games::ReadFile(path, s, 1 << 20))
        {
            fprintf(stderr, "can't read %s\n", path.c_str());
            return false;
        }
        std::string text(s.begin(), s.end());
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            char what[16] = {}, arg[512] = {};
            int f = 0, hold = 2;
            const int got = sscanf(text.substr(pos, end - pos).c_str(), "%15s %d %511s %d", what, &f, arg, &hold);
            if (got >= 2 && what[0] != '#') script.push_back({ f, what, arg, got >= 4 ? hold : 2 });
            pos = end + 1;
        }
        return true;
    }

    // Everything SDL made for this, let go (embedded: the launcher's list comes next, and maybe
    // another game).
    void Close()
    {
        if (core) Stop();
        for (auto& kv : artTex)
            if (kv.second) SDL_DestroyTexture(kv.second);
        artTex.clear();
        for (SDL_Texture** t : { &uiTex, &glassTex, &bigTex, &gameTex, &hdTex, &lcdTex, &zoomTex, &zoomHdTex })
            if (*t)
            {
                SDL_DestroyTexture(*t);
                *t = nullptr;
            }
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        ren = nullptr;
        win = nullptr;
        if (audio) SDL_CloseAudioDevice(audio);
        audio = 0;
        pads.CloseAll();
    }

    // A controller's A is the button labelled A (CONTROLLER BUTTONS), always on the Switch.
    bool ByLabel() const
    {
#if defined(__SWITCH__)
        return true;
#else
        return set.Get("buttons") != "positions";
#endif
    }

    void SetVolume()
    {
        if (!core) return;
        mCoreConfigSetIntValue(&core->config, "volume", set.Int("volume") * 0x100 / 100);
        core->reloadConfigOption(core, "volume", nullptr);
    }

    // Ten states a game, as the DS games have (SAVE and LOAD in the pause menu each open the
    // slots). The first is the one state there was before: <game>.ss; the others <game>.2.ss ...
    static constexpr int StateSlots = 10;
    std::vector<uint32_t> slotThumb[StateSlots]; // (the slots' pictures while SAVE or LOAD is open)
    int slotThumbW[StateSlots] = {}, slotThumbH[StateSlots] = {};
    std::string StatePath(int slot = 0) const { return states + "/" + game.key + (slot > 0 ? "." + std::to_string(slot + 1) : std::string()) + ".ss"; }
    // When the slot was saved ("12 NOV 21:05"); empty: nothing in it.
    std::string StateDate(int slot) const
    {
        struct stat st;
        if (stat(StatePath(slot).c_str(), &st) != 0) return "";
        char buf[32] = "";
        const time_t t = st.st_mtime;
        if (const tm* lt = localtime(&t)) strftime(buf, sizeof(buf), "%d %b %H:%M", lt);
        std::string out = buf;
        for (char& c : out) c = (char)toupper((unsigned char)c);
        return out;
    }
    void SaveState(int slot = 0)
    {
        struct VFile* vf = VFileOpen(StatePath(slot).c_str(), O_CREAT | O_TRUNC | O_RDWR);
        const bool ok = vf && mCoreSaveStateNamed(core, vf, SAVESTATE_RTC);
        if (vf) vf->close(vf);
#ifdef GBAW_EMBEDDED
        // (Its picture for the slot's card: the game as it was on the screen when the menu opened.)
        if (ok && !glassShot.empty() && glassShotW > 0) GbaGlass::SaveSlotPicture(StatePath(slot), glassShot.data(), glassShotW, glassShotH);
#endif
        Toast(ok ? "STATE SAVED" : "COULDN'T SAVE THE STATE");
    }
    void LoadState(int slot = 0)
    {
        struct VFile* vf = VFileOpen(StatePath(slot).c_str(), O_RDONLY);
        if (!vf)
        {
            Toast("NO SAVED STATE FOR THIS GAME YET");
            return;
        }
        const bool ok = mCoreLoadStateNamed(core, vf, SAVESTATE_RTC);
        vf->close(vf);
        if (ok && ZoomOn()) { camera.Reset(); pk_wide_jump(core, frame240.data()); zoomShown = false; }
#ifdef GBAW_CHEATS
        if (ok) cheats.Reset();
#endif
        Toast(ok ? "STATE LOADED" : "COULDN'T LOAD THE STATE");
#ifdef GBAW_EMBEDDED
        if (ok && raReady && ra->reset) ra->reset(ra->ud);
#endif
    }

    // How many columns the picture gets on each side: the window's shape, or WIDESCREEN.
    int WantedExtra() const
    {
        const std::string w = set.Get("widescreen");
        double aspect;
        if (w == "off" || w == "3:2" || w == "4:3") return 0;
        if (w == "auto")
        {
            int ow = 16, oh = 9;
            if (ren) SDL_GetRendererOutputSize(ren, &ow, &oh);
            aspect = oh > 0 ? (double)ow / oh : 16.0 / 9;
        }
        else
        {
            const size_t c = w.find(':');
            aspect = c == std::string::npos ? 16.0 / 9 : atof(w.c_str()) / atof(w.c_str() + c + 1);
        }
        const int e = (int)lround((GbaH * aspect - GbaW) / 2);
        return std::max(0, std::min(MaxExtra, e));
    }

    // Keys held for the game.
    uint32_t GameKeys() const
    {
        uint32_t k = 0;
        auto on = [&](GbaKey g) { k |= 1u << g; };
        if (key[SDL_SCANCODE_X]) on(K_A);
        if (key[SDL_SCANCODE_Z]) on(K_B);
        if (key[SDL_SCANCODE_BACKSPACE] || key[SDL_SCANCODE_RSHIFT]) on(K_SELECT);
        if (key[SDL_SCANCODE_RETURN] || key[SDL_SCANCODE_KP_ENTER]) on(K_START);
        if (key[SDL_SCANCODE_RIGHT]) on(K_RIGHT);
        if (key[SDL_SCANCODE_LEFT]) on(K_LEFT);
        if (key[SDL_SCANCODE_UP]) on(K_UP);
        if (key[SDL_SCANCODE_DOWN]) on(K_DOWN);
        if (key[SDL_SCANCODE_W] || key[SDL_SCANCODE_S]) on(K_R);
        if (key[SDL_SCANCODE_Q] || key[SDL_SCANCODE_A]) on(K_L);
        const bool byLabel = ByLabel();
        if (pads.Button(byLabel ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B)) on(K_A);
        if (pads.Button(byLabel ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A)) on(K_B);
        {
            // START BUTTON: PAUSE MENU (start) or GBA START. The other is START and SELECT together.
            const bool st = pads.Button(SDL_CONTROLLER_BUTTON_START), sel = pads.Button(SDL_CONTROLLER_BUTTON_BACK);
            const Uint32 now = SDL_GetTicks();
            const bool stAlone = (st && !sel && !comboOn && now - plusAt >= 150) || (int32_t)(plusPulse - now) > 0;
            const bool selAlone = (sel && !st && !comboOn && now - minusAt >= 150) || (int32_t)(minusPulse - now) > 0;
            if (set.Get("gba_start") == "start")
            {
                if (stAlone) on(K_START);
                if (selAlone) on(K_SELECT);
            }
            else
            {
                if (st && sel) on(K_START);
                else if (selAlone) on(K_SELECT);
            }
        }
        if (pads.Button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) on(K_L);
        if (pads.Button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) on(K_R);
        k |= (touchHeld | touchTap) & 0x3FF;
        const int d = pads.Dirs();
        if (d & 1) on(K_UP);
        if (d & 2) on(K_DOWN);
        if (d & 4) on(K_LEFT);
        if (d & 8) on(K_RIGHT);
        return k;
    }
    bool FastDown() const
    {
        return key[SDL_SCANCODE_LSHIFT] || pads.Axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000;
    }
    bool FastForward() const
    {
        return fastButton.Requested(set.Get("fast_button") == "toggle", FastDown(), fastToggle);
    }

    // Runs one GBA frame, with sound if `sound`.
    // ZOOM and the look-around, after a frame: the camera's window for it, drawn by WIDE where it goes past the
    // picture the widescreen patch draws (whose own pixels are laid over the middle); a frame not shown only notes
    // the game's memory for the next.
    double zoomCost = 0, zoomCostMax = 0, zoomDrawCost = 0;   // ms in pk_wide_view and DrawZoom (test log)
    void ZoomFrame(bool shown)
    {
        zoomShown = false;
        // (FREE FLIGHT: drawn through WIDE's view at any zoom, 1x too, as PokeDaisy's EXPAND does: its smooth camera moves
        // the map evenly, where the game steps it a tile at a time.)
        const bool flying = FlightView();
        if (!ZoomOn() && !flying) return;
        const int e = std::max(0, wideExtra);
        for (int y = 0; y < GbaH; y++)
            memcpy(frame240.data() + (size_t)y * GbaW, video.data() + (size_t)y * MaxW + e, GbaW * 4);
        int ow = 0, oh = 0;
        if (ren) SDL_GetRendererOutputSize(ren, &ow, &oh);
        camera.viewW = ow;
        camera.viewH = oh;
        camera.zoomOnText = set.Get("zoom_text") == "1";
        const int dirs = pads.Dirs();
        camera.walking = !paused && ((GameKeys() & 0xF0) != 0 || dirs);
        float sx = pads.Axis(SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f, sy = pads.Axis(SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;
        if (key[SDL_SCANCODE_J]) sx = -1;
        if (key[SDL_SCANCODE_L]) sx = 1;
        if (key[SDL_SCANCODE_I]) sy = -1;
        if (key[SDL_SCANCODE_K]) sy = 1;
        camera.stickX = paused ? 0 : sx;
        camera.stickY = paused ? 0 : sy;
        const PkWideCamera::Window w = camera.Frame(pk_wide_scene(), (int64_t)SDL_GetPerformanceCounter() * 1000000000LL / (int64_t)SDL_GetPerformanceFrequency());
        // (The plain 1x view on the player: the widescreen picture as it is, WIDE only noting the memory.)
        const bool inside = w.valid && w.x >= -e && w.y >= 0 && w.x + w.w <= GbaW + e && w.y + w.h <= GbaH;
        // (Tests: GBAW_ZOOM_LOG=file, the camera every 30 frames.)
        static FILE* zlog = getenv("GBAW_ZOOM_LOG") ? fopen(getenv("GBAW_ZOOM_LOG"), "w") : nullptr;
        static int zframes = 0;
        if (zlog && ++zframes % 30 == 0)
        {
            fprintf(zlog, "frame %d scene %d score %d level %d view %dx%d win %d,%d %dx%d scale %.3f plain %d inside %d extra %d shown %d"
                    " dispcnt %04x bgcnt %04x %04x %04x %04x take %d ms %.2f max %.2f draw %.2f\n",
                    zframes, pk_wide_scene(), pk_wide_last_score(), camera.level, ow, oh, w.x, w.y, w.w, w.h, w.scale, (int)camera.Plain(), (int)inside, e, (int)shown,
                    core->busRead16(core, 0x04000000), core->busRead16(core, 0x04000008), core->busRead16(core, 0x0400000A),
                    core->busRead16(core, 0x0400000C), core->busRead16(core, 0x0400000E), pk_wide_take_fail, zoomCost, zoomCostMax, zoomDrawCost);
            zoomCostMax = 0;
            fflush(zlog);
        }
        // (WIDE's scene, which the camera goes by, comes only from a real view: at 1x with the stick still the skip will
        // do, as nothing past the picture is wanted; otherwise a view every frame shown, of one pixel when the window
        // is inside the picture.)
        const bool idle = camera.level == 0 && std::fabs(camera.stickX) < 0.2f && std::fabs(camera.stickY) < 0.2f;
        // (DAY AND NIGHT goes by the scene too: a one-pixel view then.)
        if (!shown || !w.valid || (idle && camera.Plain() && !SceneWanted() && !flying))
        {
            pk_wide_skip(core, frame240.data());
            return;
        }
        if ((camera.Plain() || inside) && !flying)
        {
            uint32_t px = 0;
            struct PkWideView one = { 0, 0, 1, 1 };
            pk_wide_view(core, frame240.data(), &px, one, nullptr);
            return;
        }
        struct PkWideView v = { w.x, w.y, w.w, w.h };
        const uint64_t zt0 = SDL_GetPerformanceCounter();
        pk_wide_view(core, frame240.data(), zoomBuf.data(), v, nullptr);
        zoomCost = (double)(SDL_GetPerformanceCounter() - zt0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
        zoomCostMax = std::max(zoomCostMax, zoomCost);
        if (flying)
        {
            pk_flight_draw_wide(zoomBuf.data(), v.w, v.h, v.x, v.y);   // (its clouds over the whole picture)
            zoomWin = w;
            zoomShown = true;
            return;
        }
        // The widescreen patch's own picture where the window covers it: the game's real pixels, its sides included,
        // all but the outermost 8 columns each side (a hack's patch can leave a strip of repeated tiles there,
        // Monomyth's left edge; WIDE's rebuild of the map is right there).
        const int trim = e > 0 ? std::min(8, e) : 0;
        const int x0 = std::max(w.x, -e + trim), x1 = std::min(w.x + w.w, GbaW + e - trim), y0 = std::max(w.y, 0), y1 = std::min(w.y + w.h, GbaH);
        if (x1 > x0)
            for (int y = y0; y < y1; y++)
                memcpy(zoomBuf.data() + (size_t)(y - w.y) * w.w + (x0 - w.x), video.data() + (size_t)y * MaxW + x0 + e, (size_t)(x1 - x0) * 4);
        zoomWin = w;
        zoomShown = true;
    }

    bool FlightView() const { return flightRec && pkWide && pk_flight_state() != PK_FLIGHT_IDLE; }
    // ZOOM AND LOOK AROUND on, for a game WIDE knows.
    bool ZoomOn() const { return pkWide && set.Get("zoom") == "1"; }

    // ZOOM: the next step out (back to 1x after the last), said in a toast.
    void ZoomCycle()
    {
        if (!ZoomOn())
        {
            Toast(!pkWide ? "ZOOM ISN'T AVAILABLE FOR THIS GAME YET" : "ZOOM AND LOOK AROUND IS OFF (DISPLAY)");
            return;
        }
        const int l = camera.Cycle();
        char t[48];
        snprintf(t, sizeof t, "ZOOM %.1fX", camera.Factor(l));
        Toast(l == 0 ? "ZOOM 1X" : t, 1200);
    }

    void RunFrame(bool sound, bool ff = false, bool shown = true)
    {
        const int e = WantedExtra();
        if (e != wideExtra)
        {
            wideExtra = e;
            mCoreConfigSetIntValue(&core->config, "gba.wideExtra", wideExtra);
            core->reloadConfigOption(core, "gba.wideExtra", nullptr);
        }
        uint32_t keys = paused ? 0 : GameKeys();
#ifdef GBAW_EMBEDDED
        if (!paused) keys = NavTick(keys); // (FULLDEX NAV open: the keys are the watch's)
#endif
        if (holdKeys)
        {
            if (keys == 0) holdKeys = false;
            keys = 0;
        }
        core->setKeys(core, keys);
        if (gpField && !paused) gpState = pk_field_state(core, set.Get("run_indoors") == "1", WildKeep());
        if (!paused) ShinyFrame();
        core->runFrame(core);
        fly.FrameEnded();
        if (!paused) { FlyFrame(); TmFrame(); }
        FogUndo();
        FlightFrame();
        ExploredFrame();
        FlyTest();
#ifdef GBAW_CHEATS
        cheats.Frame(cheatBus, keys);
#endif
#ifdef GBAW_EMBEDDED
        if (raReady && ra->frame) ra->frame(ra->ud);
#endif
        if (touchTapFrames > 0 && --touchTapFrames == 0) touchTap = 0;
        ZoomFrame(shown);
        if (ffMusic.Supported())
        {
            const uint32_t k = GbaFfMusic::CurrentKey(core);
            if (k != ffKey) { ffKey = k; ffKeyAt = SDL_GetTicks(); }
            if (k && set.Get("ff_mute") == "0") ffMusic.Request(k);
        }
        Sound(sound, ff);
        fpsFrames++;
        gameFrames++;
    }

    // -----------------------------------------------------------------------------------------
    // Menus

    void Rescan()
    {
        games = Games::Scan(roms, saves, listCache);
        listSel = std::max(0, std::min(listSel, (int)games.size() - 1));
    }

    // The pause menu's rows for the page that is open (the root, or RetroAchievements' pages).
    void BuildPause()
    {
        pause.items.clear();
#ifdef GBAW_EMBEDDED
        auto line = [&](bool (*fn)(void*, char*, size_t)) {
            char buf[96] = {};
            if (ra && fn) fn(ra->ud, buf, sizeof buf);
            return std::string(buf[0] ? buf : "-");
        };
        if (pausePage == P_LOGIN)
        {
            std::string u = raUser.size() > 18 ? raUser.substr(0, 15) + "..." : raUser;
            if (u.empty()) u = "...";
            const bool busy = ra && ra->busy && ra->busy(ra->ud);
            pause.items.push_back({ "USER NAME", u + (raField == 0 ? "_" : ""), true });
            pause.items.push_back({ "PASSWORD", std::to_string(raPass.size()) + " CHARACTERS" + (raField == 1 ? "_" : ""), true });
            pause.items.push_back({ busy ? "LOGGING IN..." : "LOG IN", "" });
            pause.items.push_back({ "CANCEL", "" });
            return;
        }
        if (pausePage == P_DEX)
        {
            // (Every Pokemon the ROM has, in National Pokedex order.)
            dexRows.clear();
            const int last = dex.count;
            std::vector<int> byDex(last + 2, 0);
            for (int sp = 1; sp <= last; sp++) { const int n = dex.Dex(sp); if (n > 0 && n < (int)byDex.size() && !byDex[n]) byDex[n] = sp; }
            for (int n = 1; n < (int)byDex.size(); n++)
            {
                if (!byDex[n]) continue;
                char label[64];
                snprintf(label, sizeof label, "%03d  %s", n, DexTitle(dex.Name(byDex[n])).c_str());
                const bool caught = n < (int)comp.caught.size() && comp.caught[n], seen = n < (int)comp.seen.size() && comp.seen[n];
                pause.items.push_back({ label, caught ? "CAUGHT" : seen ? "SEEN" : "", true });
                dexRows.push_back(byDex[n]);
            }
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_COMP)
        {
            for (const char* l : { "PARTY", "BAG", "LOCATION", "BATTLE INFO", "GUIDE" }) pause.items.push_back({ l, "" });
            if (boxes.Supported()) pause.items.push_back({ "BOXES", "" });
            if (fly.Supported()) pause.items.push_back({ "FLY", "" });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_FLY)
        {
            if (flightRec) { pause.items.push_back({ "FREE FLIGHT", "", false }); pause.items.push_back({ "FLY ON", "", false }); }
            for (int sec : flyT.towns) pause.items.push_back({ "TOWN " + std::to_string(sec), "", false });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_BOXES)
        {
            boxList = boxes.Read();
            for (int b = 0; b < (int)boxList.size(); b++)
            {
                if (boxList[b].mons.empty()) continue;
                pause.items.push_back({ "#BOX " + std::to_string(b), "", true });
                for (const auto& sl : boxList[b].mons) pause.items.push_back({ "B " + std::to_string(b) + " " + std::to_string(sl.slot), "", false });
            }
            if (pause.items.empty()) pause.items.push_back({ "Your boxes are empty", "", true });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_BOXMON)
        {
            if ((int)comp.party.size() < 6) pause.items.push_back({ "WITHDRAW", "", false });
            for (size_t i = 0; i < comp.party.size(); i++) pause.items.push_back({ "SWAP " + std::to_string(i), "", false });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_PARTYMON)
        {
            if (items.Supported() && partyPick >= 0 && partyPick < (int)comp.party.size() && comp.party[partyPick].item && !GbaItems::IsMail(comp.party[partyPick].item))
                pause.items.push_back({ "TAKE ITEM", "", false });
            if (boxes.Supported()) pause.items.push_back({ "DEPOSIT", "", false });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_ITEM)
        {
            if (items.CanTeach(itemPick)) pause.items.push_back({ "TEACH", "", false });
            if (items.CanUse(itemPick)) pause.items.push_back({ "USE", "", false });
            if (items.CanGive(itemPick)) pause.items.push_back({ "GIVE", "", false });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_ITEMMON)
        {
            for (size_t i = 0; i < itemTargets.size(); i++) pause.items.push_back({ "T " + std::to_string(i), "", false });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_ITEMMOVE)
        {
            for (int m = 0; m < 4; m++) pause.items.push_back({ "M " + std::to_string(m), "", false });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_FLYMON)
        {
            for (int n : FlyMonList()) pause.items.push_back({ "MON " + std::to_string(n), "", false });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_PARTY)
        {
            for (size_t i = 0; i < comp.party.size(); i++) pause.items.push_back({ "MON " + std::to_string(i), "", true });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_GUIDE)
        {
            const auto& g = GbaGuides::For(gameCode.c_str());
            for (size_t i = 0; i < g.size(); i++) pause.items.push_back({ g[i].title, i < guideShown.size() && guideShown[i] ? "SHOWN" : "", false });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_BAG || pausePage == P_PLACE || pausePage == P_BATTLE)
        {
            for (const auto& r : pausePage == P_BAG ? BagRows() : pausePage == P_PLACE ? PlaceRows() : BattleRows()) pause.items.push_back({ r.first, r.second, true });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_DEXENTRY)
        {
            for (const auto& r : DexEntryRows()) pause.items.push_back({ r.first, r.second, true });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_LIST)
        {
            achDesc.clear();
            const int n = ra && ra->count ? ra->count(ra->ud) : 0;
            if (n <= 0)
            {
                pause.items.push_back({ "NONE YET", "", true });
                return;
            }
            for (int i = 0; i < n; i++)
            {
                char title[80] = {}, desc[160] = {}, prog[32] = {};
                unsigned points = 0;
                int unlocked = 0;
                if (ra->at) ra->at(ra->ud, i, title, sizeof title, desc, sizeof desc, prog, sizeof prog, &points, &unlocked);
                std::string name = title[0] ? title : "ACHIEVEMENT";
                if (name.size() > 28) name = name.substr(0, 25) + "...";
                const std::string value = unlocked ? "DONE" : prog[0] ? std::string(prog) : std::to_string(points);
                pause.items.push_back({ name, value, true });
                std::string d = desc;
                if (d.size() > 52) d = d.substr(0, 49) + "...";
                achDesc.push_back(d);
            }
            return;
        }
#ifdef GBAW_CHEATS
        if (pausePage == P_CHEATS)
        {
            for (int i = 0; i < GbaCheats::COUNT; i++)
            {
                if (cheats.Has(i)) pause.items.push_back({ GbaCheats::About(i).name, cheats.On(i) ? "ON" : "OFF" });
                else pause.items.push_back({ GbaCheats::About(i).name, "-", true });
            }
            pause.items.push_back({ "BACK", "" });
            return;
        }
#endif
        if (IsSettingsPage())
        {
            static const char* const display[] = { "widescreen", "fullscreen", "scaling", "hd_filter", "lcd", "colour", "zoom", "zoom_text", "day_night", "follower", "nav_style", "show_fps" };
            static const char* const sound[] = { "volume", "ff_mute" };
            static const char* const controls[] = { "gba_start", "buttons", "fast_lock", "ff_speed", "fast_button" };
            static const char* const gameplay[] = { "instant_text", "reusable_tms", "run_indoors", "wild_encounters", "shiny_odds", "battle_speed", "text_speed", "bike_speed", "surf_speed", "fly_on", "flight_clouds" };
            const char* const* keys = pausePage == P_DISPLAY ? display : pausePage == P_SOUND ? sound : pausePage == P_GAMEPLAY ? gameplay : controls;
            const int count = pausePage == P_DISPLAY ? 11 : pausePage == P_SOUND ? 2 : pausePage == P_GAMEPLAY ? 11 : 5;
            pageSettings.clear();
            for (int i = 0; i < count; i++)
                for (const Setting& s : SettingsList())
                    if (!strcmp(s.key, keys[i]) && GameplayOffered(s.key))
                    {
                        pause.items.push_back({ s.name, set.Shown(s) });
                        pageSettings.push_back(&s);
                    }
#ifdef GBAW_PHONE
            if (pausePage == P_CONTROLS) pause.items.push_back({ "TOUCH CONTROLS", "EDIT LAYOUT" });
#endif
            if (pausePage == P_GAMEPLAY && pageSettings.empty()) pause.items.push_back({ "NOT FOR THIS GAME", "", true });
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_RA)
        {
            const bool on = ra && ra->on && ra->on(ra->ud);
            const bool in = ra && ra->loggedIn && ra->loggedIn(ra->ud);
            pause.items.push_back({ "RETROACHIEVEMENTS", on ? "ON" : "OFF" });
            pause.items.push_back({ "ACCOUNT", line(ra ? ra->account : nullptr), true });
            pause.items.push_back({ "THIS GAME", on ? line(ra ? ra->game : nullptr) : "-", true });
            if (on && ra->active && ra->active(ra->ud)) pause.items.push_back({ "ACHIEVEMENTS", "" });
            pause.items.push_back({ in ? "LOG OUT" : "LOG IN", "" });
            pause.items.push_back({ "BACK", "" });
            return;
        }
#endif
#ifdef GBAW_EMBEDDED
        if (pausePage == P_SAVE || pausePage == P_LOAD)
        {
            // (A slot with nothing in it cannot be loaded.)
            for (int i = 0; i < StateSlots; i++)
            {
                pause.items.push_back({ "SLOT " + std::to_string(i + 1), GbaGlass::SlotWhen(StatePath(i)), true });
                GbaGlass::LoadSlotPicture(StatePath(i), slotThumb[i], slotThumbW[i], slotThumbH[i]);
            }
            pause.items.push_back({ "BACK", "" });
            return;
        }
        if (pausePage == P_SETTINGS)
        {
            for (const char* l : { "DISPLAY", "SOUND", "CONTROLS", "GAMEPLAY", "RESET GAME", "BACK" }) pause.items.push_back({ l, "" });
            return;
        }
        if (pausePage == P_QUIT || pausePage == P_RESET)
        {
            // (Asked first: either loses what was played since the last save in the game.)
            pause.items.push_back({ pausePage == P_QUIT ? "QUIT GAME" : "RESET GAME", "" });
            pause.items.push_back({ "CANCEL", "" });
            return;
        }
        // (The first page as the DS games' pause menu has it: two rows of tiles. DISPLAY, SOUND, CONTROLS
        // and RESET GAME are in SETTINGS.)
        {
            pause.items.push_back({ "RESUME", "" });
            pause.items.push_back({ "SAVE STATE", "" });
            pause.items.push_back({ "LOAD STATE", "" });
            // (As the DS games' tiles: FAST FORWARD after LOAD, POKEDEX and COMPANION first in the
            // second row. FAST FORWARD's on and off is in CONTROLS too.)
            pause.items.push_back({ "FAST FORWARD", fastToggle ? "ON" : "OFF" });
#ifdef GBAW_CHEATS
            pause.items.push_back({ "CHEATS", cheats.Active() && cheats.OnCount() ? std::to_string(cheats.OnCount()) + " ON" : "" });
#else
            pause.items.push_back({ "CHEATS", "" });
#endif
            pause.items.push_back({ "POKEDEX", "" });
            pause.items.push_back({ "COMPANION", "" });
            pause.items.push_back({ "RETROACHIEVEMENTS", raReady && ra && ra->on && ra->on(ra->ud) ? "ON" : "" });
            pause.items.push_back({ "SETTINGS", "" });
            pause.items.push_back({ "QUIT", "" });
            return;
        }
#endif
        pause.items.push_back({ "RESUME", "" });
        pause.items.push_back({ "SAVE STATE", "" });
        pause.items.push_back({ "LOAD STATE", "" });
        pause.items.push_back({ "FAST FORWARD", fastToggle ? "ON" : "OFF" });
#ifdef GBAW_EMBEDDED
        // (As the DS games' pause menu: its settings by group, then CHEATS.)
        pause.items.push_back({ "DISPLAY", "" });
        pause.items.push_back({ "SOUND", "" });
        pause.items.push_back({ "CONTROLS", "" });
        pause.items.push_back({ "GAMEPLAY", "" });
#else
        pause.items.push_back({ "SETTINGS", "" });
#endif
#ifdef GBAW_CHEATS
        if (cheats.Active()) pause.items.push_back({ "CHEATS", cheats.OnCount() ? std::to_string(cheats.OnCount()) + " ON" : "" });
#endif
#if defined(GBAW_PHONE) && !defined(GBAW_EMBEDDED)
        pause.items.push_back({ "TOUCH CONTROLS", "" });
#endif
#ifdef GBAW_EMBEDDED
        if (raReady) pause.items.push_back({ "RETROACHIEVEMENTS", "" });
#endif
        pause.items.push_back({ "RESET GAME", "" });
#ifdef GBAW_EMBEDDED
        pause.items.push_back({ "QUIT", "" });
#else
        pause.items.push_back({ "QUIT GAME", "" });
#endif
    }

    void OpenPause()
    {
        paused = true;
#ifdef GBAW_EMBEDDED
        pausePage = P_ROOT;
#endif
        pause.selected = 0;
#ifdef GBAW_EMBEDDED
        glassSel = -1;
        glassShotW = glassShotH = 0;
        glassSlider = glassPick = -1;
#endif
        BuildPause();
        if (audio)
        {
            SDL_PauseAudioDevice(audio, 1);
            SDL_ClearQueuedAudio(audio);
        }
    }
    void Resume()
    {
#ifdef GBAW_EMBEDDED
        if (typing) SDL_StopTextInput();
        typing = false;
#endif
        paused = false;
        holdKeys = true;
        if (audio) SDL_PauseAudioDevice(audio, 0);
    }

#ifdef GBAW_EMBEDDED
    void PopChar(std::string& s)
    {
        if (s.empty()) return;
        s.pop_back();
        while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
    }
    void ForgetPass()
    {
        std::fill(raPass.begin(), raPass.end(), '\0');
        raPass.clear();
    }
    void LoginInput(const MenuIn& in)
    {
        const int n = (int)pause.items.size();
        if (n <= 0) return;
        if (in.erase)
        {
            PopChar(raField == 0 ? raUser : raPass);
            return;
        }
        if (in.menu)
        {
            ForgetPass();
            Resume();
            return;
        }
        if (in.back)
        {
            ForgetPass();
            if (typing) SDL_StopTextInput();
            typing = false;
            pausePage = P_RA;
            pause.selected = 0;
            return;
        }
        if (in.up) pause.selected = (pause.selected + n - 1) % n;
        if (in.down) pause.selected = (pause.selected + 1) % n;
        if (pause.selected <= 1) raField = pause.selected;
        if (!in.accept) return;
        if (pause.selected <= 1) raField = pause.selected;
        else if (pause.selected == 3)
        {
            ForgetPass();
            if (typing) SDL_StopTextInput();
            typing = false;
            pausePage = P_RA;
            pause.selected = 0;
        }
        else if (ra && ra->login)
        {
            if (ra->busy && ra->busy(ra->ud)) return;
            if (raUser.empty())
            {
                raField = 0;
                pause.selected = 0;
                Toast("TYPE YOUR USER NAME");
                return;
            }
            if (raPass.empty())
            {
                raField = 1;
                pause.selected = 1;
                Toast("TYPE YOUR PASSWORD");
                return;
            }
            ra->login(ra->ud, raUser.c_str(), raPass.c_str());
            ForgetPass();
            if (typing) SDL_StopTextInput();
            typing = false;
            pausePage = P_RA;
            pause.selected = 0;
        }
    }
#endif

#ifdef GBAW_CHEATS
    // The achievements host is told whether any cheat is on (achievements pause while one is).
    void CheatsToHost()
    {
        if (ra && ra->cheats) ra->cheats(ra->ud, cheats.OnCount() > 0 ? 1 : 0);
    }
    // CHEATS: a row a cheat (A, or left and right, turns it on or off: kept at once), then BACK.
    void CheatsInput(const MenuIn& in)
    {
        const int n = (int)pause.items.size();
        if (n <= 0) return;
        if (in.up) pause.selected = (pause.selected + n - 1) % n;
        if (in.down) pause.selected = (pause.selected + 1) % n;
        if (in.menu)
        {
            Resume();
            return;
        }
        if (in.back || (in.accept && pause.selected == GbaCheats::COUNT))
        {
            GoPage(P_ROOT, "CHEATS");
            return;
        }
        if (!in.accept && !in.left && !in.right) return;
        const int i = pause.selected;
        if (i < 0 || i >= GbaCheats::COUNT || !cheats.Has(i)) return;
        cheats.Set(i, !cheats.On(i));
        if (!cheats.Save(cheatsFile)) Toast("COULDN'T SAVE THE CHEATS");
        CheatsToHost();
        fprintf(stderr, "[gba] cheat %s %s\n", GbaCheats::About(i).name, cheats.On(i) ? "on" : "off");
    }
#endif

#ifdef GBAW_EMBEDDED
    // DISPLAY, SOUND and CONTROLS: a row a setting (left and right, or A, change it: kept at once,
    // in Fulldex's settings too), then BACK to the pause menu, on the row that opened the page.
    bool IsSettingsPage() const { return pausePage == P_DISPLAY || pausePage == P_SOUND || pausePage == P_CONTROLS || pausePage == P_GAMEPLAY; }
    const char* SettingsPageName() const
    {
        return pausePage == P_DISPLAY ? "DISPLAY" : pausePage == P_SOUND ? "SOUND" : pausePage == P_GAMEPLAY ? "GAMEPLAY" : "CONTROLS";
    }
    // (A GAMEPLAY row only where this game's code has what it changes.)
    bool GameplayOffered(const char* key) const
    {
        if (!strcmp(key, "instant_text")) return !gpText.empty();
        if (!strcmp(key, "reusable_tms")) return !gpTms.empty();
        if (!strcmp(key, "run_indoors")) return gpField && gameCode.size() == 4 && gameCode[0] == 'B'; // (Emerald and FireRed's engines)
        if (!strcmp(key, "wild_encounters")) return gpField && gpWild;
        if (!strcmp(key, "shiny_odds")) return EnemyParty() != 0;
        if (!strcmp(key, "day_night")) return pkWide && gpField;
        if (!strcmp(key, "fly_on") || !strcmp(key, "flight_clouds")) return flightRec != nullptr;
        if (!strcmp(key, "follower")) return pkWide && gpField && compConfig;
        if (!strcmp(key, "battle_speed") || !strcmp(key, "text_speed") || !strcmp(key, "bike_speed") || !strcmp(key, "surf_speed")) return gpField;
        return true;
    }
    // gEnemyParty, from the companion's gPlayerParty (where each engine keeps it); 0 for a game it doesn't know.
    uint32_t EnemyParty() const
    {
        if (!compConfig || gameCode.size() < 3) return 0;
        const std::string f = gameCode.substr(0, 3);
        return f == "AXV" || f == "AXP" ? compConfig->party + 0x260 : f == "BPE" ? compConfig->party + 0x258 : f == "BPR" || f == "BPG" ? compConfig->party - 0x258 : 0;
    }
    void ShinyFrame()
    {
        const int odds = atoi(set.Get("shiny_odds").c_str());
        const uint32_t enemy = EnemyParty();
        if (!enemy || odds <= 0 || odds >= 8192) return;
        shiny.Tick(odds, enemy, compConfig->saveBlock2, compConfig->staticSaveBlocks, compConfig->gMain + compConfig->inBattleOff,
                   [&](uint32_t a, uint8_t* b, int n) { for (int i = 0; i < n; i++) b[i] = (uint8_t)core->busRead8(core, a + i); return true; },
                   [&](uint32_t a, const uint8_t* b, int n) { for (int i = 0; i < n; i++) core->busWrite8(core, a + i, b[i]); return true; });
    }
    // SCENE SPEEDS: the speed for what is on the screen (the field's state from pk_field_state), without fast forward.
    int SceneSpeed() const
    {
        if (!gpField || !(gpState & PK_FIELD_KNOWN)) return 1;
        const char* key = (gpState & PK_FIELD_BATTLE) ? "battle_speed" : (gpState & (PK_FIELD_TEXT | PK_FIELD_WINDOW)) ? "text_speed"
            : (gpState & PK_FIELD_BIKE) ? "bike_speed" : (gpState & PK_FIELD_SURF) ? "surf_speed" : nullptr;
        return key ? std::max(1, std::min(4, set.Int(key))) : 1;
    }
    // DAY AND NIGHT: the tint for the hour (red, green, blue of 256), between the day's keys.
    static void DayNightAt(float hour, int out[3])
    {
        static const struct { float h; int c[3]; } keys[] = { { 0, { 135, 150, 225 } }, { 4.5f, { 135, 150, 225 } }, { 6, { 245, 220, 220 } },
                                                              { 7.5f, { 256, 256, 256 } }, { 16.5f, { 256, 256, 256 } }, { 18, { 255, 200, 165 } },
                                                              { 19.5f, { 135, 150, 225 } }, { 24, { 135, 150, 225 } } };
        int i = 0;
        while (i < 6 && keys[i + 1].h <= hour) i++;
        const float t = (hour - keys[i].h) / (keys[i + 1].h - keys[i].h);
        for (int c = 0; c < 3; c++) out[c] = (int)(keys[i].c[c] + (keys[i + 1].c[c] - keys[i].c[c]) * std::max(0.0f, std::min(1.0f, t)));
    }
    bool DayNightOn() const { return set.Get("day_night") == "1" && pkWide && gpField; }
    bool FollowerOn() const { return set.Get("follower") == "1" && pkWide && gpField && compConfig; }
    // (WIDE's scene, for DAY AND NIGHT and the FOLLOWER: zoom notes it; without zoom a one-pixel view each frame shown.)
    bool SceneWanted() const { return DayNightOn() || FollowerOn() || FogWanted(); }
    void SceneFrame()
    {
        if (ZoomOn() || !SceneWanted() || zoomShown) return;
        const int e = std::max(0, wideExtra);
        for (int y = 0; y < GbaH; y++) memcpy(frame240.data() + (size_t)y * GbaW, video.data() + (size_t)y * MaxW + e, GbaW * 4);
        uint32_t px = 0;
        struct PkWideView one = { 0, 0, 1, 1 };
        pk_wide_view(core, frame240.data(), &px, one, nullptr);
    }
    // FOLLOWING POKEMON: HeartGold or SoulSilver looked for once (on a thread), the lead's sprite handed to WIDE when it
    // changes, and shown on the field (not surfing, not in a battle).
    // FLY from the companion: the game's own Fly, started once the menu has closed (fly_gba.h).
    GbaFly::Fly fly;
    GbaFly::Targets flyT;
    int flySec = -1, flyStage = 0, flyWait = 0, flyHgssWait = 0;
    bool flyTesting = false;
    void FlyFrame()
    {
        if (flyStage == 1)
        {
            // (The first frame the game is at rest: the checks again, controls locked, the fade to black.)
            const GbaFly::Result r = fly.Start(flySec, flyT);
            if (r == GbaFly::OK) { flyStage = 2; flyWait = 0; }
            else if (r != GbaFly::NOT_NOW || ++flyWait > 120) { Toast(GbaFly::Why(r), 3000); flyStage = 0; }
        }
        else if (flyStage == 10)
        {
            // FREE FLIGHT: the checks again, then the take-off on the next frames.
            // (YOUR FLY POKEMON: HeartGold / SoulSilver looked for first, a second or so at most.)
            if ((set.Get("fly_on") == "pokemon" || set.Get("fly_on") == "any" || LegendNational(set.Get("fly_on"))) && followState <= 1 && ++flyHgssWait < 300) { if (followState == 0) FollowFind(); return; }
            const GbaFly::Targets t = fly.Now();
            if (t.result == GbaFly::OK)
            {
                FlightMount(t.slot);
                ExploredSeed();
                const int r = pk_flight_take_off(core);
                if (r == PK_FLIGHT_OK || r != PK_FLIGHT_NOT_NOW || ++flyWait > 120)
                {
                    if (r != PK_FLIGHT_OK && flyTesting)
                        if (FILE* l = fopen("fly-log.txt", "a"))
                        {
                            char info[256] = {};
                            pk_flight_info(core, info, sizeof info);
                            fprintf(l, "take-off %d rec %s cb2 %08x want %08x lock %d | %s\n", r, flightRec ? flightRec->name : "-",
                                    flightRec ? core->busRead32(core, flightRec->words[PK_FLIGHT_MAIN] + 4) : 0, flightRec ? flightRec->words[PK_FLIGHT_CB2_OVERWORLD] : 0,
                                    flightRec ? core->busRead8(core, flightRec->words[PK_FLIGHT_LOCK_BYTE]) : -1, info);
                            fclose(l);
                        }
                    if (r != PK_FLIGHT_OK) Toast(r == PK_FLIGHT_NOT_ON_FOOT ? "GET OFF THE WATER FIRST" : "FREE FLIGHT CAN'T START HERE", 3000);
                    flyStage = 0;
                }
            }
            else if (t.result != GbaFly::NOT_NOW || ++flyWait > 120) { Toast(GbaFly::Why(t.result), 3000); flyStage = 0; }
        }
        else if (flyStage == 2 && ++flyWait >= 30)
        {
            const auto step = fly.Finish(flySec, flyT);
            if (FILE* l = fopen("fly-log.txt", "r")) { fclose(l); if (FILE* a = fopen("fly-log.txt", "a")) { fprintf(a, "finish frame %d step %d\n", (int)gameFrames, (int)step); fclose(a); } }
            if (step != GbaFly::Fly::LATER || flyWait > 120) flyStage = 0;
        }
    }
    // TEACH a TM or HM: the game's own TM party menu, opened once TmStart's fade is black (GbaItems::Items::TmFinish).
    int tmItem = 0, tmWait = 0;
    void TmFrame()
    {
        if (!tmItem || ++tmWait < 30) return;
        const auto step = items.TmFinish(tmItem);
        if (step == GbaItems::Items::TM_LATER && tmWait < 120) return;
        if (step == GbaItems::Items::TM_BACK || step == GbaItems::Items::TM_LATER) Toast("THE TM SCREEN CAN'T OPEN HERE", 2500);
        if (FILE* l = fopen("fly-log.txt", "r")) { fclose(l); if (FILE* a = fopen("fly-log.txt", "a")) { fprintf(a, "tm frame %d step %d\n", (int)gameFrames, (int)step); fclose(a); } }
        tmItem = 0;
    }
    // FREE FLIGHT (PokeDaisy's pk_flight.c): the game's own Fly bird, steered over the field; set up when the game's
    // functions are where its record says (flight_records.h).
    const GbaFlight::Record* flightRec = nullptr;
    // (The record's functions checked in the ROM as it came: the widescreen patch rewrites some (Ruby's
    // CurrentMapDrawMetatileAt draws the wider view), and those are still the game's own to call.)
    void FlightFind(const std::vector<uint8_t>& clean)
    {
        flightRec = nullptr;
        pk_flight_configure(nullptr, 0);
        pk_flight_reset();
        if (!fly.Supported()) return;
        for (const auto& r : GbaFlight::kRecords)
        {
            bool ok = true;
            for (int i = 0; i < r.nfns && ok; i++)
            {
                uint8_t b[32];
                const size_t o = (r.fns[i] & ~1u) - 0x08000000u;
                if (o + 32 > clean.size()) { ok = false; break; }
                for (int k = 0; k < 32; k++) b[k] = clean[o + k];
                ok = (uint32_t)doCrc32(b, 32) == r.crcs[i];
            }
            if (!ok) continue;
            flightRec = &r;
            pk_flight_set_call(pk_game_call);
            pk_flight_configure(r.words, PK_FLIGHT_WORDS);
            return;
        }
    }
    // CLOUDS OVER UNEXPLORED: a bit per map (group << 8 | num) the player has stood on, kept beside the save
    // (saves/<game>.explored); while flying, clouds over every map not in it.
    std::vector<uint8_t> explored = std::vector<uint8_t>(8192);
    std::string exploredPath;
    bool exploredFile = false, exploredDirty = false;
    uint32_t exploredSavedAt = 0;
    void ExploredLoad(const std::string& path)
    {
        exploredPath = path;
        std::fill(explored.begin(), explored.end(), 0);
        std::vector<uint8_t> b;
        exploredFile = Games::ReadFile(path, b, 8192) && b.size() == 8192;
        if (exploredFile) explored = b;
        exploredDirty = false;
    }
    void ExploredSave()
    {
        if (!exploredDirty || exploredPath.empty()) return;
        Games::WriteFile(exploredPath, explored);
        exploredFile = true;
        exploredDirty = false;
        exploredSavedAt = SDL_GetTicks();
    }
    bool Explored(int group, int num) const { const int id = (group & 0xFF) << 8 | (num & 0xFF); return (explored[id >> 3] >> (id & 7)) & 1; }
    void Explore(int group, int num)
    {
        const int id = (group & 0xFF) << 8 | (num & 0xFF);
        if ((explored[id >> 3] >> (id & 7)) & 1) return;
        explored[id >> 3] |= (uint8_t)(1 << (id & 7));
        exploredDirty = true;
    }
    // Each frame on the field: the map the player is on (SaveBlock1's location); written out every ten seconds or so.
    void ExploredFrame()
    {
        if (!compConfig || !gpField || paused) return;
        const int sc = pk_wide_scene();
        if (sc != PK_WIDE_SCENE_FIELD && sc != PK_WIDE_SCENE_TEXT) return;
        const uint32_t sb1 = compConfig->staticSaveBlocks ? compConfig->saveBlock1 : core->busRead32(core, compConfig->saveBlock1);
        if (sb1 < 0x02000000 || sb1 >= 0x02040000) return;
        Explore(core->busRead8(core, sb1 + 4), core->busRead8(core, sb1 + 5));
        if (exploredDirty && SDL_GetTicks() - exploredSavedAt > 10000) ExploredSave();
    }
    // A save from before the record: the towns the Fly map lists as visited, and the maps beside each, count as seen.
    void ExploredSeed()
    {
        if (exploredFile || !flightRec) return;
        const uint32_t groups = flightRec->words[PK_FLIGHT_MAP_GROUPS];
        auto header = [&](int g, int n) -> uint32_t {
            const uint32_t list = core->busRead32(core, groups + 4u * (uint32_t)g);
            if ((list >> 24) != 0x08) return 0;
            const uint32_t h = core->busRead32(core, list + 4u * (uint32_t)n);
            return (h >> 24) == 0x08 ? h : 0;
        };
        for (const auto& tm : fly.VisitedTownMaps())
        {
            Explore(tm.first, tm.second);
            const uint32_t h = header(tm.first, tm.second);
            const uint32_t conns = h ? core->busRead32(core, h + 0xC) : 0;
            if ((conns >> 24) != 0x08) continue;
            const int count = std::min(8, (int)core->busRead32(core, conns));
            const uint32_t arr = core->busRead32(core, conns + 4);
            for (int i = 0; i < count; i++)
            {
                const uint32_t c = arr + 12u * (uint32_t)i;
                const int dir = core->busRead8(core, c);
                if (dir >= 1 && dir <= 4) Explore(core->busRead8(core, c + 8), core->busRead8(core, c + 9));
            }
        }
        exploredDirty = true;
        ExploredSave();
    }
    // The clouds: drawn into the picture after each frame while flying (and taken out again before the next, where
    // the renderer left a line as it was), over every 8 px cell whose map isn't explored, soft at the edges.
    std::vector<uint32_t> fogBefore, fogAfter;
    bool fogDrawn = false;
    bool FogWanted() const { return flightRec && set.Get("flight_clouds") == "1" && pk_flight_state() != PK_FLIGHT_IDLE; }
    void FogUndo()
    {
        if (!fogDrawn) return;
        fogDrawn = false;
        for (size_t i = 0; i < fogAfter.size(); i++)
            if (video[i] == fogAfter[i]) video[i] = fogBefore[i];
    }
    static float FogNoise(int x, int y)
    {
        auto h = [](int a, int b) { uint32_t v = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u; v = (v ^ (v >> 13)) * 1274126177u; return (float)((v ^ (v >> 16)) & 1023) / 1023.0f; };
        auto vn = [&](int px, int py, int sc) {
            const int gx = (px >= 0 ? px : px - sc + 1) / sc, gy = (py >= 0 ? py : py - sc + 1) / sc;
            const float fx = (float)(px - gx * sc) / sc, fy = (float)(py - gy * sc) / sc;
            const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
            const float a = h(gx, gy), b = h(gx + 1, gy), c = h(gx, gy + 1), d = h(gx + 1, gy + 1);
            return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
        };
        return 0.55f * vn(x, y, 40) + 0.3f * vn(x + 1000, y + 500, 17) + 0.15f * vn(x - 300, y + 900, 7);
    }
    void FogFrame()
    {
        if (!FogWanted()) return;
        int camX = 0, camY = 0;
        if (!pk_wide_map_cam(&camX, &camY)) return;
        const int e = std::max(0, wideExtra);
        if (zoomShown && zoomWin.valid)
        {
            // (Through WIDE's view: its window, moved by the flight's smooth camera as the view was.)
            int fdx = 0, fdy = 0;
            pk_wide_flight_offset(&fdx, &fdy);
            FogInto(zoomBuf.data(), zoomWin.w, zoomWin.h, zoomWin.w, zoomWin.x + fdx, zoomWin.y + fdy, camX, camY, false);
        }
        else FogInto(video.data(), GbaW + 2 * e, GbaH, MaxW, -e, 0, camX, camY, true);
    }
    // The clouds into [buf] (w x h, [stride] a row), its (0, 0) at the frame's (ox, oy); [undo]: kept to be taken out again.
    void FogInto(uint32_t* buf, int W, int Hh, int stride, int ox, int oy, int camX, int camY, bool undo)
    {
        // The cells' marks: 1 = a map not yet explored, every 8 px from 40 px left of and above the picture (so the
        // blur below reaches in from past its edges).
        const int M = 40, cw = (W + 2 * M) / 8 + 1, ch = (Hh + 2 * M) / 8 + 1;
        std::vector<float> m((size_t)cw * ch, 0.0f), t((size_t)cw * ch);
        bool any = false;
        for (int j = 0; j < ch; j++)
            for (int i = 0; i < cw; i++)
            {
                int g, n;
                const int sx = ox - M + 4 + i * 8, sy = oy - M + 4 + j * 8;
                if (pk_wide_map_at(sx, sy, &g, &n) && !Explored(g, n)) { m[(size_t)j * cw + i] = 1.0f; any = true; }
            }
        if (!any) return;
        // (Blurred four times over 3 x 3 cells: the edge a soft slope about 32 px wide.)
        for (int pass = 0; pass < 4; pass++)
        {
            for (int j = 0; j < ch; j++)
                for (int i = 0; i < cw; i++)
                {
                    float sum = 0; int cnt = 0;
                    for (int dj = -1; dj <= 1; dj++)
                        for (int di = -1; di <= 1; di++)
                        {
                            const int jj = std::min(ch - 1, std::max(0, j + dj)), ii = std::min(cw - 1, std::max(0, i + di));
                            sum += m[(size_t)jj * cw + ii]; cnt++;
                        }
                    t[(size_t)j * cw + i] = sum / cnt;
                }
            m.swap(t);
        }
        if (undo) fogBefore.assign(video.begin(), video.end());
        auto smooth = [](float a, float b, float v) { v = std::min(1.0f, std::max(0.0f, (v - a) / (b - a))); return v * v * (3 - 2 * v); };
        for (int y = 0; y < Hh; y++)
            for (int x = 0; x < W; x++)
            {
                const float fx = (x + M - 4) / 8.0f, fy = (y + M - 4) / 8.0f;
                const int ix = std::min(cw - 2, (int)fx), iy = std::min(ch - 2, (int)fy);
                const float tx = fx - ix, ty = fy - iy;
                const float a = m[(size_t)iy * cw + ix], b = m[(size_t)iy * cw + ix + 1], c = m[(size_t)(iy + 1) * cw + ix], d = m[(size_t)(iy + 1) * cw + ix + 1];
                const float k = (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
                if (k <= 0.02f) continue;
                // (Billows: the slope pushed in and out by the noise, so the edge is a row of puffs; anchored to the map.)
                const int wx = camX + ox + x, wy = camY + oy + y;
                const float nz = FogNoise(wx, wy);
                const float alpha = 0.97f * smooth(0.28f, 0.52f, k + (nz - 0.5f) * 1.05f);
                if (alpha <= 0.01f) continue;
                // (Light from the top left: brighter where the noise rises that way, a cool grey in the hollows.)
                const float lit = std::min(1.04f, std::max(0.88f, 0.97f + (nz - FogNoise(wx + 3, wy + 4)) * 1.2f));
                const float cr = std::min(255.0f, 236 * lit), cg = std::min(255.0f, 241 * lit), cb = std::min(255.0f, 250 * lit);
                uint32_t& p = buf[(size_t)y * stride + x];
                const float r = (p & 0xFF) * (1 - alpha) + cr * alpha, gg = ((p >> 8) & 0xFF) * (1 - alpha) + cg * alpha, bb = ((p >> 16) & 0xFF) * (1 - alpha) + cb * alpha;
                p = (p & 0xFF000000u) | (uint32_t)bb << 16 | (uint32_t)gg << 8 | (uint32_t)r;
            }
        if (undo)
        {
            fogAfter.assign(video.begin(), video.end());
            fogDrawn = true;
        }
    }
    // FLY ON a legendary: Emerald's, FireRed's and LeafGreen's own object sprites (FireRed and LeafGreen: the three birds,
    // Navel Rock's Lugia and Ho-Oh, Birth Island's Deoxys); Emerald's (the ones its story shows: Southern Island's Latias and Latios,
    // Sky Pillar's Rayquaza, Navel Rock's Lugia and Ho-Oh, Birth Island's Deoxys), two of each one's frames taking turns.
    // Read from the ROM (gObjectEventGraphicsInfoPointers, and the sprite palettes found by their tags); retail Emerald only.
    bool LegendMount(const std::string& which)
    {
        std::vector<uint8_t> px;
        int w = 0, h = 0;
        uint32_t pal[16];
        if (!LegendFrames(which, px, w, h, pal)) return false;
        pk_flight_set_mount(px.data(), w, h, pal);
        return true;
    }
    // Two frames (palette indices, w x h each, one after the other) and the palette (ABGR) of an Emerald legendary.
    uint32_t objGfxTable = 0, objPalTable = 0, objPalCount = 0;   // (the ROM's object graphics and palette tables; 1: not found)
    bool LegendFrames(const std::string& which, std::vector<uint8_t>& px, int& outW, int& outH, uint32_t pal[16])
    {
        const bool emerald = gameCode.rfind("BPE", 0) == 0, kanto = gameCode.rfind("BPR", 0) == 0 || gameCode.rfind("BPG", 0) == 0;
        if (!emerald && !kanto) return false;
        const struct GBA* gba = (const struct GBA*)core->board;
        const uint8_t* rom = (const uint8_t*)gba->memory.rom;
        const size_t size = gba->memory.romSize;
        auto r32 = [&](uint32_t a) -> uint32_t { const size_t o = a - 0x08000000u; return o + 4 <= size ? rom[o] | rom[o + 1] << 8 | rom[o + 2] << 16 | (uint32_t)rom[o + 3] << 24 : 0; };
        auto r16 = [&](uint32_t a) -> uint32_t { const size_t o = a - 0x08000000u; return o + 2 <= size ? (uint32_t)(rom[o] | rom[o + 1] << 8) : 0; };
        auto isRom = [&](uint32_t v) { return v >= 0x08000000u && v - 0x08000000u < size; };
        struct L { const char* name; int gfx, f0, f1; };
        static const L hoenn[] = { { "latias", 187, 0, 4 }, { "latios", 188, 0, 4 }, { "rayquaza", 207, 5, 6 }, { "lugia", 237, 0, 4 }, { "hooh", 238, 0, 4 }, { "deoxys", 232, 0, 4 } };
        // (FireRed and LeafGreen: the Sevii and Seafoam / Power Plant / Mt. Ember birds have one picture each.)
        static const L kantoList[] = { { "zapdos", 136, 0, 0 }, { "moltres", 137, 0, 0 }, { "articuno", 138, 0, 0 }, { "lugia", 144, 0, 4 }, { "hooh", 145, 0, 4 }, { "deoxys", 148, 0, 4 } };
        const L* l = nullptr;
        for (const L& c : hoenn) if (emerald && which == c.name) l = &c;
        for (const L& c : kantoList) if (kanto && which == c.name) l = &c;
        if (!l) return false;
        // (gObjectEventGraphicsInfoPointers: the longest run of pointers to graphics infos, found once per ROM, so every
        // language and revision has its own.)
        auto infoOk = [&](uint32_t i) {
            if (!isRom(i) || r16(i) != 0xFFFF) return false;
            // (Any size in 8s, the ferry's 96 x 40 too; some hold two frames' worth of bytes.)
            const uint32_t w = r16(i + 8), h = r16(i + 0xA), b = r16(i + 6), one = w * h / 2;
            return w >= 8 && h >= 8 && w <= 128 && h <= 128 && !(w % 8) && !(h % 8) && b && !(b % one) && b / one <= 4;
        };
        if (!objGfxTable)
        {
            uint32_t bestN = 0;
            for (size_t o = 0; o + 4 <= size;)
            {
                uint32_t n = 0;
                while (o + 4 * (n + 1) <= size && infoOk(r32(0x08000000u + (uint32_t)(o + 4 * n)))) n++;
                if (n > bestN) { bestN = n; objGfxTable = 0x08000000u + (uint32_t)o; }
                o += 4 * (n ? n : 1);
            }
            if (bestN < 100) objGfxTable = 1;   // (none: don't look again)
        }
        if (objGfxTable < 0x08000000u) return false;
        const uint32_t info = r32(objGfxTable + 4u * (uint32_t)l->gfx);
        if (!isRom(info)) return false;
        const uint32_t tag = r16(info + 2), bytes = r16(info + 6), w = r16(info + 8), h = r16(info + 0xA), images = r32(info + 0x1C);
        if (w > 64 || h > 64 || bytes != w * h / 2 || !isRom(images)) return false;
        // (The palette: the table of {data, tag} pairs the object palettes are loaded from, the longest run of them, found
        // once; LeafGreen has other pairs with the same tags before it. Else the first pair with the tag.)
        if (!objPalTable)
        {
            uint32_t bestN = 0;
            auto pair = [&](size_t o) { const uint32_t a = 0x08000000u + (uint32_t)o; return o + 8 <= size && isRom(r32(a)) && r16(a + 4) >= 0x1100 && r16(a + 4) < 0x1200 && r16(a + 6) == 0; };
            for (size_t o = 0; o + 8 <= size;)
            {
                uint32_t n = 0;
                while (pair(o + 8 * n)) n++;
                if (n > bestN) { bestN = n; objPalTable = 0x08000000u + (uint32_t)o; objPalCount = n; }
                o += n ? 8 * n : 4;
            }
            if (bestN < 8) { objPalTable = 1; objPalCount = 0; }
        }
        uint32_t palData = 0;
        for (uint32_t k = 0; k < objPalCount && !palData; k++)
            if (r16(objPalTable + 8 * k + 4) == tag) palData = r32(objPalTable + 8 * k);
        for (size_t o = 0; o + 16 <= size && !palData; o += 4)
        {
            const uint32_t a = 0x08000000u + (uint32_t)o;
            if (r16(a + 4) == tag && isRom(r32(a)) && isRom(r32(a + 8)) && r16(a + 12) >= 0x1100 && r16(a + 12) < 0x1200) palData = r32(a);
        }
        if (!palData) return false;
        for (int i = 0; i < 16; i++)
        {
            const unsigned c = r16(palData + 2u * (uint32_t)i), rr = c & 31, gg = (c >> 5) & 31, bb = (c >> 10) & 31;
            pal[i] = 0xFF000000u | ((bb << 3 | bb >> 2) << 16) | ((gg << 3 | gg >> 2) << 8) | (rr << 3 | rr >> 2);
        }
        px.assign(2u * w * h, 0);
        const int frames[2] = { l->f0, l->f1 };
        for (int f = 0; f < 2; f++)
        {
            const uint32_t d = r32(images + 8u * (uint32_t)frames[f]);
            if (!isRom(d) || r16(images + 8u * (uint32_t)frames[f] + 4) != bytes) return false;
            for (uint32_t y = 0; y < h; y++)
                for (uint32_t x = 0; x < w; x++)
                {
                    const uint32_t t = (y / 8) * (w / 8) + x / 8;
                    const uint8_t b = rom[d - 0x08000000u + t * 32 + (y % 8) * 4 + (x % 8) / 2];
                    px[(size_t)f * w * h + y * w + x] = (x & 1) ? b >> 4 : b & 15;
                }
        }
        outW = (int)w;
        outH = (int)h;
        return true;
    }
    // BAG's USE, GIVE and TAKE (items_gba.h): the item picked, then the Pokemon (and move) it goes to.
    GbaItems::Items items;
    std::vector<int> bagRowIds;   // (each BAG row's item; 0: a heading)
    int itemPick = 0, itemTarget = -1;
    bool itemGive = false;
    std::vector<GbaItems::Target> itemTargets;
    std::string ItemName(int id) { const std::string n = dex.ItemName(id); return n.empty() ? "Item " + std::to_string(id) : DexTitle(n); }
    void ItemDone(GbaItems::Result r, const std::string& done)
    {
        Toast(r == GbaItems::USED || r == GbaItems::GIVEN || r == GbaItems::TAKEN ? done : std::string(GbaItems::Why(r)), 2500);
        ReadCompanion();
    }
    bool ItemsInput(const MenuIn& in)
    {
        const bool sel = pause.selected >= 0 && pause.selected < (int)pause.items.size();
        const std::string l = sel ? pause.items[pause.selected].label : std::string();
        if (pausePage == P_BAG)
        {
            const int id = sel && pause.selected < (int)bagRowIds.size() ? bagRowIds[pause.selected] : 0;
            if (!id || !(items.CanUse(id) || items.CanGive(id) || items.CanTeach(id)) || comp.inBattle) return false;
            itemPick = id;
            GoPage(P_ITEM, items.CanTeach(id) ? "TEACH" : items.CanUse(id) ? "USE" : "GIVE");
            return true;
        }
        if (pausePage == P_PARTYMON)   // TAKE ITEM
        {
            if (partyPick < 0 || partyPick >= (int)comp.party.size()) return true;
            int item = 0;
            const GbaItems::Result r = items.Take(items.Slot(partyPick, comp.party[partyPick].personality, comp.party[partyPick].item), item);
            ItemDone(r, ItemName(item) + " PUT IN THE BAG");
            GoPage(P_PARTY, "");
            if (partyPick < (int)pause.items.size()) pause.selected = partyPick;
            return true;
        }
        auto toBag = [&]() {
            GoPage(P_BAG, "");
            for (size_t i = 0; i < bagRowIds.size(); i++) if (bagRowIds[i] == itemPick) { pause.selected = (int)i; break; }
        };
        if (in.back || (in.accept && l == "BACK"))
        {
            if (pausePage == P_ITEMMOVE) GoPage(P_ITEMMON, "");
            else if (pausePage == P_ITEMMON) GoPage(P_ITEM, itemGive ? "GIVE" : "USE");
            else toBag();
            return true;
        }
        if (!in.accept || !sel) return true;
        if (pausePage == P_ITEM && l == "TEACH")
        {
            // (The game's own TM screen: the fade starts now, the party menu once it's black.)
            const GbaItems::Result r = items.TmStart(itemPick);
            if (r != GbaItems::USED) { Toast(GbaItems::Why(r), 2500); return true; }
            tmItem = itemPick;
            tmWait = 0;
            Resume();
            return true;
        }
        if (pausePage == P_ITEM)
        {
            itemGive = l == "GIVE";
            if (!itemGive && GbaItems::IsRepel(itemPick))
            {
                int steps = 0;
                const GbaItems::Result r = items.UseRepel(itemPick, steps);
                ItemDone(r, ItemName(itemPick) + ": " + std::to_string(steps) + " STEPS WITHOUT WILD POKEMON");
                toBag();
                return true;
            }
            GbaItems::Result why = GbaItems::USED;
            itemTargets = items.Targets(itemPick, itemGive, why);
            if (itemTargets.empty()) { Toast(GbaItems::Why(why == GbaItems::USED ? GbaItems::FAILED : why), 2500); return true; }
            GoPage(P_ITEMMON, "");
            return true;
        }
        if (pausePage == P_ITEMMON && l.rfind("T ", 0) == 0)
        {
            itemTarget = std::atoi(l.c_str() + 2);
            if (itemTarget < 0 || itemTarget >= (int)itemTargets.size()) return true;
            const GbaItems::Target& t = itemTargets[itemTarget];
            if (t.verdict != GbaItems::OK) { Toast(t.verdict == GbaItems::EGG ? "AN EGG CAN'T USE OR HOLD THAT" : t.verdict == GbaItems::HOLDS_MAIL ? "TAKE ITS MAIL FIRST" : t.verdict == GbaItems::HOLDS_IT ? "IT'S HOLDING ONE ALREADY" : "IT WON'T HAVE ANY EFFECT", 2500); return true; }
            if (!itemGive && items.PicksMove(itemPick)) { GoPage(P_ITEMMOVE, ""); return true; }
            if (itemGive)
            {
                int was = 0;
                const GbaItems::Result r = items.Give(itemPick, t, was);
                ItemDone(r, ItemName(itemPick) + " GIVEN" + (was ? ", " + ItemName(was) + " PUT IN THE BAG" : std::string()));
            }
            else ItemDone(items.UseOn(itemPick, t, 0), ItemName(itemPick) + " USED");
            toBag();
            return true;
        }
        if (pausePage == P_ITEMMOVE && l.rfind("M ", 0) == 0 && itemTarget >= 0 && itemTarget < (int)itemTargets.size())
        {
            const int m = std::atoi(l.c_str() + 2);
            if (!itemTargets[itemTarget].moves[m]) { Toast("IT WON'T HAVE ANY EFFECT", 2500); return true; }
            ItemDone(items.UseOn(itemPick, itemTargets[itemTarget], m), ItemName(itemPick) + " USED");
            toBag();
            return true;
        }
        return true;
    }
    template <class Page, class Put>
    void ItemsGlass(GbaGlass::Page& g, int n, Page page, Put put)
    {
        using namespace GbaGlass;
        const std::string name = ItemName(itemPick);
        page(pausePage == P_ITEM ? name.c_str() : itemGive ? "GIVE TO" : "USE ON", IC_LOAD);
        const int card = pausePage == P_ITEMMON ? (pause.selected >= 0 && pause.selected < (int)itemTargets.size() ? itemTargets[pause.selected].slot : -1)
                                                : pausePage == P_ITEMMOVE && itemTarget >= 0 && itemTarget < (int)itemTargets.size() ? itemTargets[itemTarget].slot : -1;
        if (card >= 0 && card < (int)comp.party.size()) g.mons.push_back(MonCardFor(card));
        for (int i = 0; i < n; i++)
        {
            const std::string& l = pause.items[i].label;
            if (l == "BACK") continue;
            Item it;
            it.kind = LINK;
            it.icon = IC_LOAD;
            if (l == "USE") { it.label = "Use"; it.desc = GbaItems::IsRepel(itemPick) ? "Wild Pok\xC3\xA9" "mon keep away for a while." : "Use it on a Pok\xC3\xA9" "mon in your party."; }
            else if (l == "GIVE") { it.label = "Give"; it.desc = "Give it to a Pok\xC3\xA9" "mon to hold. What it held goes back in the bag."; }
            else if (l.rfind("T ", 0) == 0)
            {
                const int k = std::atoi(l.c_str() + 2);
                if (k < 0 || k >= (int)itemTargets.size()) continue;
                const GbaItems::Target& t = itemTargets[k];
                it.label = t.slot < (int)comp.party.size() ? MonCardFor(t.slot).name : "Pok\xC3\xA9" "mon";
                it.value = t.verdict == GbaItems::OK ? (itemGive && t.held ? "HOLDS " + ItemName(t.held) : std::string()) :
                           t.verdict == GbaItems::EGG ? "EGG" : t.verdict == GbaItems::HOLDS_MAIL ? "HOLDS MAIL" : t.verdict == GbaItems::HOLDS_IT ? "HOLDS IT" : "NO EFFECT";
                it.disabled = t.verdict != GbaItems::OK;
                if (!g.mons.empty()) it.monCard = 0;
            }
            else if (l.rfind("M ", 0) == 0)
            {
                const int m = std::atoi(l.c_str() + 2);
                const GbaItems::Target* t = itemTarget >= 0 && itemTarget < (int)itemTargets.size() ? &itemTargets[itemTarget] : nullptr;
                const int slot = t ? t->slot : -1;
                const int mv = slot >= 0 && slot < (int)comp.party.size() ? comp.party[slot].moves[m] : 0;
                if (!mv) continue;
                it.label = DexTitle(dex.Move(mv));
                it.value = std::to_string(comp.party[slot].pp[m]) + " PP";
                it.disabled = !t->moves[m];
                if (!g.mons.empty()) it.monCard = 0;
            }
            else continue;
            put(it, i);
        }
    }
    // BOXES (boxes_gba.h): the PC read live; a box Pokemon's card with its level and stats worked out from its experience,
    // IVs, EVs and nature, as the game does when it comes out.
    GbaBoxes::Boxes boxes;
    std::vector<GbaBoxes::Box> boxList;
    int boxPick = -1, boxPickSlot = -1, partyPick = -1;
    std::map<int, GbaDex::Picture> boxPics;
    static int ExpFor(int n, int growth)
    {
        const long long n3 = (long long)n * n * n;
        switch (growth)
        {
        case 1: return (int)(n <= 50 ? n3 * (100 - n) / 50 : n <= 68 ? n3 * (150 - n) / 100 : n <= 98 ? n3 * ((1911 - 10 * n) / 3) / 500 : n3 * (160 - n) / 100);
        case 2: return (int)(n <= 15 ? n3 * ((n + 1) / 3 + 24) / 50 : n <= 36 ? n3 * (n + 14) / 50 : n3 * (n / 2 + 32) / 50);
        case 3: return (int)(6 * n3 / 5 - 15LL * n * n + 100 * n - 140);
        case 4: return (int)(4 * n3 / 5);
        case 5: return (int)(5 * n3 / 4);
        default: return (int)n3;
        }
    }
    GbaCompanion::Mon BoxFilled(GbaCompanion::Mon m)
    {
        const GbaDex::Stats st = dex.Get(m.species);
        int level = 1;
        while (level < 100 && ExpFor(level + 1, st.growth) <= (int)m.exp) level++;
        m.level = level;
        static const int baseOf[6] = { 0, 1, 2, 5, 3, 4 };   // (the card's order from gba_dex.h's)
        for (int k = 0; k < 6; k++)
        {
            const int c0 = (2 * st.base[baseOf[k]] + m.iv[k] + m.ev[k] / 4) * level / 100;
            if (k == 0) { m.stats[0] = dex.Dex(m.species) == 292 ? 1 : c0 + level + 10; continue; }   // (Shedinja: 1)
            int v = c0 + 5;
            const int nat = (int)(m.personality % 25), up = 1 + nat / 5, down = 1 + nat % 5;
            if (up != down) { if (k == up) v = v * 110 / 100; if (k == down) v = v * 90 / 100; }
            m.stats[k] = v;
        }
        m.maxHp = m.hp = m.stats[0];
        return m;
    }
    GbaGlass::MonCard BoxCard(const GbaCompanion::Mon& raw)
    {
        const GbaCompanion::Mon m = BoxFilled(raw);
        auto it = boxPics.find(m.species);
        if (it == boxPics.end()) it = boxPics.emplace(m.species, dex.Front(m.species, false)).first;
        return MonCardOf(m, it->second.Ready() ? &it->second : nullptr);
    }
    const GbaBoxes::Slot* BoxSlot(int b, int slot) const
    {
        if (b < 0 || b >= (int)boxList.size()) return nullptr;
        for (const auto& sl : boxList[b].mons) if (sl.slot == slot) return &sl;
        return nullptr;
    }
    std::string BoxName(int b) const
    {
        if (b < 0 || b >= (int)boxList.size()) return {};
        std::string n = GbaDex::Text((const uint8_t*)boxList[b].name.data(), boxList[b].name.size());
        while (!n.empty() && n.back() == ' ') n.pop_back();
        return n.empty() ? "Box " + std::to_string(b + 1) : DexTitle(n);
    }
    void BoxDone(GbaBoxes::Result r, const char* done)
    {
        Toast(r == GbaBoxes::DONE ? done : GbaBoxes::Why(r), 2500);
        ReadCompanion();
        boxList = boxes.Read();
    }
    // BOXES' pages: A on a box Pokemon for what to do with it; A on a party one (PARTY) to put it away.
    bool BoxesInput(const MenuIn& in)
    {
        const bool sel = pause.selected >= 0 && pause.selected < (int)pause.items.size();
        const std::string l = sel ? pause.items[pause.selected].label : std::string();
        if (pausePage == P_PARTY)
        {
            if (!sel || l == "BACK" || pause.selected >= (int)comp.party.size() || comp.inBattle) return false;
            partyPick = pause.selected;
            GoPage(P_PARTYMON, "TAKE ITEM");
            if (pause.items.size() <= 1) { GoPage(P_PARTY, ""); pause.selected = partyPick; }
            return true;
        }
        if (pausePage == P_BOXES)
        {
            if (in.accept && l.rfind("B ", 0) == 0)
            {
                std::sscanf(l.c_str(), "B %d %d", &boxPick, &boxPickSlot);
                GoPage(P_BOXMON, "");
                return true;
            }
            return false;   // (BACK and B: the companion's own)
        }
        if (in.back || (in.accept && l == "BACK"))
        {
            if (pausePage == P_BOXMON) GoPage(P_BOXES, ("B " + std::to_string(boxPick) + " " + std::to_string(boxPickSlot)).c_str());
            else GoPage(P_PARTY, "");
            if (pausePage == P_PARTY && partyPick >= 0 && partyPick < (int)pause.items.size()) pause.selected = partyPick;
            return true;
        }
        if (!in.accept || !sel) return true;
        if (pausePage == P_PARTYMON && l == "DEPOSIT" && partyPick >= 0 && partyPick < (int)comp.party.size())
        {
            if (boxList.empty()) boxList = boxes.Read();
            const int box = boxes.CurrentBox();
            BoxDone(boxes.Deposit(partyPick, comp.party[partyPick].personality, box), "PUT IN THE BOX");
            GoPage(P_PARTY, "");
            return true;
        }
        const GbaBoxes::Slot* bs = BoxSlot(boxPick, boxPickSlot);
        if (pausePage == P_BOXMON && bs)
        {
            const uint32_t pers = bs->mon.personality;
            if (l == "WITHDRAW") BoxDone(boxes.Withdraw(boxPick, boxPickSlot, pers), "TAKEN INTO YOUR PARTY");
            else if (l.rfind("SWAP ", 0) == 0)
            {
                const int ps = std::atoi(l.c_str() + 5);
                if (ps >= 0 && ps < (int)comp.party.size()) BoxDone(boxes.Swap(boxPick, boxPickSlot, pers, ps, comp.party[ps].personality), "SWAPPED");
            }
            GoPage(P_BOXES, "");
        }
        return true;
    }
    template <class Page, class Put>
    void BoxesGlass(GbaGlass::Page& g, int n, Page page, Put put)
    {
        using namespace GbaGlass;
        if (pausePage == P_BOXES)
        {
            page("BOXES", IC_LOAD);
            // (The chosen row's Pokemon in the preview, its card worked out as it would come out of the box.)
            for (int i = 0; i < n; i++)
            {
                const std::string& l = pause.items[i].label;
                Item it;
                if (l.rfind("#BOX ", 0) == 0)
                {
                    const int b = std::atoi(l.c_str() + 5);
                    it.label = BoxName(b);
                    it.kind = SECTION;
                    it.icon = IC_LOAD;
                    put(it, -1);
                    continue;
                }
                if (l.rfind("B ", 0) != 0) { if (l != "BACK") { it.label = l; it.info = true; put(it, i); } continue; }
                int b = 0, slot = 0;
                std::sscanf(l.c_str(), "B %d %d", &b, &slot);
                const GbaBoxes::Slot* bs = BoxSlot(b, slot);
                if (!bs) continue;
                const GbaCompanion::Mon m = BoxFilled(bs->mon);
                std::string nick = GbaDex::Text((const uint8_t*)m.nickname.data(), m.nickname.size());
                while (!nick.empty() && nick.back() == ' ') nick.pop_back();
                it.label = m.egg ? std::string("Egg") : DexTitle(nick.empty() ? dex.Name(m.species) : nick);
                it.value = m.egg ? "EGG" : "LV " + std::to_string(m.level);
                it.kind = LINK;
                if (i == pause.selected) { g.mons.push_back(BoxCard(bs->mon)); it.monCard = (int)g.mons.size() - 1; }
                put(it, i);
            }
            return;
        }
        const bool boxMon = pausePage == P_BOXMON;
        page(boxMon ? "BOX POKEMON" : "PARTY POKEMON", boxMon ? IC_LOAD : IC_PEOPLE);
        const GbaBoxes::Slot* bs = BoxSlot(boxPick, boxPickSlot);
        if (boxMon && bs) g.mons.push_back(BoxCard(bs->mon));
        else if (!boxMon && partyPick >= 0 && partyPick < (int)comp.party.size()) g.mons.push_back(MonCardFor(partyPick));
        for (int i = 0; i < n; i++)
        {
            const std::string& l = pause.items[i].label;
            if (l == "BACK") continue;
            Item it;
            it.kind = LINK;
            if (l == "WITHDRAW") { it.label = "Take into your party"; it.icon = IC_PEOPLE; }
            else if (l == "TAKE ITEM") { it.label = "Take its " + (partyPick >= 0 && partyPick < (int)comp.party.size() ? DexTitle(dex.ItemName(comp.party[partyPick].item)) : std::string("item")); it.icon = IC_LOAD; }
            else if (l == "DEPOSIT") { it.label = "Put in " + BoxName(boxes.CurrentBox()); it.icon = IC_LOAD; if (boxList.empty()) boxList = boxes.Read(); it.label = "Put in " + BoxName(boxes.CurrentBox()); }
            else if (l.rfind("SWAP ", 0) == 0)
            {
                const int ps = std::atoi(l.c_str() + 5);
                it.label = "Swap with " + (ps < (int)comp.party.size() ? MonCardFor(ps).name : std::string("?"));
                it.icon = IC_FAST;
            }
            else continue;
            if (!g.mons.empty()) it.monCard = 0;
            put(it, i);
        }
    }
    // FLY ON > ANY POKEMON: the National Pokedex number picked (Charizard to begin with).
    int FlyMonChosen() const { const int n = std::atoi(set.Get("fly_on_mon").c_str()); return n >= 1 && n <= 493 ? n : 6; }
    // A Pokemon's name by National Pokedex number: the game's own up to #386, then the Sinnoh ones'.
    std::string MonName(int national)
    {
        if (national > 386) return SinnohNames::Name(national);
        if (!dexTried) { dexTried = true; dex.Load(dexRom); }
        const std::string n = dex.Ready() ? dex.Name(dex.Species(national)) : std::string();
        return n.empty() ? std::string() : DexTitle(n);
    }
    // FLY ON's preview: the Pokemon as it will carry you (HeartGold's sprite facing you, or Emerald's own), ARGB; kept.
    struct MountPic { std::vector<uint32_t> px; int w = 0, h = 0; bool emerald = false; };
    std::map<int, MountPic> mountPics;
    const MountPic& MountPicture(int national)
    {
        auto it = mountPics.find(national);
        if (it != mountPics.end()) return it->second;
        MountPic m;
        uint8_t frames[6 * 512], pal[32];
        auto argb = [](unsigned c) { const unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31; return 0xFF000000u | (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2); };
        if (followState == 2 && follow && follow->Mon(national, false, frames, pal))
        {
            m.w = m.h = 32;
            m.px.assign(32 * 32, 0);
            for (int y = 0; y < 32; y++)
                for (int x = 0; x < 32; x++)
                {
                    const uint8_t b = frames[((y / 8) * 4 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2], i = (x & 1) ? b >> 4 : b & 15;
                    if (i) m.px[y * 32 + x] = argb(pal[i * 2] | pal[i * 2 + 1] << 8);
                }
        }
        else
        {
            // (As the mount: Emerald's own legendary, else the party icon, else FireRed and LeafGreen's legendary.)
            const char* legend = LegendFor(national);
            const bool kanto = gameCode.rfind("BPR", 0) == 0 || gameCode.rfind("BPG", 0) == 0;
            std::vector<uint8_t> px;
            uint32_t abgr[16];
            bool got = legend && !kanto && LegendFrames(legend, px, m.w, m.h, abgr);
            if (!got && IconMount(national, &px, abgr)) { got = true; m.w = m.h = 32; px.resize(32 * 32); }
            if (!got && legend) got = LegendFrames(legend, px, m.w, m.h, abgr);
            if (got)
            {
                m.emerald = true;
                m.px.assign((size_t)m.w * m.h, 0);
                for (size_t i = 0; i < m.px.size(); i++)
                    if (px[i] & 15) { const uint32_t c = abgr[px[i] & 15]; m.px[i] = 0xFF000000u | (c & 0xFF) << 16 | (c & 0xFF00) | (c >> 16 & 0xFF); }
            }
        }
        if (followState != 1) return mountPics[national] = m;   // (not kept while HeartGold is still being looked for)
        static MountPic none;
        none = m;
        return none;
    }
    std::string FlyOnLabel()
    {
        const std::string on = set.Get("fly_on");
        if (on == "any")
        {
            const std::string name = MonName(FlyMonChosen());
            return name.empty() ? std::string("#") + std::to_string(FlyMonChosen()) : name;
        }
        for (const Setting& st : SettingsList())
            if (!strcmp(st.key, "fly_on"))
                for (const auto& o : st.values) if (o.first == on) return DexTitle(o.second);
        return "Bird";
    }
    // A Pokemon's party icon from the game's own ROM as a mount: 32 x 32, two frames, facing west as most Gen 3 icons do.
    // gMonIconTable, gMonIconPaletteIndices and gMonIconPaletteTable sit one after another in every GBA game: the palette
    // table found by its tags (56000 on), the indices just before it, the icon pointers before those. Found once per ROM.
    uint32_t iconTable = 0, iconIdx = 0, iconPals = 0;
    int iconCount = 0, iconPalCount = 0;
    bool IconMount(int national, std::vector<uint8_t>* outPx = nullptr, uint32_t* outPal = nullptr)
    {
        const struct GBA* gba = (const struct GBA*)core->board;
        const uint8_t* rom = (const uint8_t*)gba->memory.rom;
        const size_t size = gba->memory.romSize;
        auto r32 = [&](uint32_t a) -> uint32_t { const size_t o = a - 0x08000000u; return o + 4 <= size ? rom[o] | rom[o + 1] << 8 | rom[o + 2] << 16 | (uint32_t)rom[o + 3] << 24 : 0; };
        auto r16 = [&](uint32_t a) -> uint32_t { const size_t o = a - 0x08000000u; return o + 2 <= size ? (uint32_t)(rom[o] | rom[o + 1] << 8) : 0; };
        auto isRom = [&](uint32_t v) { return v >= 0x08000000u && v - 0x08000000u + 0x400 <= size; };
        if (!iconTable)
        {
            iconTable = 1;
            for (size_t o = 0; o + 24 <= size; o += 4)
            {
                const uint32_t a = 0x08000000u + (uint32_t)o;
                if (r16(a + 4) != 56000 || r16(a + 12) != 56001 || r16(a + 20) != 56002 || !isRom(r32(a)) || !isRom(r32(a + 8))) continue;
                int np = 0;
                while (np < 16 && r16(a + 8u * (uint32_t)np + 4) == 56000u + (uint32_t)np) np++;
                size_t k = o;
                while (k > 0 && rom[k - 1] < np) k--;
                size_t j = k & ~(size_t)3;
                int n = 0;
                while (j >= 4 && isRom(r32(0x08000000u + (uint32_t)(j - 4)))) { j -= 4; n++; }
                const int idx = (int)(o - k);
                if (n < 300 || idx < 300) continue;
                iconTable = 0x08000000u + (uint32_t)j; iconIdx = 0x08000000u + (uint32_t)k; iconPals = a;
                iconCount = std::min(n, idx); iconPalCount = np;
                break;
            }
        }
        if (iconTable < 0x08000000u || national <= 0) return false;
        // (The game's own species number for the National Pokedex one.)
        if (!dexTried) { dexTried = true; dex.Load(dexRom); }
        int sp = 0;
        if (dex.Ready()) { for (int i = 1; i < std::min(dex.count + 1, iconCount) && !sp; i++) if (dex.Dex(i) == national) sp = i; }
        else if (national <= 251) sp = national;
        if (sp <= 0 || sp >= iconCount) return false;
        const uint32_t gfx = r32(iconTable + 4u * (uint32_t)sp);
        const int p = rom[iconIdx - 0x08000000u + (uint32_t)sp];
        if (!isRom(gfx) || p >= iconPalCount) return false;
        const uint32_t palData = r32(iconPals + 8u * (uint32_t)p);
        if (!isRom(palData)) return false;
        uint32_t pal[16];
        for (int i = 0; i < 16; i++)
        {
            const unsigned c = r16(palData + 2u * (uint32_t)i), rr = c & 31, gg = (c >> 5) & 31, bb = (c >> 10) & 31;
            pal[i] = 0xFF000000u | ((bb << 3 | bb >> 2) << 16) | ((gg << 3 | gg >> 2) << 8) | (rr << 3 | rr >> 2);
        }
        std::vector<uint8_t> px(2 * 32 * 32, 0);
        bool any = false;
        for (int f = 0; f < 2; f++)
            for (int y = 0; y < 32; y++)
                for (int x = 0; x < 32; x++)
                {
                    const uint8_t b = rom[gfx - 0x08000000u + (uint32_t)(f * 0x200 + ((y / 8) * 4 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2)];
                    px[(size_t)f * 1024 + y * 32 + x] = (x & 1) ? b >> 4 : b & 15;
                    any = any || px[(size_t)f * 1024 + y * 32 + x];
                }
        if (!any) return false;
        if (outPx) { *outPx = px; memcpy(outPal, pal, sizeof pal); return true; }   // (a preview: not the mount)
        pk_flight_set_mount(px.data(), 32, 32, pal);
        return true;
    }
    // FLY ON's named legendaries, by National Pokedex number (0: not one).
    static int LegendNational(const std::string& on)
    {
        static const std::pair<const char*, int> l[] = { { "latias", 380 }, { "latios", 381 }, { "rayquaza", 384 }, { "lugia", 249 }, { "hooh", 250 }, { "deoxys", 386 }, { "articuno", 144 }, { "zapdos", 145 }, { "moltres", 146 } };
        for (const auto& x : l) if (on == x.first) return x.second;
        return 0;
    }
    // Emerald's own sprite for the Pokemon HeartGold has none of (the very big ones), by National Pokedex number.
    static const char* LegendFor(int national)
    {
        switch (national) { case 249: return "lugia"; case 250: return "hooh"; case 384: return "rayquaza"; case 380: return "latias"; case 381: return "latios"; case 386: return "deoxys"; case 144: return "articuno"; case 145: return "zapdos"; case 146: return "moltres"; default: return nullptr; }
    }
    // The Pokemon the list offers: those with a sprite in HeartGold / SoulSilver (all of them while it's still being looked
    // for), and Emerald's legendaries.
    std::vector<char> hgssHas;
    std::vector<int> FlyMonList()
    {
        if (!dexTried) { dexTried = true; dex.Load(dexRom); }
        std::vector<int> out;
        if (followState == 2 && follow && hgssHas.empty())
        {
            hgssHas.assign(494, 0);
            uint8_t frames[6 * 512], pal[32];
            for (int nat = 1; nat <= 493; nat++) hgssHas[nat] = follow->Mon(nat, false, frames, pal) ? 1 : 0;
        }
        for (int nat = 1; nat <= 493; nat++)
            if ((hgssHas.empty() ? followState != 3 : hgssHas[nat] != 0) || nat <= 386) out.push_back(nat);   // (up to Deoxys: the game's own party icon at least)
        return out;
    }
    // A Pokemon to fly on, by National Pokedex number: HeartGold's sprite, else Emerald's own for its legendaries.
    bool MonMount(int national, bool shiny)
    {
        uint8_t frames[6 * 512], pal[32];
        if (followState == 2 && follow && national > 0 && follow->Mon(national, shiny, frames, pal))
        {
            // (The west-facing frames, 4 and 5, from 4bpp tiles in 1D order to a palette index a pixel.)
            uint8_t px[2][32 * 32];
            for (int f = 0; f < 2; f++)
                for (int y = 0; y < 32; y++)
                    for (int x = 0; x < 32; x++)
                    {
                        const uint8_t b = frames[(4 + f) * 512 + ((y / 8) * 4 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2];
                        px[f][y * 32 + x] = (x & 1) ? b >> 4 : b & 15;
                    }
            uint32_t abgr[16];
            for (int i = 0; i < 16; i++)
            {
                const unsigned c = pal[i * 2] | pal[i * 2 + 1] << 8;
                const unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
                abgr[i] = 0xFF000000u | ((b << 3 | b >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (r << 3 | r >> 2);
            }
            pk_flight_set_mount(&px[0][0], 32, 32, abgr);
            return true;
        }
        // (Without HeartGold / SoulSilver: Emerald's own legendaries, which face sideways; then the game's party icon,
        // which faces west; FireRed and LeafGreen's legendaries face only the front, so they come last.)
        const char* legend = LegendFor(national);
        const bool kanto = gameCode.rfind("BPR", 0) == 0 || gameCode.rfind("BPG", 0) == 0;
        if (legend && !kanto && LegendMount(legend)) return true;
        if (IconMount(national)) return true;
        return legend && LegendMount(legend);
    }
    // FLY ON: the vehicle word (PokeDaisy's vehicles from 1, PK_FLIGHT_MOUNT a Pokemon), the Pokemon in party slot [slot]
    // from HeartGold / SoulSilver's sprites. Before each take-off.
    void FlightMount(int slot)
    {
        if (!flightRec) return;
        const std::string on = set.Get("fly_on");
        uint32_t v = on == "balloon" ? 1 : on == "helicopter" ? 2 : on == "zeppelin" ? 3 : on == "plane" ? 4 : 0;
        if (on == "pokemon" || on == "any")
        {
            if (followState == 0) FollowFind();
            int national = 0;
            bool shiny = false;
            if (on == "any") national = FlyMonChosen();
            else if (compConfig && slot >= 0)
            {
                uint8_t m[100];
                for (int i = 0; i < 100; i++) m[i] = (uint8_t)core->busRead8(core, compConfig->party + (uint32_t)slot * 100 + (uint32_t)i);
                GbaCompanion::Mon mon;
                if (GbaCompanion::Decode(m, mon) && !mon.egg && mon.species)
                {
                    if (!dexTried) { dexTried = true; dex.Load(dexRom); }
                    national = dex.Ready() ? dex.Dex(mon.species) : (mon.species <= 251 ? mon.species : 0);
                    shiny = ((mon.otId >> 16) ^ (mon.otId & 0xFFFF) ^ (mon.personality >> 16) ^ (mon.personality & 0xFFFF)) < 8;
                }
            }
            if (national > 0 && MonMount(national, shiny)) v = PK_FLIGHT_MOUNT;
            else
            {
                pk_flight_set_mount(nullptr, 0, 0, nullptr);
                Toast(followState == 3 && !LegendFor(national) ? "NO HEARTGOLD OR SOULSILVER IN THE ROMS FOLDER: FLYING ON THE BIRD" : "NO SPRITE FOR THIS POKEMON: FLYING ON THE BIRD", 3000);
            }
        }
        if (const int nat = LegendNational(on))
        {
            // (HeartGold / SoulSilver's sprite first, which faces the way it flies; else the game's own, which in FireRed
            // and LeafGreen only faces the front.)
            if (followState == 0) FollowFind();
            if (MonMount(nat, false)) v = PK_FLIGHT_MOUNT;
            else Toast("THIS ONE ISN'T IN THIS GAME: FLYING ON THE BIRD", 3000);
        }
        uint32_t w[PK_FLIGHT_WORDS];
        memcpy(w, flightRec->words, sizeof(w));
        w[PK_FLIGHT_VEHICLE] = v;
        pk_flight_configure(w, PK_FLIGHT_WORDS);
    }
    // After every frame: the flight's next step (the game's keys: the D-pad, A, B), then its shadow, clouds and bird
    // into the picture (the widescreen sides' clouds too).
    void FlightFrame()
    {
        if (!flightRec) return;
        pk_flight_tick(core);
        const int e = std::max(0, wideExtra);
        const bool view = FlightView();   // (ZoomFrame draws the picture through WIDE, its clouds over all of it)
        pk_flight_draw(video.data() + e, GbaW, GbaH, MaxW, view ? 0 : 1);
        if (e > 0 && !view)
        {
            PkFlightClouds c;
            pk_flight_clouds_plan(&c, -e, 0, GbaW, GbaH);
            pk_flight_clouds_margins(&c, video.data(), GbaW + 2 * e, GbaH);
        }
    }
    // (Tests: fly-test.txt beside the program, lines "go GAMEFRAME TOWN-INDEX" and "page GAMEFRAME"; what FLY offered
    // and did goes to fly-log.txt.)
    void FlyTest()
    {
        static int state = -1;
        static char itAct[8] = {};
        static int goAt = -1, goIndex = 0, pageAt = -1, freeAt = -1, monPageAt = -1, boxPageAt = -1, boxSwapAt = -1, depositAt = -1, itemAt = -1, bagPageAt = -1, itId = 0, itSlot = 0, bxB = 0, bxS = 0, bxP = 0, traceFrom = -1, traceTo = -1, foeAt = -1, foeDone = -1, legendAt = -1;
        if (state < 0)
        {
            state = 0;
            if (FILE* f = fopen("fly-test.txt", "r"))
            {
                flyTesting = true;
                char w[16]; int a = 0, b = 0;
                while (fscanf(f, "%15s %d", w, &a) == 2)
                {
                    if (!strcmp(w, "go") && fscanf(f, "%d", &b) == 1) { goAt = a; goIndex = b; }
                    else if (!strcmp(w, "page")) pageAt = a;
                    else if (!strcmp(w, "free")) freeAt = a;
                    else if (!strcmp(w, "monpage")) monPageAt = a;
                    else if (!strcmp(w, "boxpage")) boxPageAt = a;
                    else if (!strcmp(w, "item") && fscanf(f, "%7s %d %d", itAct, &itId, &itSlot) == 3) itemAt = a;
                    else if (!strcmp(w, "bagpage")) bagPageAt = a;
                    else if (!strcmp(w, "boxswap") && fscanf(f, "%d %d %d", &bxB, &bxS, &bxP) == 3) boxSwapAt = a;
                    else if (!strcmp(w, "deposit") && fscanf(f, "%d", &bxP) == 1) depositAt = a;
                    else if (!strcmp(w, "trace") && fscanf(f, "%d", &b) == 1) { traceFrom = a; traceTo = b; }
                    else if (!strcmp(w, "foecopy")) foeAt = a;
                    else if (!strcmp(w, "legends")) legendAt = a;
                }
                fclose(f);
            }
        }
        const long long gf = (long long)gameFrames;
        if (gf == freeAt) { flyStage = 10; flyWait = 0; flyHgssWait = 0; }
        // (legends F: every legendary mount this game has, its two frames side by side, written to legend-NAME.bmp.)
        if (gf == legendAt)
            for (const char* n : { "latias", "latios", "rayquaza", "lugia", "hooh", "deoxys", "articuno", "zapdos", "moltres" })
            {
                std::vector<uint8_t> px;
                int w = 0, h = 0;
                uint32_t pal[16];
                const bool ok = LegendFrames(n, px, w, h, pal);
                if (FILE* l = fopen("fly-log.txt", "a")) { fprintf(l, "legend %s %d %dx%d table %08x\n", n, (int)ok, w, h, objGfxTable); fclose(l); }
                if (!ok) continue;
                const int W = w * 2, rowBytes = (W * 3 + 3) & ~3;
                std::vector<uint8_t> bmp(54 + (size_t)rowBytes * h, 0);
                auto put32 = [&](size_t o, uint32_t v) { for (int k = 0; k < 4; k++) bmp[o + k] = (uint8_t)(v >> (8 * k)); };
                bmp[0] = 'B'; bmp[1] = 'M'; put32(2, (uint32_t)bmp.size()); put32(10, 54); put32(14, 40); put32(18, (uint32_t)W); put32(22, (uint32_t)h);
                bmp[26] = 1; bmp[28] = 24;
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < W; x++)
                    {
                        const int f = x / w, i = px[(size_t)f * w * h + (size_t)y * w + (x % w)];
                        const uint32_t c = i ? pal[i] : 0xFF808080u;
                        uint8_t* d = &bmp[54 + (size_t)(h - 1 - y) * rowBytes + (size_t)x * 3];
                        d[0] = (uint8_t)(c >> 16); d[1] = (uint8_t)(c >> 8); d[2] = (uint8_t)c;
                    }
                if (FILE* b = fopen((std::string("legend-") + n + ".bmp").c_str(), "wb")) { fwrite(bmp.data(), 1, bmp.size(), b); fclose(b); }
            }
        // (foecopy F: from frame F, in the first wild battle, your first two Pokemon copied into the foe's party slots 2
        // and 3, the second at 1 HP, so its team reads as a trainer's; BATTLE INFO opened a second later, its rows logged.)
        if (foeAt >= 0 && foeDone < 0 && gf >= foeAt && gf % 30 == 0 && compConfig)
        {
            ReadCompanion();
            if (comp.inBattle && comp.foeParty.size() == 1 && comp.party.size() >= 2)
                for (int d : { 600, -600, 608 })
                {
                    const uint32_t at = compConfig->party + (uint32_t)d;
                    if (core->busRead32(core, at) != comp.foe.personality) continue;
                    for (int i = 0; i < 200; i++) core->busWrite8(core, at + 100 + (uint32_t)i, (uint8_t)core->busRead8(core, compConfig->party + (uint32_t)i));
                    core->busWrite16(core, at + 200 + 86, 1);
                    foeDone = (int)gf;
                    if (FILE* l = fopen("fly-log.txt", "a")) { fprintf(l, "foecopy frame %d at %08x (party %+d)\n", (int)gf, at, d); fclose(l); }
                    break;
                }
        }
        if (foeDone >= 0 && gf == foeDone + 60)
        {
            OpenPause();
            ReadCompanion();
            GoPage(P_BATTLE, "");
            pause.selected = std::max(0, (int)pause.items.size() - 2);   // (scrolled to the team, at the end)
            if (FILE* l = fopen("fly-log.txt", "a"))
            {
                for (const auto& r : BattleRows()) fprintf(l, "row %s | %s\n", r.first.c_str(), r.second.c_str());
                fclose(l);
            }
        }
        if (gf == itemAt || gf == bagPageAt)
        {
            ReadCompanion();
            if (FILE* l = fopen("fly-log.txt", "a"))
            {
                fprintf(l, "items frame %d supported %d party %d:", (int)gf, (int)items.Supported(), (int)comp.party.size());
                for (const auto& m : comp.party) fprintf(l, " %d(item %d hp %d/%d)", m.species, m.item, m.hp, m.maxHp);
                fprintf(l, "\n");
                if (gf == itemAt)
                {
                    int r = -1, extra = 0;
                    GbaItems::Result why = GbaItems::USED;
                    if (!strcmp(itAct, "repel")) r = (int)items.UseRepel(itId, extra);
                    else if (!strcmp(itAct, "teach")) { r = (int)items.TmStart(itId); if (r == (int)GbaItems::USED) { tmItem = itId; tmWait = 0; } }
                    else if (!strcmp(itAct, "take") && itSlot < (int)comp.party.size()) r = (int)items.Take(items.Slot(itSlot, comp.party[itSlot].personality, comp.party[itSlot].item), extra);
                    else
                    {
                        auto t = items.Targets(itId, !strcmp(itAct, "give"), why);
                        fprintf(l, "targets %d why %d:", (int)t.size(), (int)why);
                        for (const auto& x : t) fprintf(l, " [%d v%d held %d mv %d%d%d%d]", x.slot, (int)x.verdict, x.held, x.moves[0], x.moves[1], x.moves[2], x.moves[3]);
                        fprintf(l, "\n");
                        if (itSlot < (int)t.size()) r = !strcmp(itAct, "give") ? (int)items.Give(itId, t[itSlot], extra) : (int)items.UseOn(itId, t[itSlot], 0);
                    }
                    fprintf(l, "item %s %d slot %d: result %d extra %d\n", itAct, itId, itSlot, r, extra);
                    ReadCompanion();
                    fprintf(l, "after:");
                    for (const auto& m : comp.party) fprintf(l, " %d(item %d)", m.species, m.item);
                    fprintf(l, "\n");
                }
                fclose(l);
            }
            if (gf == bagPageAt) { OpenPause(); GoPage(P_BAG, ""); }
        }
        if (gf == boxPageAt || gf == boxSwapAt || gf == depositAt)
        {
            ReadCompanion();
            boxList = boxes.Read();
            if (FILE* l = fopen("fly-log.txt", "a"))
            {
                fprintf(l, "boxes frame %d supported %d current %d party %d:", (int)gf, (int)boxes.Supported(), boxes.CurrentBox(), (int)comp.party.size());
                for (int b = 0; b < (int)boxList.size(); b++) if (!boxList[b].mons.empty()) fprintf(l, " box%d=%d", b, (int)boxList[b].mons.size());
                fprintf(l, "\n");
                if (gf == boxSwapAt)
                {
                    const GbaBoxes::Slot* bs = BoxSlot(bxB, bxS);
                    const int r = bs && bxP < (int)comp.party.size() ? (int)boxes.Swap(bxB, bxS, bs->mon.personality, bxP, comp.party[bxP].personality) : -1;
                    fprintf(l, "swap box %d slot %d party %d: %d\n", bxB, bxS, bxP, r);
                }
                if (gf == depositAt)
                {
                    const int r = bxP < (int)comp.party.size() ? (int)boxes.Deposit(bxP, comp.party[bxP].personality, boxes.CurrentBox()) : -1;
                    fprintf(l, "deposit party %d: %d\n", bxP, r);
                }
                fclose(l);
            }
            if (gf == boxPageAt) { OpenPause(); GoPage(P_BOXES, ""); if (pause.items.size() > 2) pause.selected = 1; }
        }
        if (gf == monPageAt) { OpenPause(); if (followState == 0) FollowFind(); flyT = fly.Now(); GoPage(P_FLYMON, ("MON " + std::to_string(FlyMonChosen())).c_str()); }
        if (FILE* l = gf >= traceFrom && gf <= traceTo ? fopen("fly-log.txt", "a") : nullptr)
        {
            char g[256] = {}, info[256] = {};
            pk_wide_flight_debug(g, sizeof g);
            pk_flight_info(core, info, sizeof info);
            const char* at = strstr(info, " at ");
            fprintf(l, "trace %d | %s |%.12s\n", (int)gf, g, at ? at : "");
            fclose(l);
        }
        if (FILE* l = flightRec && pk_flight_state() != PK_FLIGHT_IDLE && gf % 30 == 0 ? fopen("fly-log.txt", "a") : nullptr)
        {
            char info[256] = {};
            pk_flight_info(core, info, sizeof info);
            fprintf(l, "flight frame %d state %d | %s\n", (int)gf, pk_flight_state(), info);
            fclose(l);
        }
        if (gf != goAt && gf != pageAt) return;
        if (!dexTried) { dexTried = true; dex.Load(dexRom); }
        flyT = fly.Now();
        if (FILE* l = fopen("fly-log.txt", "a"))
        {
            fprintf(l, "frame %d table %s flight %s result %d slot %d towns", (int)gf, fly.Name(), flightRec ? flightRec->name : "-", (int)flyT.result, flyT.slot);
            for (int t : flyT.towns) fprintf(l, " %d:%s", t, dex.MapsecName(t).c_str());
            fprintf(l, "\n");
            fclose(l);
        }
        if (gf == goAt && flyT.result == GbaFly::OK && goIndex < (int)flyT.towns.size())
        {
            flySec = flyT.towns[goIndex];
            flyStage = 1;
            flyWait = 0;
        }
        if (gf == pageAt && flyT.result == GbaFly::OK) { OpenPause(); GoPage(P_FLY, ""); }
    }
    std::unique_ptr<HgssFollow::Source> follow;
    std::thread followFind;
    std::atomic<int> followState{ 0 }; // 0 not looked for, 1 looking, 2 found, 3 none
    int followSpecies = -1, followTick = 0;
    bool followShiny = false, followShown = false, followToldNone = false;
    void FollowFind()
    {
        followState = 1;
        const std::string dir = roms;
        followFind = std::thread([this, dir] {
            namespace fs = std::filesystem;
            std::error_code ec;
            auto src = std::make_unique<HgssFollow::Source>();
            bool ok = false;
            // (The GBA games' folder, and the one it is in: the launcher keeps the DS games there.)
            for (const fs::path& top : { fs::path(dir), fs::path(dir).parent_path() })
            {
                ec.clear();
                for (auto it = fs::recursive_directory_iterator(top, fs::directory_options::skip_permission_denied, ec);
                     !ok && !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
                {
                    if (it.depth() > 2) { it.disable_recursion_pending(); continue; }
                    std::string ext = it->path().extension().string();
                    for (char& c : ext) c = (char)tolower((unsigned char)c);
                    if (ext == ".nds" && it->is_regular_file(ec))
                    {
                        ok = src->Open(it->path().string());
                        if (getenv("GBAW_FOLLOW_LOG"))
                            if (FILE* flog = fopen(getenv("GBAW_FOLLOW_LOG"), "a")) { fprintf(flog, "  %s: %s\n", it->path().string().c_str(), ok ? "HeartGold or SoulSilver" : "no"); fclose(flog); }
                    }
                }
                if (getenv("GBAW_FOLLOW_LOG"))
                    if (FILE* flog = fopen(getenv("GBAW_FOLLOW_LOG"), "a")) { fprintf(flog, "  folder %s (%s)\n", top.string().c_str(), ec ? ec.message().c_str() : "ok"); fclose(flog); }
                if (ok) break;
            }
            if (ok) follow = std::move(src);
            followState = ok ? 2 : 3;
            if (getenv("GBAW_FOLLOW_LOG"))
                if (FILE* flog = fopen(getenv("GBAW_FOLLOW_LOG"), "a")) { fprintf(flog, "looked in %s: %s\n", dir.c_str(), ok ? "found" : "none"); fclose(flog); }
        });
    }
    void FollowerFrame()
    {
        const bool want = FollowerOn();
        if (want && followState == 0) FollowFind();
        if (want && followState == 3 && !followToldNone)
        {
            followToldNone = true;
            Toast("FOLLOWING POKEMON NEEDS HEARTGOLD OR SOULSILVER IN THE ROMS FOLDER", 4000);
        }
        bool show = false;
        if (want && followState == 2 && follow)
        {
            if (followTick++ % 30 == 0)
            {
                uint8_t m[100];
                for (int i = 0; i < 100; i++) m[i] = (uint8_t)core->busRead8(core, compConfig->party + i);
                GbaCompanion::Mon lead;
                int national = 0;
                if (GbaCompanion::Decode(m, lead) && !lead.egg && lead.species)
                {
                    if (!dexTried) { dexTried = true; dex.Load(dexRom); }
                    national = dex.Ready() ? dex.Dex(lead.species) : (lead.species <= 251 ? lead.species : 0);
                }
                const bool shiny = ((lead.otId >> 16) ^ (lead.otId & 0xFFFF) ^ (lead.personality >> 16) ^ (lead.personality & 0xFFFF)) < 8;
                if (national != followSpecies || shiny != followShiny)
                {
                    uint8_t frames[6 * 512], pal[32];
                    if (national > 0 && follow->Mon(national, shiny, frames, pal)) pk_follower_set(frames, pal);
                    else pk_follower_set(nullptr, nullptr);
                    followSpecies = national;
                    followShiny = shiny;
                }
            }
            const int scene = pk_wide_scene();
            show = followSpecies > 0 && (scene == PK_WIDE_SCENE_FIELD || scene == PK_WIDE_SCENE_TEXT) && !(gpState & (PK_FIELD_BATTLE | PK_FIELD_SURF));
        }
        // (Tests: GBAW_FOLLOW_LOG=file, the follower's state every second.)
        static int flogN = 0;
        if (getenv("GBAW_FOLLOW_LOG") && ++flogN % 60 == 1)
            if (FILE* flog = fopen(getenv("GBAW_FOLLOW_LOG"), "a"))
            {
                char info[256] = {};
                pk_follower_info(info, sizeof info);
                fprintf(flog, "want %d state %d species %d scene %d bits %x show %d | %s\n", (int)want, followState.load(), followSpecies, pk_wide_scene(), gpState, (int)show, info);
                fclose(flog);
            }
        if (show != followShown || (show && followTick % 60 == 0))
        {
            pk_follower_show(core, show);
            followShown = show;
        }
    }
    // Once a frame shown: eased toward the hour's tint on the field outdoors, toward none anywhere else.
    void DayNightFrame()
    {
        int want[3] = { 256, 256, 256 };
        if (DayNightOn())
        {
            const int scene = pk_wide_scene(), mt = pk_field_map_type(core);
            const bool field = (scene == PK_WIDE_SCENE_FIELD || scene == PK_WIDE_SCENE_TEXT) && !(gpState & PK_FIELD_BATTLE);
            const bool outdoors = mt < 0 || mt == 1 || mt == 2 || mt == 3 || mt == 6;

            if (field && outdoors)
            {
                const time_t now = time(nullptr);
                const struct tm* lt = localtime(&now);
                const char* fh = getenv("FULLDEX_HOUR"); // (tests: a fixed hour)
                if (fh) DayNightAt((float)atof(fh), want);
                else if (lt) DayNightAt(lt->tm_hour + lt->tm_min / 60.0f, want);
            }
        }
        bool changed = false;
        for (int c = 0; c < 3; c++)
        {
            const float d = want[c] - tintNow[c];
            tintNow[c] = std::fabs(d) < 8 ? (float)want[c] : tintNow[c] + (d > 0 ? 8 : -8);
            if ((int)tintNow[c] != tintSent[c]) { tintSent[c] = (int)tintNow[c]; changed = true; }
        }
        const bool tinted = tintSent[0] != 256 || tintSent[1] != 256 || tintSent[2] != 256;
        if (getenv("FULLDEX_HOUR") && ++dnLog % 120 == 1)
            if (FILE* f = fopen("daynight-log.txt", "a"))
            {
                fprintf(f, "on %d wide %d field %d scene %d maptype %d bits %x tint %d %d %d\n", set.Get("day_night") == "1", (int)pkWide, (int)gpField,
                        pkWide ? pk_wide_scene() : -1, gpField ? pk_field_map_type(core) : -2, gpState, tintSent[0], tintSent[1], tintSent[2]);
                fclose(f);
            }
        if (changed || (tinted && ++tintFrames % 60 == 0)) pk_tint_set(core, tintSent[0], tintSent[1], tintSent[2]);
    }
    int WildKeep() const { return gpWild ? std::max(0, std::min(100, atoi(set.Get("wild_encounters").c_str()))) : 100; }
    // INSTANT TEXT and REUSABLE TMS into the running game, or the ROM's own bytes back.
    void ApplyGameplay()
    {
        if (!core) return;
        auto put = [&](const std::vector<GbaGameplay::Write>& w, bool on, bool& was) {
            if (on == was) return;
            for (const auto& x : w)
            {
                const auto& b = on ? x.value : x.orig;
                for (size_t k = 0; k < b.size(); k++) core->rawWrite8(core, x.addr + (uint32_t)k, -1, b[k]);
            }
            was = on;
        };
        put(gpText, set.Get("instant_text") == "1", gpTextOn);
        put(gpTms, set.Get("reusable_tms") == "1", gpTmsOn);
    }
    void SettingsPageInput(const MenuIn& in)
    {
        const int n = (int)pause.items.size();
        if (n <= 0) return;
        if (in.up) pause.selected = (pause.selected + n - 1) % n;
        if (in.down) pause.selected = (pause.selected + 1) % n;
        if (in.menu)
        {
            Resume();
            return;
        }
        const int i = pause.selected;
        if (in.back || (in.accept && pause.items[i].label == "BACK"))
        {
            const char* from = SettingsPageName();
            pausePage = P_SETTINGS;
            BuildPause();
            pause.selected = 0;
            for (size_t j = 0; j < pause.items.size(); j++)
                if (pause.items[j].label == from) pause.selected = (int)j;
            return;
        }
#ifdef GBAW_PHONE
        if (in.accept && pause.items[i].label == "TOUCH CONTROLS")
        {
            const bool saved = TouchLayout::Edit<Canvas>(ren, touchProfile, data + "/touch-layout-gba.cfg", Touch::Defaults);
            fingers.clear(); touchHeld = touchTap = 0; touchTapFrames = 0;
            std::fill(std::begin(key), std::end(key), false);
            Toast(saved ? "TOUCH LAYOUT SAVED" : "TOUCH LAYOUT UNCHANGED");
            return;
        }
#endif
        if (i < 0 || i >= (int)pageSettings.size()) return;
        const Setting& s = *pageSettings[i];
        int step = in.left ? -1 : 1;
        bool go = in.left || in.right || in.accept;
        // (A tap on the volume's bar, or on one of the preview's choices: straight to that value.)
        const int want = glassSlider >= 0 && !strcmp(s.key, "volume") ? glassSlider : glassPick;
        if (want >= 0 && want < (int)s.values.size())
        {
            int cur = 0;
            const std::string v = set.Get(s.key);
            for (size_t j = 0; j < s.values.size(); j++)
                if (s.values[j].first == v) cur = (int)j;
            step = want - cur;
            go = step != 0;
        }
        if (!go) return;
        set.Step(s, step);
        if (!strcmp(s.key, "fullscreen"))
        {
            const bool want = set.Int("fullscreen");
            const bool full = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
            if (want != full) SDL_SetWindowFullscreen(win, want ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        }
        if (!strcmp(s.key, "volume")) SetVolume();
        if (!strcmp(s.key, "fast_lock")) fastToggle = set.Get("fast_lock") == "1";
        if (!strcmp(s.key, "instant_text") || !strcmp(s.key, "reusable_tms")) ApplyGameplay();
        pause.items[i].value = set.Shown(s);
    }
#endif

#ifdef GBAW_EMBEDDED
    // POKEDEX's words: the games' capitals as the menu writes names ("BULBASAUR": "Bulbasaur").
    static std::string DexTitle(const std::string& t)
    {
        std::string out = t;
        bool start = true;
        for (char& c : out)
        {
            if (c >= 'A' && c <= 'Z' && !start) c = (char)(c - 'A' + 'a');
            start = c == ' ' || c == '-' || c == '.' || c == '/';
        }
        return out;
    }
    static std::string TypeName(int t) { return CompanionTypes::Valid(t) ? DexTitle(CompanionTypes::Name(t)) : std::string(); }
    // The chosen Pokemon's card (gba_dex.h): its pictures decoded once while it stays chosen.
    GbaGlass::DexCard DexCardFor(int species)
    {
        GbaGlass::DexCard c;
        const GbaDex::Stats st = dex.Get(species);
        c.id = dex.Dex(species);
        if (!comp.seen.empty() && c.id > 0 && c.id < (int)comp.seen.size()) c.state = comp.caught[c.id] ? 2 : comp.seen[c.id] ? 1 : 0;
        c.name = DexTitle(dex.Name(species));
        c.type1 = st.type1; c.type2 = st.type2;
        static const int order[6] = { 0, 1, 2, 5, 3, 4 }; // (the card's: HP, Atk, Def, Speed, SpA, SpD)
        for (int i = 0; i < 6; i++) c.base[i] = st.base[order[i]];
        for (int a : { st.ability1, st.ability2 }) if (a && dex.Readable(dex.Ability(a))) c.abilities.push_back(DexTitle(dex.Ability(a)));
        if (st.hidden) c.abilities.push_back(DexTitle(dex.Ability(st.hidden)) + " (hidden)");
        if (dexPicFor != species)
        {
            dexPicFor = species;
            dexPic = dex.Front(species, false);
            dexShiny = dex.Front(species, true);
        }
        if (dexPic.Ready()) { c.pic = dexPic.pixels; c.picW = dexPic.width; c.picH = dexPic.height; if (dexShiny.Ready()) c.shinyPic = dexShiny.pixels; }
        return c;
    }
    // The entry page's rows, label and value; a label starting "#" heads a section (its value says
    // which icon).
    std::vector<std::pair<std::string, std::string>> DexEntryRows()
    {
        std::vector<std::pair<std::string, std::string>> rows;
        const GbaDex::Stats st = dex.Get(dexSpecies);
        if (!st.ok) return rows;
        rows.push_back({ "#ENTRY", "entry" });
        rows.push_back({ DexTitle(dex.Name(dexSpecies)), "" });
        std::string types = TypeName(st.type1);
        if (st.type2 != st.type1 && CompanionTypes::Valid(st.type2)) types += " / " + TypeName(st.type2);
        if (!types.empty()) rows.push_back({ "Type", types });
        rows.push_back({ "#BASE STATS", "stats" });
        static const char* const shown[6] = { "HP", "Attack", "Defence", "Speed", "Sp. Atk", "Sp. Def" }; // (as the DS page)
        static const int order[6] = { 0, 1, 2, 5, 3, 4 }; // (gba_dex.h's: HP, Atk, Def, SpA, SpD, Speed)
        for (int i = 0; i < 6; i++) rows.push_back({ shown[i], std::to_string(st.base[order[i]]) });
        rows.push_back({ "#ABILITIES", "abilities" });
        for (int a : { st.ability1, st.ability2 }) if (a && dex.Readable(dex.Ability(a))) rows.push_back({ DexTitle(dex.Ability(a)), "" });
        if (st.hidden) rows.push_back({ DexTitle(dex.Ability(st.hidden)), "Hidden" });
        {
            // (Where to find: one row a place, how and the levels; as the DS games' page.)
            struct Place { std::string name; std::vector<std::string> ways; int low = 0, high = 0; };
            std::vector<Place> places;
            for (const auto& f : dex.WhereToFind(dexSpecies))
            {
                auto at = std::find_if(places.begin(), places.end(), [&](const Place& p) { return p.name == f.place; });
                if (at == places.end()) { places.push_back({ f.place }); at = places.end() - 1; }
                if (std::find(at->ways.begin(), at->ways.end(), f.method) == at->ways.end()) at->ways.push_back(f.method);
                at->low = at->low ? std::min(at->low, f.low) : f.low; at->high = std::max(at->high, f.high);
            }
            if (dex.wildHeaders != std::string::npos)
            {
                rows.push_back({ "#WHERE TO FIND", "where" });
                if (places.empty()) rows.push_back({ "Not in the wild", "" });
                for (size_t i = 0; i < places.size() && i < 40; i++)
                {
                    std::string how;
                    for (const auto& w : places[i].ways) how += (how.empty() ? "" : ", ") + DexTitle(w);
                    rows.push_back({ DexTitle(places[i].name) + "  \xC2\xB7  " + how,
                                     places[i].high != places[i].low ? "Lv " + std::to_string(places[i].low) + "-" + std::to_string(places[i].high) : "Lv " + std::to_string(places[i].low) });
                }
                if (places.size() > 40) rows.push_back({ "And " + std::to_string(places.size() - 40) + " more places", "" });
            }
        }
        rows.push_back({ "#DETAILS", "details" });
        rows.push_back({ "Catch rate", std::to_string(st.catchRate) });
        char b[64];
        if (st.gender == 255) rows.push_back({ "Gender", "Unknown" });
        else if (st.gender == 0) rows.push_back({ "Gender", "Always male" });
        else if (st.gender == 254) rows.push_back({ "Gender", "Always female" });
        else { snprintf(b, sizeof b, "%.1f%% female", st.gender * 100.0 / 254); std::string g = b; if (g.find(".0%") != std::string::npos) g.erase(g.find(".0%"), 2); rows.push_back({ "Gender", g }); }
        static const char* const eggNames[] = { "", "Monster", "Water 1", "Bug", "Flying", "Field", "Fairy", "Grass", "Human-Like", "Water 3", "Mineral", "Amorphous", "Water 2", "Ditto", "Dragon", "Undiscovered" };
        auto egg = [&](int g) { return g >= 1 && g <= 15 ? std::string(eggNames[g]) : std::to_string(g); };
        rows.push_back({ "Egg groups", egg(st.eggGroup1) + (st.eggGroup2 != st.eggGroup1 ? " / " + egg(st.eggGroup2) : std::string()) });
        snprintf(b, sizeof b, "%d steps", (st.hatchCycles + 1) * 256);
        rows.push_back({ "Hatches in", b });
        static const char* const evNames[] = { "HP", "Attack", "Defence", "Sp. Atk", "Sp. Def", "Speed" };
        std::string evs;
        for (int i = 0; i < 6; i++) if (st.ev[i]) evs += (evs.empty() ? "" : ", ") + std::to_string(st.ev[i]) + " " + evNames[i];
        if (!evs.empty()) rows.push_back({ "EV yield", evs });
        static const char* const growth[] = { "Medium Fast", "Erratic", "Fluctuating", "Medium Slow", "Fast", "Slow" };
        if (st.growth >= 0 && st.growth < 6) rows.push_back({ "Growth", growth[st.growth] });
        rows.push_back({ "Base friendship", std::to_string(st.friendship) });
        if (CompanionTypes::Valid(st.type1))
        {
            rows.push_back({ "#TAKES FROM ATTACKS", "attacks" });
            for (int pct : { 400, 200, 50, 25, 0 })
            {
                std::string types;
                for (int t = 0; t < 18; t++)
                    if (CompanionTypes::Valid(t) && CompanionTypes::Multiplier(t, st.type1, CompanionTypes::Valid(st.type2) ? st.type2 : st.type1) == pct)
                        types += (types.empty() ? "" : ", ") + TypeName(t);
                if (!types.empty())
                    rows.push_back({ pct == 400 ? "4x damage" : pct == 200 ? "2x damage" : pct == 50 ? "Half damage" : pct == 25 ? "Quarter damage" : "No damage", types });
            }
        }
        const auto level = dex.LevelMoves(dexSpecies);
        if (!level.empty())
        {
            rows.push_back({ "#LEARNS BY LEVEL", "level" });
            for (const auto& l : level) rows.push_back({ DexTitle(dex.Move(l.move)), "Lv " + std::to_string(std::max(1, l.level)) });
        }
        const auto machines = dex.Machines(dexSpecies);
        if (!machines.empty())
        {
            rows.push_back({ "#TMS AND HMS", "machines" });
            for (const auto& m : machines) rows.push_back({ DexTitle(dex.Move(m.second)), m.first });
        }
        const auto eggs = dex.EggMoves(dexSpecies);
        if (!eggs.empty())
        {
            rows.push_back({ "#EGG MOVES", "egg" });
            for (int m : eggs) rows.push_back({ DexTitle(dex.Move(m)), "" });
        }
        return rows;
    }
    // COMPANION: the paused game read now (its memory through the core's bus), and the party's
    // pictures from the ROM (gba_dex.h, which COMPANION loads too for names and places).
    void ReadCompanion()
    {
        if (!dexTried) { dexTried = true; dex.Load(dexRom); }
        comp = GbaCompanion::Snapshot();
        compPics.clear();
        if (!compConfig || !core) return;
        comp = GbaCompanion::Take(*compConfig, [&](uint32_t at, uint32_t n, uint8_t* out) {
            const bool ram = (at >= 0x02000000u && at + n <= 0x02040000u) || (at >= 0x03000000u && at + n <= 0x03008000u) ||
                             (at >= 0x08000000u && at + n <= 0x08000000u + dexRom.size());
            if (!ram) return false;
            for (uint32_t i = 0; i < n; i++) out[i] = (uint8_t)core->busRead8(core, at + i);
            return true;
        });
        for (const auto& m : comp.party) compPics.push_back(dex.Front(m.species, false));
        foePic = comp.inBattle ? dex.Front(comp.foe.species, false) : GbaDex::Picture();
        foeTeamPics.clear();
        for (const auto& m : comp.foeParty) foeTeamPics.push_back(dex.Front(m.species, false));
        // (The foe's team: which of it has been out. A new team is a new battle.)
        if (!comp.inBattle || comp.foeParty.empty()) foeSeen.clear();
        else
        {
            if (!foeSeen.empty() && std::none_of(comp.foeParty.begin(), comp.foeParty.end(), [&](const GbaCompanion::Mon& m) { return m.personality == foeSeen[0]; })) foeSeen.clear();
            if (std::find(foeSeen.begin(), foeSeen.end(), comp.foe.personality) == foeSeen.end()) foeSeen.push_back(comp.foe.personality);
        }
        fprintf(stderr, "[gba] companion: %d in party, map %d.%d, %d pockets\n", (int)comp.party.size(), comp.mapGroup, comp.mapNum, (int)comp.pockets.size());
    }
    std::vector<uint32_t> foeSeen;
    std::vector<int> battleRowTeam;              // (BATTLE INFO's rows: the foe's team slot of each, -2 unseen, -1 not a team row)
    std::vector<GbaDex::Picture> foeTeamPics;   // (personalities of the foe's team seen out this battle; the first one out first)
    GbaGlass::MonCard MonCardFor(int i)
    {
        return MonCardOf(comp.party[i], i < (int)compPics.size() && compPics[i].Ready() ? &compPics[i] : nullptr);
    }
    GbaGlass::MonCard MonCardOf(const GbaCompanion::Mon& m, const GbaDex::Picture* pic)
    {
        GbaGlass::MonCard c;
        const GbaDex::Stats st = dex.Get(m.species);
        std::string nick = GbaDex::Text((const uint8_t*)m.nickname.data(), m.nickname.size());
        while (!nick.empty() && nick.back() == ' ') nick.pop_back();
        c.name = m.egg ? std::string("Egg") : DexTitle(nick.empty() ? dex.Name(m.species) : nick);
        c.item = m.item ? DexTitle(dex.ItemName(m.item)) : std::string();
        static const char* const natures[25] = { "Hardy", "Lonely", "Brave", "Adamant", "Naughty", "Bold", "Docile", "Relaxed", "Impish", "Lax", "Timid", "Hasty", "Serious", "Jolly",
                                                 "Naive", "Modest", "Mild", "Quiet", "Bashful", "Rash", "Calm", "Gentle", "Sassy", "Careful", "Quirky" };
        c.nature = natures[m.personality % 25];
        const int ability = m.abilityBit && st.ability2 ? st.ability2 : st.ability1;
        if (ability && dex.Readable(dex.Ability(ability))) c.ability = DexTitle(dex.Ability(ability));
        c.level = m.level; c.hp = m.hp; c.maxHp = m.maxHp; c.egg = m.egg;
        // (Status as the DS cards take it: sleep turns in the low bits, then poison, burn, freeze,
        // paralysis, bad poison, as Generation 3 keeps it too.)
        c.status = (int)m.status;
        c.type1 = st.type1; c.type2 = st.type2;
        for (int k = 0; k < 6; k++) { c.stats[k] = m.stats[k]; c.iv[k] = m.iv[k]; c.ev[k] = m.ev[k]; }
        for (int k = 0; k < 4; k++)
        {
            if (!m.moves[k]) continue;
            const int base = dex.MovePp(m.moves[k]), bonus = m.ppBonuses >> (k * 2) & 3;
            c.moves.push_back({ DexTitle(dex.Move(m.moves[k])), dex.MoveType(m.moves[k]), m.pp[k], base + base * bonus / 5 });
        }
        if (pic && !m.egg) { c.icon = pic->pixels; c.iconW = pic->width; c.iconH = pic->height; }
        return c;
    }
    std::vector<std::pair<std::string, std::string>> BagRows()
    {
        std::vector<std::pair<std::string, std::string>> rows;
        bagRowIds.clear();
        for (const auto& p : comp.pockets)
        {
            if (p.second.empty()) continue;
            rows.push_back({ std::string("#") + p.first, "" });
            bagRowIds.push_back(0);
            for (const auto& it : p.second)
            {
                const std::string name = dex.ItemName(it.id);
                rows.push_back({ name.empty() ? "ITEM " + std::to_string(it.id) : DexTitle(name), "x" + std::to_string(it.quantity) });
                bagRowIds.push_back(it.id);
            }
        }
        if (rows.empty()) { rows.push_back({ "Your bag is empty", "" }); bagRowIds.push_back(0); }
        return rows;
    }
    std::vector<std::pair<std::string, std::string>> PlaceRows()
    {
        std::vector<std::pair<std::string, std::string>> rows;
        const std::string place = dex.PlaceOf(comp.mapGroup, comp.mapNum);
        rows.push_back({ "#WHERE YOU ARE", "" });
        rows.push_back({ place.empty() ? std::string("Not known") : DexTitle(place), "" });
        const auto wild = dex.WildAt(comp.mapGroup, comp.mapNum);
        rows.push_back({ "#WILD POKEMON HERE", "" });
        if (wild.empty()) rows.push_back({ "None", "" });
        for (const auto& w : wild)
        {
            const int n = dex.Dex(w.species);
            const bool caught = n > 0 && n < (int)comp.caught.size() && comp.caught[n], seen = n > 0 && n < (int)comp.seen.size() && comp.seen[n];
            // (One not yet seen keeps its name to itself, as the DS games' list.)
            const std::string name = seen || comp.seen.empty() ? DexTitle(dex.Name(w.species)) : std::string("???");
            rows.push_back({ name + "  \xC2\xB7  " + DexTitle(w.method) + (caught ? "  \xC2\xB7  Caught" : ""),
                             w.high != w.low ? "Lv " + std::to_string(w.low) + "-" + std::to_string(w.high) : "Lv " + std::to_string(w.low) });
        }
        return rows;
    }
    // BATTLE INFO: the foe's card, as a party card (its stats are its battle stats; no IVs or EVs).
    GbaGlass::MonCard FoeCard()
    {
        GbaGlass::MonCard c;
        const auto& f = comp.foe;
        std::string nick = GbaDex::Text((const uint8_t*)f.nickname.data(), f.nickname.size());
        while (!nick.empty() && nick.back() == ' ') nick.pop_back();
        c.name = DexTitle(nick.empty() ? dex.Name(f.species) : nick);
        c.level = f.level; c.hp = f.hp; c.maxHp = f.maxHp; c.status = (int)f.status;
        c.type1 = f.type1; c.type2 = f.type2;
        if (f.ability && dex.Readable(dex.Ability(f.ability))) c.ability = DexTitle(dex.Ability(f.ability));
        if (f.item) c.item = DexTitle(dex.ItemName(f.item));
        static const char* const natures[25] = { "Hardy", "Lonely", "Brave", "Adamant", "Naughty", "Bold", "Docile", "Relaxed", "Impish", "Lax", "Timid", "Hasty", "Serious", "Jolly",
                                                 "Naive", "Modest", "Mild", "Quiet", "Bashful", "Rash", "Calm", "Gentle", "Sassy", "Careful", "Quirky" };
        c.nature = natures[f.personality % 25];
        c.stats[0] = f.maxHp;
        for (int i = 0; i < 5; i++) c.stats[i + 1] = f.stats[i];
        for (int i = 0; i < 6; i++) c.iv[i] = f.iv[i];
        for (int k = 0; k < 4; k++)
            if (f.moves[k]) c.moves.push_back({ DexTitle(dex.Move(f.moves[k])), dex.MoveType(f.moves[k]), f.pp[k], dex.MovePp(f.moves[k]) });
        if (foePic.Ready()) { c.icon = foePic.pixels; c.iconW = foePic.width; c.iconH = foePic.height; }
        return c;
    }
    std::vector<std::pair<std::string, std::string>> BattleRows()
    {
        std::vector<std::pair<std::string, std::string>> rows;
        battleRowTeam.clear();
        if (!comp.inBattle) { rows.push_back({ "Not in a battle", "" }); return rows; }
        const auto& f = comp.foe;
        rows.push_back({ "#THE FOE", "" });
        rows.push_back({ DexTitle(dex.Name(f.species)), "Lv " + std::to_string(f.level) });
        std::string types = TypeName(f.type1);
        if (f.type2 != f.type1 && CompanionTypes::Valid(f.type2)) types += " / " + TypeName(f.type2);
        rows.push_back({ "Type", types });
        rows.push_back({ "HP", std::to_string(f.maxHp ? f.hp * 100 / f.maxHp : 0) + "%" });
        if (f.ability && dex.Readable(dex.Ability(f.ability))) rows.push_back({ "Ability", DexTitle(dex.Ability(f.ability)) });
        // (Your Pokemon out: each of its moves against the foe's types.)
        const auto& m = comp.mine;
        if (m.species)
        {
            rows.push_back({ "#YOUR " + dex.Name(m.species) + "'S MOVES", "" });
            for (int k = 0; k < 4; k++)
            {
                if (!m.moves[k]) continue;
                const int type = dex.MoveType(m.moves[k]);
                const int pct = CompanionTypes::Valid(type) ? CompanionTypes::Multiplier(type, f.type1, CompanionTypes::Valid(f.type2) ? f.type2 : f.type1) : 100;
                rows.push_back({ DexTitle(dex.Move(m.moves[k])) + "  \xC2\xB7  " + TypeName(type), DexTitle(CompanionTypes::Label(pct)) });
            }
        }
        // (What hits the foe hard: its weaknesses, as the Pokedex page's.)
        rows.push_back({ "#THE FOE TAKES", "" });
        for (int pct : { 400, 200, 50, 25, 0 })
        {
            std::string list;
            for (int t = 0; t < 18; t++)
                if (CompanionTypes::Valid(t) && CompanionTypes::Multiplier(t, f.type1, CompanionTypes::Valid(f.type2) ? f.type2 : f.type1) == pct) list += (list.empty() ? "" : ", ") + TypeName(t);
            if (!list.empty()) rows.push_back({ pct == 400 ? "4x damage" : pct == 200 ? "2x damage" : pct == 50 ? "Half damage" : pct == 25 ? "Quarter damage" : "No damage", list });
        }
        // (A trainer's whole team, in order, as PokeDaisy's FOE TEAM: the ones seen out, fainted or hurt by name, the
        // rest hidden until they come out.)
        if (comp.foeParty.size() > 1)
        {
            int left = 0;
            for (const auto& t : comp.foeParty) left += t.hp > 0;
            rows.push_back({ "#THE FOE'S TEAM: " + std::to_string(left) + " OF " + std::to_string(comp.foeParty.size()) + " LEFT", "" });
            for (size_t k = 0; k < comp.foeParty.size(); k++)
            {
                const auto& t = comp.foeParty[k];
                const bool out = t.personality == f.personality;
                const bool seen = out || t.hp < t.maxHp || std::find(foeSeen.begin(), foeSeen.end(), t.personality) != foeSeen.end();
                battleRowTeam.resize(rows.size(), -1);
                battleRowTeam.push_back(!seen ? -2 : out ? -1 : (int)k);   // (its own card beside it; none while unseen)
                if (!seen) { rows.push_back({ "? ? ?", "Not seen yet" }); continue; }
                const int hp = out ? f.hp : t.hp, maxHp = out ? f.maxHp : t.maxHp;
                rows.push_back({ DexTitle(dex.Name(t.species)) + "  Lv " + std::to_string(out ? f.level : t.level),
                                 hp <= 0 ? std::string("Fainted") : (out ? "Out now, " : "") + std::to_string(maxHp ? std::max(1, hp * 100 / maxHp) : 0) + "%" });
            }
        }
        return rows;
    }
    // COMPANION's pages: A on a link opens it; B goes back a page.
    void CompanionInput(const MenuIn& in)
    {
        if (in.menu) { Resume(); return; }
        if (pausePage == P_GUIDE && in.accept && pause.selected >= 0 && pause.selected < (int)guideShown.size())
        {
            guideShown[pause.selected] = !guideShown[pause.selected];
            const int sel = pause.selected;
            BuildPause();
            pause.selected = sel;
            return;
        }
        if (pausePage == P_FLY && in.accept && pause.selected >= 0 && pause.selected < (int)pause.items.size() && pause.items[pause.selected].label == "FREE FLIGHT")
        {
            flyStage = 10;
            flyWait = 0;
            flyHgssWait = 0;
            Resume();
            return;
        }
        if (pausePage == P_ITEM || pausePage == P_ITEMMON || pausePage == P_ITEMMOVE || (pausePage == P_BAG && in.accept && items.Supported()) ||
            (pausePage == P_PARTYMON && in.accept && pause.selected >= 0 && pause.selected < (int)pause.items.size() && pause.items[pause.selected].label == "TAKE ITEM"))
        {
            if (ItemsInput(in)) return;
        }
        if (pausePage == P_BOXES || pausePage == P_BOXMON || pausePage == P_PARTYMON || (pausePage == P_PARTY && in.accept && (boxes.Supported() || items.Supported())))
        {
            if (BoxesInput(in)) return;
        }
        if (pausePage == P_FLY && in.accept && pause.selected >= 0 && pause.selected < (int)pause.items.size() && pause.items[pause.selected].label == "FLY ON")
        {
            if (followState == 0) FollowFind();
            GoPage(P_FLYMON, ("MON " + std::to_string(FlyMonChosen())).c_str());
            return;
        }
        if (pausePage == P_FLYMON)
        {
            if (in.back) { GoPage(P_FLY, "FLY ON"); return; }
            if (in.accept && pause.selected >= 0 && pause.selected < (int)pause.items.size())
            {
                const std::string& l = pause.items[pause.selected].label;
                if (l == "BACK") { GoPage(P_FLY, "FLY ON"); return; }
                if (l.rfind("MON ", 0) == 0)
                {
                    set.values["fly_on"] = "any";
                    set.values["fly_on_mon"] = l.substr(4);
                    set.Save();
                    GoPage(P_FLY, "FLY ON");
                }
            }
            return;
        }
        if (pausePage == P_FLY && in.accept && pause.selected >= 0 && pause.selected - (flightRec ? 2 : 0) < (int)flyT.towns.size() && pause.selected - (flightRec ? 2 : 0) >= 0)
        {
            flySec = flyT.towns[pause.selected - (flightRec ? 2 : 0)];
            flyStage = 1;
            flyWait = 0;
            Resume();
            return;
        }
        if (pausePage != P_COMP)
        {
            if (in.back || (in.accept && pause.selected >= 0 && pause.selected < (int)pause.items.size() && pause.items[pause.selected].label == "BACK"))
                GoPage(P_COMP, pausePage == P_PARTY ? "PARTY" : pausePage == P_BAG ? "BAG" : pausePage == P_PLACE ? "LOCATION" : pausePage == P_BATTLE ? "BATTLE INFO" : pausePage == P_FLY ? "FLY" : pausePage == P_BOXES ? "BOXES" : "GUIDE");
            return;
        }
        if (in.back) { GoPage(P_ROOT, "COMPANION"); return; }
        if (!in.accept || pause.selected < 0 || pause.selected >= (int)pause.items.size()) return;
        const std::string item = pause.items[pause.selected].label;
        if (item == "PARTY") GoPage(P_PARTY, "");
        else if (item == "BOXES") { ReadCompanion(); GoPage(P_BOXES, ""); }
        else if (item == "BAG") GoPage(P_BAG, "");
        else if (item == "LOCATION") GoPage(P_PLACE, "");
        else if (item == "BATTLE INFO") { if (comp.inBattle) GoPage(P_BATTLE, ""); }
        else if (item == "GUIDE") { if (!guideShown.empty()) GoPage(P_GUIDE, ""); }
        else if (item == "FLY")
        {
            if (!dexTried) { dexTried = true; dex.Load(dexRom); }
            flyT = fly.Now();
            if (set.Get("fly_on") == "pokemon" && followState == 0) FollowFind();
            if (flyT.result != GbaFly::OK) Toast(GbaFly::Why(flyT.result), 3000);
            else if (flyT.towns.empty()) Toast("NO TOWNS TO FLY TO YET", 3000);
            else GoPage(P_FLY, "");
        }
        else if (item == "BACK") GoPage(P_ROOT, "COMPANION");
    }
    // POKEDEX's pages: A on a row opens its entry; B goes back (the entry to the list, on its row).
    void DexInput(const MenuIn& in)
    {
        if (in.menu) { Resume(); return; }
        if (pausePage == P_DEXENTRY)
        {
            if (in.back)
            {
                pausePage = P_DEX;
                BuildPause();
                for (size_t i = 0; i < dexRows.size(); i++) if (dexRows[i] == dexSpecies) pause.selected = (int)i;
            }
            return;
        }
        if (in.back) { GoPage(P_ROOT, "POKEDEX"); return; }
        if (!in.accept || pause.selected < 0 || pause.selected >= (int)dexRows.size()) return;
        dexSpecies = dexRows[pause.selected];
        GoPage(P_DEXENTRY, "");
    }
    // Another page of the pause menu, on the row named.
    void GoPage(PausePage page, const char* row)
    {
        pausePage = page;
        BuildPause();
        pause.selected = 0;
        for (size_t j = 0; j < pause.items.size(); j++)
            if (pause.items[j].label == row) pause.selected = (int)j;
    }
    void ResetGame()
    {
        core->reset(core);
        pk_wide_reset();
        camera.Reset();
#ifdef GBAW_CHEATS
        cheats.Reset();
#endif
        if (raReady && ra && ra->reset) ra->reset(ra->ud);
        Resume();
    }
    // SETTINGS (links to DISPLAY, SOUND, CONTROLS, and RESET GAME) and the two questions.
    void HubPageInput(const MenuIn& in)
    {
        if (in.menu)
        {
            Resume();
            return;
        }
        const int n = (int)pause.items.size();
        const std::string item = pause.selected >= 0 && pause.selected < n ? pause.items[pause.selected].label : std::string();
        if (pausePage == P_SAVE || pausePage == P_LOAD)
        {
            const bool saving = pausePage == P_SAVE;
            if (in.back || (in.accept && item == "BACK")) GoPage(P_ROOT, saving ? "SAVE STATE" : "LOAD STATE");
            else if (in.accept && pause.selected < StateSlots)
            {
                if (!saving && StateDate(pause.selected).empty()) return;
                if (saving) SaveState(pause.selected);
                else LoadState(pause.selected);
                Resume();
            }
            return;
        }
        if (pausePage == P_SETTINGS)
        {
            if (in.back || (in.accept && item == "BACK")) GoPage(P_ROOT, "SETTINGS");
            else if (!in.accept) return;
            else if (item == "DISPLAY" || item == "SOUND" || item == "CONTROLS" || item == "GAMEPLAY")
            {
                pausePage = item == "DISPLAY" ? P_DISPLAY : item == "SOUND" ? P_SOUND : item == "GAMEPLAY" ? P_GAMEPLAY : P_CONTROLS;
                BuildPause();
                pause.selected = 0;
            }
            else if (item == "RESET GAME") GoPage(P_RESET, "CANCEL");
            return;
        }
        const bool quitting = pausePage == P_QUIT;
        if (in.back || (in.accept && item == "CANCEL"))
        {
            if (quitting) GoPage(P_ROOT, "QUIT");
            else GoPage(P_SETTINGS, "RESET GAME");
            return;
        }
        if (!in.accept) return;
        if (quitting) Stop();
        else ResetGame();
    }

    static const char* SettingIcon(const char* key, int& icon)
    {
        using namespace GbaGlass;
        static const struct { const char* key; int icon; } icons[] = {
            { "widescreen", IC_RATIO }, { "fullscreen", IC_FULLSCREEN }, { "scaling", IC_EXPAND }, { "hd_filter", IC_SPARKLE },
            { "lcd", IC_GRID }, { "colour", IC_MONITOR }, { "zoom", IC_EXPAND }, { "zoom_text", IC_EXPAND }, { "show_fps", IC_GAUGE }, { "volume", IC_SPEAKER }, { "ff_mute", IC_FAST },
            { "gba_start", IC_PAD }, { "buttons", IC_PAD }, { "ff_speed", IC_FAST }, { "fast_button", IC_FAST }, { "fast_lock", IC_FAST },
            { "instant_text", IC_INFO }, { "reusable_tms", IC_CHIP }, { "run_indoors", IC_FAST }, { "wild_encounters", IC_GLOBE }, { "shiny_odds", IC_SPARKLE }, { "battle_speed", IC_FAST }, { "text_speed", IC_FAST }, { "bike_speed", IC_FAST }, { "surf_speed", IC_FAST }, { "day_night", IC_SPARKLE }, { "follower", IC_POKEBALL }, { "nav_style", IC_GRID } };
        icon = IC_GEAR;
        for (const auto& i : icons)
            if (!strcmp(i.key, key)) icon = i.icon;
        return key;
    }
    std::string ShownOf(const char* key) const
    {
        for (const Setting& s : SettingsList())
            if (!strcmp(s.key, key)) return set.Shown(s);
        return std::string();
    }

    // The page of the pause menu that is open, as the DS games' menu draws it (gba_glass.h): the
    // first page as tiles, the others as settings pages with a preview, the questions as cards.
    void BuildGlass()
    {
        using namespace GbaGlass;
        Page& g = glass;
        g.items.clear();
        g.dex.clear();
        g.mons.clear();
        g.place.clear(); g.map.clear(); g.mapW = g.mapH = 0; g.markerX = g.markerY = -1;
        glassRow.clear();
        g.style = PAGE;
        g.icon = -1;
        g.tall = false;
        g.slotGrid = false;
        g.title.clear();
        g.note.clear();
        g.headerNote.clear();
#ifdef GBAW_PHONE
        g.touchRows = 1.5f; // (rows big enough for a finger)
#endif
        auto put = [&](const Item& it, int row) {
            g.items.push_back(it);
            glassRow.push_back(row);
        };
        auto page = [&](const char* title, int icon) {
            g.title = title;
            g.icon = icon;
            Item b;
            b.label = "BACK";
            b.kind = BACK;
            put(b, -1);
        };
        auto link = [&](int row, const char* label, int icon, const std::string& value, const char* sub, const char* desc) {
            Item it;
            it.label = label;
            it.kind = LINK;
            it.icon = icon;
            it.value = value;
            it.info = true;
            it.sub = sub;
            it.desc = desc;
            put(it, row);
        };
        const int n = (int)pause.items.size();
        g.hint = Hint("ENTER: SELECT   ESC: BACK", "A: SELECT   B: BACK", "TAP: CHOOSE   OUTSIDE: BACK");
        if (pausePage == P_ROOT)
        {
            g.style = TILES;
            g.title = game.name;
            g.hint = Hint("ENTER: SELECT   ESC: RESUME", "A: SELECT   B: RESUME", "TAP: CHOOSE   OUTSIDE: RESUME");
            for (int i = 0; i < n; i++)
            {
                const std::string& l = pause.items[i].label;
                Item t;
                t.label = l;
                t.value = pause.items[i].value;
                if (l == "RESUME") t.icon = IC_PLAY;
                else if (l == "SAVE STATE") t.label = "SAVE", t.icon = IC_SAVE;
                else if (l == "LOAD STATE") t.label = "LOAD", t.icon = IC_LOAD;
                else if (l == "CHEATS")
                {
                    t.icon = IC_CHEATS;
#ifdef GBAW_CHEATS
                    t.disabled = !cheats.Active();
#else
                    t.disabled = true;
#endif
                }
                else if (l == "FAST FORWARD") t.icon = IC_FAST;
                else if (l == "RETROACHIEVEMENTS") t.label = "ACHIEVEMENTS", t.icon = IC_TROPHY, t.disabled = !raReady;
                else if (l == "POKEDEX") t.icon = IC_POKEBALL;
                else if (l == "COMPANION") t.icon = IC_PEOPLE, t.disabled = !compConfig;
                else if (l == "SETTINGS") t.icon = IC_GEAR;
                else if (l == "QUIT") t.label = "QUIT GAME", t.icon = IC_POWER;
                put(t, i);
            }
        }
        else if (pausePage == P_SETTINGS)
        {
            page("SETTINGS", IC_GEAR);
            g.tall = true;
            for (int i = 0; i < n; i++)
            {
                const std::string& l = pause.items[i].label;
                if (l == "DISPLAY")
                    link(i, "DISPLAY", IC_MONITOR, ShownOf("widescreen"), "Aspect ratio, scaling, filters",
                         "The shape of the picture, how the GBA's small picture is enlarged, the HD filter, the LCD effect, colour correction and the FPS counter.");
                else if (l == "SOUND")
                    link(i, "SOUND", IC_SPEAKER, ShownOf("volume"), "Volume, fast forward sound", "The game's volume, and whether its sound carries on while fast forwarding.");
                else if (l == "CONTROLS")
                    link(i, "CONTROLS", IC_PAD, ShownOf("ff_speed"), "START button, fast forward",
                         "What a controller's START does, how the buttons are named, and fast forward's speed and button.");
                else if (l == "GAMEPLAY")
                    link(i, "GAMEPLAY", IC_SPARKLE, "", "Instant text, reusable TMs, wild encounters",
                         "Changes to how the game plays, made in the running game only: instant text, TMs kept after use, running indoors and how many wild Pok\xC3\xA9" "mon turn up.");
                else if (l == "RESET GAME")
                    link(i, "RESET GAME", IC_REWIND, "", "Start again from the title screen", "Restarts the game. Progress since your last save in the game will be lost.");
            }
        }
        else if (pausePage == P_SAVE || pausePage == P_LOAD)
        {
            const bool saving = pausePage == P_SAVE;
            // (As the DS games' SAVE and LOAD: the ten slots as cards, each its picture and when it was saved.)
            page(saving ? "SAVE" : "LOAD", saving ? IC_SAVE : IC_LOAD);
            g.slotGrid = true;
            for (int i = 0; i < n && i < StateSlots; i++)
            {
                Item it;
                it.label = pause.items[i].label;
                it.value = pause.items[i].value;
                it.kind = SLOT;
                it.disabled = !saving && it.value.empty();
                it.thumb = slotThumb[i];
                it.thumbW = slotThumbW[i];
                it.thumbH = slotThumbH[i];
                put(it, i);
            }
        }
        else if (pausePage == P_QUIT || pausePage == P_RESET)
        {
            const bool quitting = pausePage == P_QUIT;
            g.style = DIALOG;
            g.icon = quitting ? IC_POWER : IC_REWIND;
            g.title = quitting ? "QUIT GAME?" : "RESET GAME?";
            g.note = "Progress since your last save in the game will be lost.";
            for (int i = 0; i < n; i++)
            {
                Item it;
                it.label = pause.items[i].label;
                put(it, i);
            }
        }
        else if (IsSettingsPage())
        {
            page(SettingsPageName(),
                 pausePage == P_DISPLAY ? IC_MONITOR : pausePage == P_SOUND ? IC_SPEAKER : pausePage == P_GAMEPLAY ? IC_SPARKLE : IC_PAD);
            for (int i = 0; i < n; i++)
            {
                if (pause.items[i].label == "BACK") continue;
                Item it;
                it.label = pause.items[i].label;
                it.value = pause.items[i].value;
                if (i < (int)pageSettings.size())
                {
                    const Setting& s = *pageSettings[i];
                    SettingIcon(s.key, it.icon);
                    it.desc = s.text;
                    for (size_t j = 0; j < s.values.size(); j++)
                    {
                        it.choices.push_back(s.values[j].second);
                        if (s.values[j].second == it.value) it.choice = (int)j;
                    }
                    const bool onOff = s.values.size() == 2 &&
                        ((s.values[0].second == "OFF" && s.values[1].second == "ON") || (s.values[0].second == "ON" && s.values[1].second == "OFF"));
                    if (onOff)
                    {
                        // (Off is the left pill and On the right, whichever the setting keeps first.)
                        it.kind = TOGGLE;
                        it.choices = { "OFF", "ON" };
                        it.choice = it.value == "ON" ? 1 : 0;
                    }
                    else
                    {
                        it.kind = PICKER;
                        if (!strcmp(s.key, "volume")) it.slider = std::max(0, it.choice);
                    }
                }
                else
                {
                    // (TOUCH CONTROLS, on a phone: opens the layout's editor.)
                    it.kind = LINK;
                    it.icon = IC_PAD;
                    it.info = true;
                    it.desc = "Move and resize the buttons on the screen.";
                }
                put(it, i);
            }
        }
        else if (pausePage == P_COMP)
        {
            // (A settings page of links, as the DS games' COMPANION: each with what the game holds now.)
            page("COMPANION", IC_PEOPLE);
            g.tall = true;
            std::string place = dex.PlaceOf(comp.mapGroup, comp.mapNum);
            int itemCount = 0;
            for (const auto& p : comp.pockets) itemCount += (int)p.second.size();
            for (int i = 0; i < n; i++)
            {
                const std::string& l = pause.items[i].label;
                if (l == "PARTY") link(i, "PARTY", IC_PEOPLE, std::to_string(comp.party.size()), "Your team as it is now", "Your team as it is now. Choose one for its stats, nature, ability and moves.");
                else if (l == "BAG") link(i, "BAG", IC_LOAD, std::to_string(itemCount) + " KINDS", "Every pocket of your bag", "Every pocket of your bag and how many of each item. Choose one to use it, or to give it to a Pok\xC3\xA9" "mon to hold.");
                else if (l == "LOCATION") link(i, "LOCATION", IC_GLOBE, place.empty() ? std::string("-") : DexTitle(place), "Where you are and what lives there", "Where you are, and the wild Pok\xC3\xA9" "mon of this place: how you find each and its levels.");
                else if (l == "BATTLE INFO")
                {
                    link(i, "BATTLE INFO", IC_FAST, comp.inBattle ? std::string("IN BATTLE") : std::string("-"), "The foe and your moves against it",
                         "In a battle: the foe's types, ability and HP, and how hard each of your Pok\xC3\xA9" "mon's moves hits it.");
                    g.items.back().disabled = !comp.inBattle;
                    if (!comp.inBattle) g.items.back().tip = "Open it from the pause menu during a battle.";
                }
                else if (l == "BOXES")
                    link(i, "BOXES", IC_LOAD, "", "Your PC's boxes, live", "Every box in your PC as it is now. Choose a Pok\xC3\xA9" "mon to take it into your party or swap it with one there; in PARTY, choose one to put it in a box. As the game's own PC does it, on the field.");
                else if (l == "FLY")
                    link(i, "FLY", IC_GLOBE, "", "Fly to a town you have been to", "Fly to a town you have been to, as the move does: the game plays its own Fly. A Pok\xC3\xA9" "mon in your party must know Fly, and you need its badge.");
                else if (l == "GUIDE")
                {
                    const size_t pages = GbaGuides::For(gameCode.c_str()).size();
                    link(i, "GUIDE", IC_INFO, std::to_string(pages) + " PAGES", "Where things are and how things work",
                         "Answers to common questions: where the key items and HMs are, which badge unlocks which HM, and how the game works. Each answer stays hidden until you choose it.");
                    g.items.back().disabled = !pages;
                }
            }
        }
        else if (pausePage == P_PARTY)
        {
            // (Each one a tall row with its picture; its card beside the list, as the DS games' PARTY.)
            page("PARTY", IC_PEOPLE);
            g.tall = true;
            for (size_t i = 0; i < comp.party.size(); i++) g.mons.push_back(MonCardFor((int)i));
            for (int i = 0; i < n; i++)
            {
                if (pause.items[i].label == "BACK" || i >= (int)g.mons.size()) continue;
                Item it;
                it.label = g.mons[i].name;
                it.value = g.mons[i].egg ? "EGG" : "LV " + std::to_string(g.mons[i].level);
                it.sub = g.mons[i].egg ? "" : std::to_string(g.mons[i].hp) + " / " + std::to_string(g.mons[i].maxHp) + " HP";
                it.info = true;
                it.monCard = i;
                put(it, i);
            }
        }
        else if (pausePage == P_BOXES || pausePage == P_BOXMON || pausePage == P_PARTYMON)
        {
            BoxesGlass(g, n, page, put);
        }
        else if (pausePage == P_ITEM || pausePage == P_ITEMMON || pausePage == P_ITEMMOVE)
        {
            ItemsGlass(g, n, page, put);
        }
        else if (pausePage == P_FLY)
        {
            // (The towns the game's own Fly map would list now.)
            page("FLY", IC_GLOBE);
            const int first = flightRec ? 2 : 0;
            if (flightRec && n > 1)
            {
                Item it;
                it.label = "Free Flight";
                it.kind = LINK;
                it.icon = IC_SPARKLE;
                it.desc = "Fly over the map yourself: the D-pad steers, A held goes faster, B lands on the nearest place you can stand.";
                put(it, 0);
                Item on;
                on.label = "Fly On";
                on.value = FlyOnLabel();
                on.kind = LINK;
                on.icon = IC_POKEBALL;
                on.desc = "Choose any Pok\xC3\xA9" "mon to carry you in Free Flight. Its sprite comes from your HeartGold or SoulSilver; Emerald's own for Lugia, Ho-Oh, Rayquaza and the rest of its legendaries.";
                put(on, 1);
            }
            for (int i = first; i < n && i - first < (int)flyT.towns.size(); i++)
            {
                Item it;
                const int sec = flyT.towns[i - first];
                const std::string name = dex.Ready() ? dex.MapsecName(sec) : std::string();
                it.label = name.empty() ? "TOWN " + std::to_string(sec) : DexTitle(name);
                it.kind = LINK;
                it.icon = IC_GLOBE;
                it.desc = "Fly here.";
                put(it, i);
            }
        }
        else if (pausePage == P_FLYMON)
        {
            page("FLY ON", IC_POKEBALL);
            const int chosen = FlyMonChosen();
            const bool any = set.Get("fly_on") == "any";
            // (The row's Pokemon in the preview, as it will carry you.)
            if (pause.selected >= 0 && pause.selected < n && pause.items[pause.selected].label.rfind("MON ", 0) == 0)
            {
                const int nat = std::atoi(pause.items[pause.selected].label.c_str() + 4);
                const MountPic& mp = MountPicture(nat);
                GbaGlass::DexCard d;
                d.id = nat;
                d.name = MonName(nat);
                d.desc = mp.px.empty() ? (followState == 1 ? "Looking for HeartGold or SoulSilver..." : "No sprite for this one: you would fly on the bird.")
                                       : mp.emerald ? "Fly on this one. Its sprite is this game's own." : "Fly on this one. Its sprite is HeartGold's.";
                d.pic = mp.px; d.picW = mp.w; d.picH = mp.h;
                g.dex.push_back(std::move(d));
            }
            for (int i = 0; i < n; i++)
            {
                const std::string& l = pause.items[i].label;
                if (l.rfind("MON ", 0) != 0) continue;
                const int nat = std::atoi(l.c_str() + 4);
                Item it;
                const std::string name = MonName(nat);
                it.label = std::to_string(nat) + "  " + (name.empty() ? std::string("?") : name);
                it.value = any && nat == chosen ? "CHOSEN" : "";
                it.kind = LINK;
                it.icon = IC_POKEBALL;
                it.desc = "Fly on this one.";
                if (i == pause.selected && !g.dex.empty()) { it.dexCard = 0; it.desc.clear(); }
                put(it, i);
            }
        }
        else if (pausePage == P_GUIDE)
        {
            // (Each question a row; its hint in the preview until it is chosen, then its answer.)
            page("GUIDE", IC_INFO);
            const auto& gd = GbaGuides::For(gameCode.c_str());
            for (int i = 0; i < n && i < (int)gd.size(); i++)
            {
                Item it;
                it.label = gd[i].title;
                const bool shown = i < (int)guideShown.size() && guideShown[i];
                it.value = shown ? "SHOWN" : "";
                it.kind = LINK;
                it.icon = IC_INFO;
                it.desc = shown ? gd[i].answer : (gd[i].hint[0] ? std::string("Hint: ") + gd[i].hint : std::string("A to show the answer."));
                if (!shown) it.tip = "A to show the answer.";
                put(it, i);
            }
        }
        else if (pausePage == P_BAG || pausePage == P_PLACE || pausePage == P_BATTLE)
        {
            page(pausePage == P_BAG ? "BAG" : pausePage == P_PLACE ? "LOCATION" : "BATTLE INFO", pausePage == P_BAG ? IC_LOAD : pausePage == P_PLACE ? IC_GLOBE : IC_FAST);
            std::vector<int> teamCard(comp.foeParty.size(), -1);
            if (pausePage == P_BATTLE && comp.inBattle)
            {
                g.mons.push_back(FoeCard());
                for (int v : battleRowTeam)
                    if (v >= 0 && v < (int)comp.foeParty.size())
                    {
                        g.mons.push_back(MonCardOf(comp.foeParty[v], v < (int)foeTeamPics.size() && foeTeamPics[v].Ready() ? &foeTeamPics[v] : nullptr));
                        teamCard[v] = (int)g.mons.size() - 1;
                    }
            }
            // (LOCATION: the place and the region's map with you on it beside every row.)
            GbaDex::Data::RegionPicture region;
            if (pausePage == P_PLACE)
            {
                region = dex.Region(comp.mapGroup, comp.mapNum);
                g.place = DexTitle(dex.PlaceOf(comp.mapGroup, comp.mapNum));
                if (region.ok) { g.map = std::move(region.px); g.mapW = region.w; g.mapH = region.h; g.markerX = region.markerX; g.markerY = region.markerY; }
            }
            if (pausePage == P_BAG && comp.money >= 0) g.headerNote = "MONEY " + std::to_string(comp.money);
            for (int i = 0; i < n; i++)
            {
                const std::string& l = pause.items[i].label;
                if (l == "BACK") continue;
                Item it;
                if (!l.empty() && l[0] == '#') { it.label = l.substr(1); it.kind = SECTION; it.icon = pausePage == P_BAG ? IC_LOAD : pausePage == P_PLACE ? IC_GLOBE : IC_FAST; put(it, -1); continue; }
                it.label = l;
                it.value = pause.items[i].value;
                it.info = true;
                if (pausePage == P_BAG && i < (int)bagRowIds.size() && bagRowIds[i] && items.Supported() && (items.CanUse(bagRowIds[i]) || items.CanGive(bagRowIds[i]) || items.CanTeach(bagRowIds[i])))
                {
                    const int id = bagRowIds[i];
                    it.kind = LINK;
                    it.info = false;
                    it.desc = items.CanTeach(id) ? "Teach it to a Pok\xC3\xA9" "mon, on the game's own TM screen." : items.CanUse(id) && items.CanGive(id) ? "Use it, or give it to a Pok\xC3\xA9" "mon to hold." : items.CanUse(id) ? "Use it." : "Give it to a Pok\xC3\xA9" "mon to hold.";
                }
                if (!g.mons.empty()) it.monCard = 0; // (BATTLE INFO: the foe's card beside every row)
                if (pausePage == P_BATTLE && i < (int)battleRowTeam.size() && battleRowTeam[i] != -1)
                    it.monCard = battleRowTeam[i] >= 0 ? teamCard[battleRowTeam[i]] : -1;   // (the foe's team: each its own; unseen none)
                if (pausePage == P_PLACE) it.placeCard = true;
                put(it, i);
            }
        }
        else if (pausePage == P_DEX || pausePage == P_DEXENTRY)
        {
            // (A settings page: the rows, and the chosen Pokemon's card beside them, as the DS
            // games' POKEDEX.)
            const bool entry = pausePage == P_DEXENTRY;
            page(entry ? "POKEDEX ENTRY" : "POKEDEX", IC_INFO);
            const int chosen = entry ? dexSpecies : (pause.selected >= 0 && pause.selected < (int)dexRows.size() ? dexRows[pause.selected] : 0);
            if (chosen > 0)
            {
                g.dex.push_back(DexCardFor(chosen));
                if (entry) { char n[16]; snprintf(n, sizeof n, "#%03d", dex.Dex(chosen)); g.headerNote = n; }
            }
            std::string section;
            for (int i = 0; i < n; i++)
            {
                const std::string& l = pause.items[i].label;
                if (l == "BACK") continue;
                Item it;
                it.label = l;
                it.value = pause.items[i].value;
                it.info = entry;
                it.kind = entry ? PLAIN : LINK;
                // (The entry's sections: rows whose label starts with "#" head one.)
                if (entry && !l.empty() && l[0] == '#')
                {
                    Item h;
                    h.label = l.substr(1);
                    h.kind = SECTION;
                    h.icon = it.value == "stats" ? IC_GAUGE : it.value == "abilities" ? IC_SPARKLE : it.value == "details" ? IC_NOTE : it.value == "attacks" ? IC_FAST :
                             it.value == "where" ? IC_GLOBE : it.value == "level" ? IC_PLAY : it.value == "machines" ? IC_CHIP : it.value == "egg" ? IC_POKEBALL : IC_INFO;
                    put(h, -1);
                    continue;
                }
                if (!g.dex.empty() && (entry || (i < (int)dexRows.size() && dexRows[i] == chosen))) it.dexCard = 0;
                put(it, i);
            }
        }
#ifdef GBAW_CHEATS
        else if (pausePage == P_CHEATS)
        {
            page("CHEATS", IC_CHEATS);
            if (cheats.OnCount()) g.headerNote = std::to_string(cheats.OnCount()) + " ON";
            for (int i = 0; i < n && i < GbaCheats::COUNT; i++)
            {
                Item it;
                it.label = pause.items[i].label;
                it.kind = TOGGLE;
                it.icon = IC_CHEATS;
                it.choices = { "OFF", "ON" };
                it.choice = cheats.Has(i) && cheats.On(i) ? 1 : 0;
                it.value = it.choice ? "ON" : "OFF";
                it.disabled = !cheats.Has(i);
                it.desc = cheats.Has(i) ? GbaCheats::About(i).note : "NOT FOR THIS VERSION OF THE GAME.";
                it.tip = "Achievements pause while any cheat is on.";
                put(it, i);
            }
        }
#endif
        else
        {
            // RetroAchievements' pages: the account, the list, the log in.
            page(pausePage == P_RA ? "RETROACHIEVEMENTS" : pausePage == P_LIST ? "ACHIEVEMENTS" : "LOG IN", IC_TROPHY);
            if (pausePage == P_RA)
            {
                char detail[80] = {};
                if (ra && ra->detail && ra->detail(ra->ud, detail, sizeof detail) && detail[0]) g.headerNote = detail;
            }
            for (int i = 0; i < n; i++)
            {
                const std::string& l = pause.items[i].label;
                if (l == "BACK") continue;
                Item it;
                it.label = l;
                it.value = pause.items[i].value;
                it.info = true;
                it.icon = IC_TROPHY;
                if (pausePage == P_LIST)
                {
                    it.icon = it.value == "DONE" ? IC_TROPHY : IC_INFO;
                    if (i < (int)achDesc.size()) it.desc = achDesc[i];
                }
                else if (pausePage == P_LOGIN)
                {
                    it.icon = i < 2 ? IC_INFO : IC_TROPHY;
                    if (i >= 2) it.kind = LINK;
                    it.desc = i < 2 ? "Type with the keyboard." : i == 2 ? "Log in to RetroAchievements with the name and password typed." : "Back without logging in.";
                    if (i == 1) it.tip = "The password goes to RetroAchievements once and is then forgotten.";
                }
                else if (l == "RETROACHIEVEMENTS")
                {
                    it.kind = TOGGLE;
                    it.info = false;
                    it.choices = { "OFF", "ON" };
                    it.choice = it.value == "ON" ? 1 : 0;
                    it.desc = "Earn RetroAchievements while you play this game.";
                    it.tip = "Achievements pause while any cheat is on.";
                }
                else if (pause.items[i].fixed) it.icon = IC_INFO;
                else it.kind = LINK;
                put(it, i);
            }
        }
        // The item chosen: as it was left on this page, or the one for pause's row.
        if (glassPage != pausePage)
        {
            glassPage = pausePage;
            glassSel = -1;
        }
        const int count = (int)g.items.size();
        if (glassSel < 0 || glassSel >= count)
        {
            glassSel = -1;
            for (int i = 0; i < count && glassSel < 0; i++)
                if (glassRow[i] == pause.selected) glassSel = i;
            for (int i = 0; i < count && glassSel < 0; i++)
                if (g.items[i].kind != BACK && g.items[i].kind != SECTION) glassSel = i;
            if (glassSel < 0) glassSel = 0;
        }
        g.selected = glassSel;
    }

    // FULLDEX NAV. Its apps, in fulldex_nav.h's order.
    enum { NAV_FLY, NAV_PARTY, NAV_PC, NAV_REPEL, NAV_COUNT, NAV_RESET, NAV_MAP, NAV_DEX, NAV_BAG, NAV_APPS };
    std::string NavCountFile() const { return states + "/" + game.key + ".counter"; }
    void NavLoadCount()
    {
        navCountLoaded = true;
        navCount = 0;
        navCountShown = false;
        if (FILE* f = fopen(NavCountFile().c_str(), "r")) { int c = 0, s = 0; if (fscanf(f, "%d %d", &c, &s) >= 1) { navCount = std::max(0, c); navCountShown = s != 0; } fclose(f); }
    }
    void NavSaveCount() const
    {
        const std::string tmp = NavCountFile() + ".tmp";
        if (FILE* f = fopen(tmp.c_str(), "w")) { fprintf(f, "%d %d\n", navCount, navCountShown ? 1 : 0); fclose(f); std::remove(NavCountFile().c_str()); std::rename(tmp.c_str(), NavCountFile().c_str()); }
    }
    // (The strongest Repel in the Bag, as the DS games' Nav takes: Max Repel, Super Repel, Repel; 0 none.)
    int NavRepel() const
    {
        for (int id : { 84, 83, 86 })
            for (const auto& p : comp.pockets)
                for (const auto& it : p.second)
                    if (it.id == id && it.quantity > 0) return id;
        return 0;
    }
    bool NavDim(int app) const
    {
        switch (app)
        {
        case NAV_FLY: return !fly.Supported();
        case NAV_REPEL: return !items.Supported() || !NavRepel();
        case NAV_PARTY: case NAV_PC: case NAV_MAP: case NAV_BAG: return !compConfig;
        default: return false;
        }
    }
    // Each frame the game runs: the watch opened and closed, its keys (the game gets none while it is open), the
    // test's (GBAW_TEST_NAV=<frame>,<app>[,1]: opened at that frame on that app, used 10 frames on), and the count.
    uint32_t NavTick(uint32_t keys)
    {
        if (!navCountLoaded) NavLoadCount();
        static long long testAt = -2;
        static int testSel = 0, testUse = 0;
        if (testAt == -2) { testAt = -1; if (const char* t = getenv("GBAW_TEST_NAV")) { int a = -1; sscanf(t, "%d,%d,%d", &a, &testSel, &testUse); testAt = a; } }
        if (testAt >= 0 && (long long)gameFrames == testAt && !navOpen) { navToggle = true; navSel = std::clamp(testSel, 0, NAV_APPS - 1); }
        // (ENCOUNTERS: a battle starting counts, a moment in, unless its foe has a team, a trainer's.)
        if (compConfig && compConfig->gMain)
        {
            const bool inBattle = (core->busRead8(core, compConfig->gMain + compConfig->inBattleOff) & 2) != 0;
            if (inBattle && !navBattleWas) navBattleAt = gameFrames + 90;
            navBattleWas = inBattle;
            if (navBattleAt && gameFrames >= navBattleAt)
            {
                navBattleAt = 0;
                ReadCompanion();
                if (comp.foeParty.size() <= 1) { navCount++; NavSaveCount(); }
            }
        }
        if (navToggle)
        {
            navToggle = false;
            navOpen = !navOpen;
            navResetAsk = 0;
            navKeysWas = keys;
            if (navOpen) ReadCompanion();
            else { holdKeys = true; return 0; }
        }
        if (!navOpen) return keys;
        const uint32_t down = keys & ~navKeysWas;
        navKeysWas = keys;
        auto pressed = [&](GbaKey k) { return ((down >> k) & 1) != 0; };
        int& sel = navSel;
        if (pressed(K_LEFT)) sel = sel % 3 == 0 ? sel + 2 : sel - 1;
        if (pressed(K_RIGHT)) sel = sel % 3 == 2 ? sel - 2 : sel + 1;
        if (pressed(K_UP)) sel = (sel + 6) % NAV_APPS;
        if (pressed(K_DOWN)) sel = (sel + 3) % NAV_APPS;
        if (pressed(K_SELECT) && sel == NAV_COUNT) { navCount = 0; NavSaveCount(); Toast("ENCOUNTERS SET TO 0", 1500); }
        if (pressed(K_B)) { navOpen = false; holdKeys = true; return 0; }
        if (pressed(K_A) || (testUse && testAt >= 0 && (long long)gameFrames == testAt + 10)) NavUse(sel);
        return 0;
    }
    // An app chosen: the page of the pause menu it is (the game paused there), or done at once (REPEL, RESET, ENCOUNTERS).
    void NavUse(int app)
    {
        auto open = [&](PausePage p) { if (!dexTried) { dexTried = true; dex.Load(dexRom); } navOpen = false; OpenPause(); GoPage(p, ""); };
        auto needCompanion = [&]() { if (!compConfig) Toast("COMPANION ISN'T AVAILABLE FOR THIS GAME"); else ReadCompanion(); return compConfig != nullptr; };
        switch (app)
        {
        case NAV_FLY: // (the Fly page: the towns, and Free Flight)
            if (!fly.Supported()) { Toast("FLY ISN'T AVAILABLE FOR THIS GAME YET"); return; }
            if (!dexTried) { dexTried = true; dex.Load(dexRom); }
            flyT = fly.Now();
            if (set.Get("fly_on") == "pokemon" && followState == 0) FollowFind();
            if (flyT.result != GbaFly::OK) { Toast(GbaFly::Why(flyT.result), 3000); return; }
            if (flyT.towns.empty()) { Toast("NO TOWNS TO FLY TO YET", 3000); return; }
            open(P_FLY);
            return;
        case NAV_PARTY: if (needCompanion()) open(P_PARTY); return;
        case NAV_PC: if (needCompanion()) open(P_BOXES); return; // (the boxes, as the PC has them)
        case NAV_MAP: if (needCompanion()) open(P_PLACE); return;
        case NAV_BAG: if (needCompanion()) open(P_BAG); return;
        case NAV_DEX:
            if (!dexTried) { dexTried = true; dex.Load(dexRom); }
            if (!dex.Ready() || !dex.count) { Toast("THE POKEDEX ISN'T AVAILABLE FOR THIS GAME"); return; }
            ReadCompanion();
            open(P_DEX);
            return;
        case NAV_REPEL:
        {
            if (!items.Supported()) { Toast("REPEL ISN'T AVAILABLE FOR THIS GAME YET"); return; }
            ReadCompanion();
            const int id = NavRepel();
            if (!id) { Toast("NO REPEL IN YOUR BAG"); return; }
            int steps = 0;
            const GbaItems::Result r = items.UseRepel(id, steps);
            if (r != GbaItems::USED) { Toast(GbaItems::Why(r), 2500); return; }
            Toast(ItemName(id) + ": " + std::to_string(steps) + " STEPS WITHOUT WILD POKEMON", 2500);
            navOpen = false;
            holdKeys = true;
            ReadCompanion();
            return;
        }
        case NAV_COUNT: navCountShown = !navCountShown; NavSaveCount(); return; // (ENCOUNTERS on the picture, or not)
        case NAV_RESET: // (A twice within 3 seconds)
            if (navResetAsk && SDL_GetTicks() - navResetAsk < 3000) { navOpen = false; navResetAsk = 0; ResetGame(); }
            else navResetAsk = SDL_GetTicks();
            return;
        }
    }
    // NAV STYLE (Display), as the DS games have it (GBAW_TEST_NAVLOOK=<style>: a test's).
    void NavLook(GbaGlass::Nav& n) const
    {
        const std::string v = set.Get("nav_style");
        n.style = v == "classic" ? 1 : v == "simple" ? 2 : v == "wheel" ? 3 : 0;
        if (const char* t = getenv("GBAW_TEST_NAVLOOK")) n.style = std::clamp(atoi(t), 0, 3);
    }
    // The watch over the game (or ENCOUNTERS on the picture), at the window's size.
    void DrawNav()
    {
        if (!navOpen && !navCountShown) return;
        int ow, oh;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        bool changed = false;
        const uint32_t* px = nullptr;
        if (navOpen)
        {
            GbaGlass::Nav n;
            n.centre = touchUi; // (in the middle while the touch controls are up, clear of A and B)
            NavLook(n);
            n.sel = navSel;
            n.count = navCount;
            for (int i = 0; i < NAV_APPS; i++) n.dim[i] = NavDim(i);
            if (navResetAsk && SDL_GetTicks() - navResetAsk < 3000) n.label[NAV_RESET] = "A again";
            n.hint = navSel == NAV_COUNT ? std::string("A: ") + (navCountShown ? "HIDE" : "SHOW") + " ON SCREEN   SELECT: SET TO 0   B: CLOSE"
                   : navSel == NAV_RESET ? "A TWICE: RESET THE GAME   B: CLOSE" : "A: USE   B: CLOSE";
            px = GbaGlass::NavDraw(n, ow, oh, changed);
        }
        else px = GbaGlass::NavCount(navCount, ow, oh, changed);
        if (!navTex || navTexW != ow || navTexH != oh)
        {
            if (navTex) SDL_DestroyTexture(navTex);
            navTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ow, oh);
            navTexW = ow;
            navTexH = oh;
            if (navTex) SDL_SetTextureBlendMode(navTex, SDL_BLENDMODE_BLEND);
            changed = true;
        }
        if (!navTex || !px) return;
        static bool lastOpen = false;
        if (changed || lastOpen != navOpen) SDL_UpdateTexture(navTex, nullptr, px, ow * 4);
        lastOpen = navOpen;
        SDL_RenderCopy(ren, navTex, nullptr, nullptr);
    }

    // The glass menu over the game: the game as just drawn is read back once (blurred under the
    // menu), the page drawn at the window's size and shown whole.
    void DrawGlass(int ow, int oh)
    {
        BuildGlass();
        if (glassShotW != ow || glassShotH != oh)
        {
            glassShotW = ow;
            glassShotH = oh;
            glassShot.assign((size_t)ow * oh, 0);
            if (!SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, glassShot.data(), ow * 4)) GbaGlass::Frame(glassShot.data(), ow, oh);
            else GbaGlass::Frame(nullptr, 0, 0); // (a renderer that cannot be read: the menu over the darkened game)
        }
        bool changed = false;
        const uint32_t* px = GbaGlass::Draw(glass, ow, oh, changed);
        if (!glassTex || glassTexW != ow || glassTexH != oh)
        {
            if (glassTex) SDL_DestroyTexture(glassTex);
            glassTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ow, oh);
            glassTexW = ow;
            glassTexH = oh;
            if (glassTex) SDL_SetTextureBlendMode(glassTex, SDL_BLENDMODE_BLEND);
            changed = true;
        }
        if (!glassTex) return;
        if (changed) SDL_UpdateTexture(glassTex, nullptr, px, ow * 4);
        SDL_RenderCopy(ren, glassTex, nullptr, nullptr);
    }

    // The highlight moves as in the DS games' menu (tiles in a grid, rows of a page, a question's
    // two buttons); what is chosen is then acted on by PauseAct, as a press on its row.
    void PauseInput(const MenuIn& raw)
    {
        BuildGlass();
        GbaGlass::Input gi;
        gi.up = raw.up; gi.down = raw.down; gi.left = raw.left; gi.right = raw.right; gi.accept = raw.accept;
        int dir = 0;
        const int hit = GbaGlass::Update(glass, gi, dir);
        glassSel = glass.selected;
        if (glassSel >= 0 && glassSel < (int)glassRow.size() && glassRow[glassSel] >= 0) pause.selected = glassRow[glassSel];
        MenuIn in = raw;
        in.up = in.down = in.left = in.right = in.accept = false;
        if (hit >= 0 && hit < (int)glass.items.size())
        {
            if (glass.items[hit].kind == GbaGlass::BACK) in.back = true;
            else if (glassRow[hit] >= 0)
            {
                pause.selected = glassRow[hit];
                (dir < 0 ? in.left : dir > 0 ? in.right : in.accept) = true;
            }
        }
        if (pausePage == P_LOGIN && pause.selected <= 1) raField = pause.selected;
        const int selWas = pause.selected;
        const PausePage pageWas = pausePage;
        PauseAct(in);
        if (pausePage != pageWas || pause.selected != selWas) glassSel = -1; // (the row the action chose)
        glassSlider = glassPick = -1;
    }
#endif

#ifdef GBAW_EMBEDDED
    void PauseAct(const MenuIn& in)
#else
    void PauseInput(const MenuIn& in)
#endif
    {
#ifdef GBAW_EMBEDDED
        if (pausePage == P_SETTINGS || pausePage == P_QUIT || pausePage == P_RESET || pausePage == P_SAVE || pausePage == P_LOAD)
        {
            HubPageInput(in);
            return;
        }
        if (pausePage == P_DEX || pausePage == P_DEXENTRY)
        {
            DexInput(in);
            return;
        }
        if (pausePage == P_COMP || pausePage == P_PARTY || pausePage == P_BAG || pausePage == P_PLACE || pausePage == P_BATTLE || pausePage == P_GUIDE || pausePage == P_FLY || pausePage == P_FLYMON || pausePage == P_BOXES || pausePage == P_BOXMON || pausePage == P_PARTYMON || pausePage == P_ITEM || pausePage == P_ITEMMON || pausePage == P_ITEMMOVE)
        {
            CompanionInput(in);
            return;
        }
        if (pausePage == P_LOGIN)
        {
            LoginInput(in);
            return;
        }
#ifdef GBAW_CHEATS
        if (pausePage == P_CHEATS)
        {
            CheatsInput(in);
            return;
        }
#endif
        if (IsSettingsPage())
        {
            SettingsPageInput(in);
            return;
        }
        if (pausePage != P_ROOT)
        {
            const int n = (int)pause.items.size();
            if (n <= 0) return;
            if (in.up) pause.selected = (pause.selected + n - 1) % n;
            if (in.down) pause.selected = (pause.selected + 1) % n;
            if (in.menu)
            {
                Resume();
                return;
            }
            if (in.back)
            {
                if (pausePage == P_LIST) GoPage(P_RA, "ACHIEVEMENTS");
                else GoPage(P_ROOT, "RETROACHIEVEMENTS");
                return;
            }
            const bool turn = pausePage == P_RA && (in.left || in.right);
            if (!in.accept && !turn) return;
            if (pause.selected < 0 || pause.selected >= n) return;
            const std::string item = pause.items[pause.selected].label;
            if (turn && item == "RETROACHIEVEMENTS" && ra && ra->setOn && ra->on)
            {
                ra->setOn(ra->ud, ra->on(ra->ud) ? 0 : 1);
                return;
            }
            if (!in.accept) return;
            if (item == "RETROACHIEVEMENTS" && ra && ra->setOn && ra->on)
                ra->setOn(ra->ud, ra->on(ra->ud) ? 0 : 1);
            else if (item == "ACHIEVEMENTS")
            {
                pausePage = P_LIST;
                pause.selected = 0;
            }
            else if (item == "LOG IN")
            {
                pausePage = P_LOGIN;
                pause.selected = raUser.empty() ? 0 : 1;
                raField = pause.selected;
                ForgetPass();
                typing = true;
                SDL_StartTextInput();
            }
            else if (item == "LOG OUT")
            {
                if (ra && ra->logout) ra->logout(ra->ud);
            }
            else if (item == "BACK")
                GoPage(P_ROOT, "RETROACHIEVEMENTS");
            return;
        }
#endif
        const int n = (int)pause.items.size();
        if (n <= 0) return;
        if (in.up) pause.selected = (pause.selected + n - 1) % n;
        if (in.down) pause.selected = (pause.selected + 1) % n;
        if (in.back || in.menu)
        {
            Resume();
            return;
        }
        if (pause.selected < 0 || pause.selected >= n) return;
        const std::string& item = pause.items[pause.selected].label;
        if (item == "FAST FORWARD" && (in.left || in.right || in.accept))
        {
            fastToggle = !fastToggle;
            set.values["fast_lock"] = fastToggle ? "1" : "0";
            set.Save();
            set.ToHost("fast_lock");
            pause.items[pause.selected].value = fastToggle ? "ON" : "OFF";
            return;
        }
        if (!in.accept) return;
#ifdef GBAW_EMBEDDED
        if (item == "SETTINGS")
        {
            GoPage(P_SETTINGS, "DISPLAY");
            return;
        }
        // (SAVE and LOAD: the slots, on the one saved last.)
        if (item == "SAVE STATE" || item == "LOAD STATE")
        {
            int newest = 0;
            time_t when = 0;
            for (int i = 0; i < StateSlots; i++)
            {
                struct stat st;
                if (stat(StatePath(i).c_str(), &st) == 0 && st.st_mtime > when) { when = st.st_mtime; newest = i; }
            }
            GoPage(item == "SAVE STATE" ? P_SAVE : P_LOAD, ("SLOT " + std::to_string(newest + 1)).c_str());
            return;
        }
        if (item == "QUIT")
        {
            GoPage(P_QUIT, "CANCEL");
            return;
        }
        if (item == "COMPANION")
        {
            if (!compConfig) { Toast("COMPANION ISN'T AVAILABLE FOR THIS GAME"); return; }
            ReadCompanion();
            GoPage(P_COMP, "PARTY");
            return;
        }
        if (item == "POKEDEX")
        {
            // (Read from the ROM the first time; a game whose tables aren't found has none.)
            if (!dexTried) { dexTried = true; dex.Load(dexRom); fprintf(stderr, "[gba] pokedex: %d species%s\n", dex.count, dex.Ready() ? "" : " (not found)"); }
            if (!dex.Ready() || !dex.count) { Toast("THE POKEDEX ISN'T AVAILABLE FOR THIS GAME"); return; }
            ReadCompanion(); // (seen and caught)
            GoPage(P_DEX, "");
            return;
        }
        if (item == "RETROACHIEVEMENTS" && !raReady) return;
#ifdef GBAW_CHEATS
        if (item == "CHEATS" && !cheats.Active()) return;
#endif
#endif
        if (item == "RESUME") Resume();
        else if (item == "SAVE STATE")
        {
            SaveState();
            Resume();
        }
        else if (item == "LOAD STATE")
        {
            LoadState();
            Resume();
        }
        else if (item == "TOUCH CONTROLS")
        {
            const bool saved = TouchLayout::Edit<Canvas>(ren, touchProfile, data + "/touch-layout-gba.cfg", Touch::Defaults);
            fingers.clear(); touchHeld = touchTap = 0; touchTapFrames = 0;
            std::fill(std::begin(key), std::end(key), false);
            Toast(saved ? "TOUCH LAYOUT SAVED" : "TOUCH LAYOUT UNCHANGED");
        }
        else if (item == "SETTINGS")
        {
            settingsBack = PLAY;
            screen = SETTINGS;
            settingsSel = 0;
        }
#ifdef GBAW_EMBEDDED
        else if (item == "DISPLAY" || item == "SOUND" || item == "CONTROLS" || item == "GAMEPLAY")
        {
            pausePage = item == "DISPLAY" ? P_DISPLAY : item == "SOUND" ? P_SOUND : item == "GAMEPLAY" ? P_GAMEPLAY : P_CONTROLS;
            BuildPause();
            pause.selected = 0;
        }
#endif
#ifdef GBAW_EMBEDDED
        else if (item == "RETROACHIEVEMENTS")
        {
            pausePage = P_RA;
            pause.selected = 0;
        }
#endif
#ifdef GBAW_CHEATS
        else if (item == "CHEATS")
        {
            pausePage = P_CHEATS;
            pause.selected = 0;
        }
#endif
        else if (item == "RESET GAME")
        {
            core->reset(core);
            pk_wide_reset();
            camera.Reset();
#ifdef GBAW_CHEATS
            cheats.Reset();
#endif
#ifdef GBAW_EMBEDDED
            if (raReady && ra && ra->reset) ra->reset(ra->ud);
#endif
            Resume();
        }
        else if (item == "QUIT GAME" || item == "QUIT")
            Stop();
    }

    void ListInput(const MenuIn& in)
    {
        const int n = (int)games.size(), cols = std::max(1, listCols);
#if defined(__ANDROID__)
        if ((needFolder || !n) && in.accept)
        {
            // Android's folder picker (after asking for access to files the first time).
            pickingFolder = true;
            Android::ChooseFolder();
            return;
        }
#endif
        if (n)
        {
            if (in.left && listSel > 0) listSel--;
            if (in.right && listSel < n - 1) listSel++;
            if (in.up && listSel >= cols) listSel -= cols;
            if (in.down && listSel + cols < n) listSel += cols;
            if (in.accept) Start(games[listSel]);
        }
        if (in.settings)
        {
            settingsBack = LIST;
            screen = SETTINGS;
            settingsSel = 0;
        }
        if (in.folder) OpenFolder();
        if (in.escape) quit = true;
    }

    void OpenFolder()
    {
#if !defined(__SWITCH__) && !defined(__ANDROID__)
        std::string url = "file://";
#ifdef _WIN32
        url += "/";
#endif
        url += roms;
        SDL_OpenURL(url.c_str());
#endif
    }

    void SettingsInput(const MenuIn& in)
    {
        const auto& l = SettingsList();
        const int n = (int)l.size();
        if (in.up) settingsSel = (settingsSel + n - 1) % n;
        if (in.down) settingsSel = (settingsSel + 1) % n;
        if (in.left || in.right || in.accept)
        {
            const Setting& s = l[settingsSel];
            set.Step(s, in.left ? -1 : 1);
            if (!strcmp(s.key, "fullscreen"))
            {
                const bool want = set.Int("fullscreen");
                const bool full = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                if (want != full) SDL_SetWindowFullscreen(win, want ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            }
            if (!strcmp(s.key, "volume")) SetVolume();
        }
        if (in.back || in.menu || in.settings) screen = settingsBack;
    }

    // -----------------------------------------------------------------------------------------
    // Drawing

    // COLOUR CORRECTION: each of the GBA's 32768 colours as a GBA's screen showed it (its dark,
    // washed-out screen, which the games' bright colours were made for): the mix of the GBA colour
    // shaders by Pokefan531, in linear light. In and out as the picture's own ABGR.
    void BuildColourLut()
    {
        colourLut.resize(32768);
        for (int i = 0; i < 32768; i++)
        {
            const double r = std::pow((i & 31) / 31.0, 2.2), g = std::pow(((i >> 5) & 31) / 31.0, 2.2),
                         b = std::pow(((i >> 10) & 31) / 31.0, 2.2);
            const double R = 0.82 * r + 0.24 * g - 0.06 * b, G = 0.125 * r + 0.665 * g + 0.21 * b, B = 0.195 * r + 0.075 * g + 0.73 * b;
            auto out = [](double v) { return (uint32_t)std::lround(std::pow(std::min(1.0, std::max(0.0, v * 0.94)), 1 / 2.2) * 255); };
            colourLut[i] = 0xFF000000u | out(B) << 16 | out(G) << 8 | out(R);
        }
    }

    // LCD EFFECT: a grid along the edges of the GBA's pixels, at the size the picture is drawn
    // (so its lines fall exactly between them), laid over the picture to darken.
    void DrawLcd(const SDL_Rect& dst, int w, int h = GbaH)
    {
        const double sx = (double)dst.w / w, sy = (double)dst.h / h;
        if (std::min(sx, sy) < 2) return; // (too small for a grid)
        if (!lcdTex || lcdW != dst.w || lcdH != dst.h || lcdCols != w || lcdRows != h)
        {
            if (lcdTex) SDL_DestroyTexture(lcdTex);
            lcdW = dst.w;
            lcdH = dst.h;
            lcdCols = w;
            lcdRows = h;
            const int t = std::max(1, (int)std::lround(std::min(sx, sy) / 5));
            std::vector<uint8_t> colEdge(lcdW), rowEdge(lcdH);
            for (int x = 0; x < lcdW; x++) colEdge[x] = (int)((int)(x / sx + 1e-9) * sx + sx) - x <= t;
            for (int y = 0; y < lcdH; y++) rowEdge[y] = (int)((int)(y / sy + 1e-9) * sy + sy) - y <= t;
            std::vector<uint32_t> px((size_t)lcdW * lcdH);
            for (int y = 0; y < lcdH; y++)
                for (int x = 0; x < lcdW; x++)
                {
                    const uint32_t v = rowEdge[y] || colEdge[x] ? 0xD4 : 0xFF;
                    px[(size_t)y * lcdW + x] = 0xFF000000u | v << 16 | v << 8 | v;
                }
            lcdTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, lcdW, lcdH);
            if (!lcdTex) return;
            SDL_UpdateTexture(lcdTex, nullptr, px.data(), lcdW * 4);
            SDL_SetTextureBlendMode(lcdTex, SDL_BLENDMODE_MOD);
            SDL_SetTextureScaleMode(lcdTex, SDL_ScaleModeNearest);
        }
        SDL_RenderCopy(ren, lcdTex, nullptr, &dst);
    }

    // The game's picture, as large as fits (SMOOTH SCALING, HD FILTER, LCD EFFECT and COLOUR
    // CORRECTION decide how).
#if defined(__SWITCH__)
    static constexpr int HdFactor = 3; // (720p or 1080p: 3 is plenty, and a third quicker)
#else
    static constexpr int HdFactor = 4;
#endif
    // ZOOM and the look-around: the window WIDE drew, over the whole screen at the camera's scale, with the same
    // SCALING, COLOUR CORRECTION, HD FILTER and LCD EFFECT as at 1x. (The HD filter scales by no more than the screen
    // needs - 2x when fully zoomed out - so zooming out costs it about what 1x does.)
    void DrawZoom()
    {
        const PkWideCamera::Window& w = zoomWin;
        const bool hd = set.Get("hd_filter") == "1", colour = set.Get("colour") == "1", lcd = set.Get("lcd") == "1";
        const SDL_Rect src = { 0, 0, w.w, w.h };
        const SDL_Rect dst = { (int)std::floor(w.offX), (int)std::floor(w.offY), (int)std::ceil(w.w * w.scale), (int)std::ceil(w.h * w.scale) };
        if (hd || colour)
        {
            if (colour && colourLut.empty()) BuildColourLut();
            fxSrc.resize((size_t)w.w * w.h);
            for (size_t i = 0; i < fxSrc.size(); i++)
            {
                uint32_t p = zoomBuf[i]; // (ABGR: R in the low byte)
                if (colour) p = colourLut[((p >> 3) & 31) | (((p >> 11) & 31) << 5) | (((p >> 19) & 31) << 10)];
                if (hd) p = (p & 0xFF00FF00u) | (p & 0xFF) << 16 | ((p >> 16) & 0xFF);
                fxSrc[i] = p;
            }
        }
        if (hd)
        {
            const int f = std::max(2, std::min(HdFactor, (int)std::ceil(w.scale - 0.01)));
            const int ow = w.w * f, oh = w.h * f;
            if (!workersStarted)
            {
                workersStarted = true;
#if defined(__SWITCH__)
                workers.Start(2);
#else
                const unsigned cores = std::thread::hardware_concurrency();
                workers.Start((int)std::min(3u, cores > 1 ? cores - 1 : 0u));
#endif
            }
            fxOut.resize((size_t)ow * oh);
            const int slices = 8;
            workers.Run(slices, [&](int i) {
                xbrz::scale(f, fxSrc.data(), fxOut.data(), w.w, w.h, xbrz::ColorFormat::RGB, xbrz::ScalerCfg(), w.h * i / slices, w.h * (i + 1) / slices);
            });
            if (!zoomHdTex || zoomHdW < ow || zoomHdH < oh)
            {
                if (zoomHdTex) SDL_DestroyTexture(zoomHdTex);
                zoomHdW = std::max(zoomHdW, ow);
                zoomHdH = std::max(zoomHdH, oh);
                zoomHdTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, zoomHdW, zoomHdH);
                if (zoomHdTex) SDL_SetTextureScaleMode(zoomHdTex, SDL_ScaleModeLinear);
            }
            if (zoomHdTex)
            {
                const SDL_Rect r = { 0, 0, ow, oh };
                SDL_UpdateTexture(zoomHdTex, &r, fxOut.data(), ow * 4);
                SDL_RenderCopy(ren, zoomHdTex, &r, &dst);
                if (lcd) DrawLcd(dst, w.w, w.h);
                return;
            }
        }
        if (!zoomTex)
        {
            zoomTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, PkWideCamera::Camera::MaxW, PkWideCamera::Camera::MaxH);
            if (!zoomTex) return;
        }
        SDL_UpdateTexture(zoomTex, &src, colour ? fxSrc.data() : zoomBuf.data(), w.w * 4);
        SDL_SetTextureScaleMode(zoomTex, set.Get("scaling") == "smooth" ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
        SDL_RenderCopy(ren, zoomTex, &src, &dst);
        if (lcd) DrawLcd(dst, w.w, w.h);
    }

    void DrawGame()
    {
        if (zoomShown && zoomWin.valid)
        {
            const uint64_t t0 = SDL_GetPerformanceCounter();
            DrawZoom();
            zoomDrawCost = (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
            return;
        }
        const int w = GbaW + 2 * wideExtra;
        int ow, oh;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        const std::string scaling = set.Get("scaling");
        const bool hd = set.Get("hd_filter") == "1", colour = set.Get("colour") == "1", lcd = set.Get("lcd") == "1";
        double scale = std::min((double)ow / w, (double)oh / GbaH);
        if (scaling == "whole") scale = std::max(1.0, std::floor(scale));
        const int dw = (int)lround(w * scale), dh = (int)lround(GbaH * scale);
        const SDL_Rect dst = { (ow - dw) / 2, (oh - dh) / 2, dw, dh };
        const SDL_Rect src = { 0, 0, w, GbaH };
        if (hd || colour)
        {
            // The picture's w columns, colour corrected; for xBRZ as ARGB (it weighs the channels).
            if (colour && colourLut.empty()) BuildColourLut();
            fxSrc.resize((size_t)w * GbaH);
            for (int y = 0; y < GbaH; y++)
                for (int x = 0; x < w; x++)
                {
                    uint32_t p = video[(size_t)y * MaxW + x];
                    if (colour) p = colourLut[((p >> 3) & 31) | (((p >> 11) & 31) << 5) | (((p >> 19) & 31) << 10)];
                    if (hd) p = (p & 0xFF00FF00u) | (p & 0xFF) << 16 | ((p >> 16) & 0xFF);
                    fxSrc[(size_t)y * w + x] = p;
                }
        }
        if (hd)
        {
            if (!workersStarted)
            {
                workersStarted = true;
#if defined(__SWITCH__)
                workers.Start(2); // (on cores 1 and 2: see Loop)
#else
                const unsigned cores = std::thread::hardware_concurrency();
                workers.Start((int)std::min(3u, cores > 1 ? cores - 1 : 0u));
#endif
            }
            fxOut.resize((size_t)w * HdFactor * GbaH * HdFactor);
            const int slices = 8;
            workers.Run(slices, [&](int i) {
                xbrz::scale(HdFactor, fxSrc.data(), fxOut.data(), w, GbaH, xbrz::ColorFormat::RGB, xbrz::ScalerCfg(),
                            GbaH * i / slices, GbaH * (i + 1) / slices);
            });
            if (!hdTex)
            {
                hdTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, MaxW * HdFactor, GbaH * HdFactor);
                if (hdTex) SDL_SetTextureScaleMode(hdTex, SDL_ScaleModeLinear);
            }
            if (hdTex)
            {
                const SDL_Rect r = { 0, 0, w * HdFactor, GbaH * HdFactor };
                SDL_UpdateTexture(hdTex, &r, fxOut.data(), w * HdFactor * 4);
                SDL_RenderCopy(ren, hdTex, &r, &dst);
                if (lcd) DrawLcd(dst, w);
                return;
            }
        }
        if (colour) SDL_UpdateTexture(gameTex, &src, fxSrc.data(), w * 4);
        else SDL_UpdateTexture(gameTex, nullptr, video.data(), MaxW * 4);
        if (scaling == "sharp" && scale > 1)
        {
            // Sharp: first a whole-number zoom with square pixels, then smoothly to the screen,
            // so every pixel is the same size and the edges stay crisp.
            int n = (int)std::ceil(scale);
            while (n > 1 && MaxW * n > maxTex) n--;
            if (!bigTex || bigW != MaxW * n || bigH != GbaH * n)
            {
                if (bigTex) SDL_DestroyTexture(bigTex);
                bigW = MaxW * n;
                bigH = GbaH * n;
                bigTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, bigW, bigH);
                if (bigTex) SDL_SetTextureScaleMode(bigTex, SDL_ScaleModeLinear);
            }
            if (bigTex)
            {
                SDL_SetRenderTarget(ren, bigTex);
                const SDL_Rect bd = { 0, 0, w * n, GbaH * n };
                SDL_RenderCopy(ren, gameTex, &src, &bd);
                SDL_SetRenderTarget(ren, nullptr);
                SDL_RenderCopy(ren, bigTex, &bd, &dst);
                if (lcd) DrawLcd(dst, w);
                return;
            }
        }
        SDL_SetTextureScaleMode(gameTex, scaling == "smooth" ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
        SDL_RenderCopy(ren, gameTex, &src, &dst);
        if (lcd) DrawLcd(dst, w);
    }

    std::string Hint(const char* keys, const char* pad, const char* touch = nullptr) const
    {
        if (touchUi && !padLast && touch) return touch;
        return padLast ? pad : keys;
    }

    void DrawList(Canvas& c)
    {
        c.Fill(0, 0, c.w, c.h, Colour::Back);
        c.Fill(0, 0, c.w, 26, Colour::Band);
        c.Fill(0, 26, c.w, 1, Colour::BandLine);
        c.TextCentred(c.w / 2, 6, "POKEMON GBA WIDESCREEN", 2, Colour::Title);
        if (needFolder)
        {
            // (Android, the first time.)
            const std::vector<std::string> lines = {
                "WHERE ARE YOUR ROMS?",
                "",
                "TAP HERE (OR PRESS A) AND CHOOSE THE FOLDER WITH YOUR ROMS.",
                "ANDROID ASKS FIRST TO LET THE APP USE YOUR FILES: ALLOW IT,",
                "THEN COME BACK. YOUR SAVES GO IN A SAVES FOLDER INSIDE IT.",
                "",
                "IT PLAYS POKEMON EMERALD, FIRERED AND LEAFGREEN (US, 1.0 OR 1.1)",
                "AND ULTRA VIOLET 1.22 (ITS .GBA, OR ITS .IPS WITH FIRERED 1.0 BESIDE IT).",
            };
            int y = 60;
            for (const std::string& l : lines)
            {
                c.Text(40, y, l, 1, l == "WHERE ARE YOUR ROMS?" ? Colour::Bright : Colour::Text);
                y += 12;
            }
        }
        else if (games.empty())
        {
            const std::vector<std::string> lines = {
                "NO GAMES YET",
                "",
                "PUT YOUR ROMS IN THE ROMS FOLDER (ANY NAMES):",
                "  POKEMON EMERALD (US)",
                "  POKEMON FIRERED OR LEAFGREEN (US, VERSION 1.0 OR 1.1)",
                "  POKEMON ULTRA VIOLET 1.22: ITS .GBA, OR ITS .IPS",
                "  WITH FIRERED 1.0 BESIDE IT",
                "",
#if defined(__ANDROID__)
                "THEN TAP HERE (OR PRESS A) TO CHOOSE THE FOLDER AGAIN.",
#else
                "THEN PRESS R (OR A CONTROLLER'S X) TO LOOK AGAIN.",
#endif
            };
            int y = 60;
            for (const std::string& l : lines)
            {
                c.Text(40, y, l, 1, l == "NO GAMES YET" ? Colour::Bright : Colour::Text);
                y += 12;
            }
            c.Text(40, y + 8, roms.size() > 90 ? "..." + roms.substr(roms.size() - 87) : roms, 1, Colour::Hint);
        }
        else
        {
            // (Square art, as the DS launcher's; as many across as fit: four on a 16:9 screen.)
            const int tileW = 140, artH = 140, tileH = artH + 52, gap = 12;
            const int cols = std::max(1, std::min((int)games.size(), (c.w - 4 + gap) / (tileW + gap)));
            listCols = cols;
            const int rowsFit = std::max(1, (c.h - 80) / (tileH + gap));
            const int row = listSel / cols;
            const int firstRow = std::max(0, row - rowsFit + 1);
            const int shownCols = std::min(cols, (int)games.size());
            const int x0 = (c.w - (shownCols * tileW + (shownCols - 1) * gap)) / 2;
            // (In the middle of the page when they all fit.)
            const int rows = std::min(rowsFit, ((int)games.size() + cols - 1) / cols);
            const int y0 = std::max(40, 27 + (c.h - 27 - 40 - (rows * tileH + (rows - 1) * gap)) / 2);
            for (int i = firstRow * cols; i < (int)games.size() && i / cols < firstRow + rowsFit; i++)
            {
                const Games::Game& g = games[i];
                const int x = x0 + (i % cols) * (tileW + gap), y = y0 + (i / cols - firstRow) * (tileH + gap);
                const bool sel = i == listSel;
                if (sel) c.Frame(x, y, tileW, tileH, 2, Colour::Bright);
                tapRects.push_back({ { x, y, tileW, tileH }, i });
                c.Fill(x, y, tileW, tileH, sel ? 0xFF22304C : 0xFF141C2C);
                // The game's art (art/, drawn over the canvas at the screen's resolution), or its
                // colour with a gem in it.
                const std::string art = g.patch.empty() ? "" : Games::ArtCode(g.sha1);
                if (!art.empty() && Art(art))
                    artSpots.push_back({ { x, y, tileW, artH }, art, sel });
                else
                {
                    const uint32_t col = g.patch.empty() ? 0xFF505A70 : g.colour;
                    c.Fill(x, y, tileW, artH, col);
                    for (int r = 0; r < 14; r++)
                    {
                        const int half = r < 7 ? r + 1 : 14 - r;
                        c.Fill(x + tileW / 2 - half * 4, y + artH / 2 - 28 + r * 4, half * 8, 4, r < 7 ? 0xC0FFFFFF : 0x80FFFFFF);
                    }
                }
                std::vector<std::string> name = Wrap(g.name, (tileW - 12) / 6);
                if (name.size() > 1) name.resize(1);
                int ty = y + artH + 8;
                for (const std::string& l : name)
                {
                    c.TextCentred(x + tileW / 2, ty, l, 1, sel ? Colour::Bright : Colour::Text);
                    ty += 10;
                }
                std::string status = g.patch.empty() ? "NO WIDESCREEN" : g.forced ? "WIDESCREEN UNTESTED" : g.hasSave ? "SAVE FOUND" : "NEW GAME";
                c.TextCentred(x + tileW / 2, y + tileH - 30, status, 1, g.patch.empty() || g.forced ? Colour::Warn : g.hasSave ? 0xFF7EE08A : Colour::Hint);
                std::string file = g.fileName;
                if ((int)file.size() * 6 > tileW - 8) file = file.substr(0, (tileW - 8) / 6 - 3) + "...";
                c.TextCentred(x + tileW / 2, y + tileH - 16, file, 1, 0xFF5E6C88);
            }
            // (More games above or below the ones shown.)
            const int allRows = ((int)games.size() + cols - 1) / cols;
            if (firstRow > 0) c.TextCentred(c.w / 2, y0 - 12, "MORE ABOVE", 1, Colour::Hint);
            if (firstRow + rowsFit < allRows) c.TextCentred(c.w / 2, y0 + rows * (tileH + gap) - gap + 4, "MORE BELOW", 1, Colour::Hint);
            if (games[listSel].patch.empty() && !games[listSel].why.empty())
                c.TextCentred(c.w / 2, c.h - 30, "NO WIDESCREEN: " + games[listSel].why, 1, Colour::Warn);
            else if (games[listSel].forced)
                c.TextCentred(c.w / 2, c.h - 30, "ROM HACK: WIDESCREEN TRIED ANYWAY (" + games[listSel].why + ")", 1, Colour::Warn);
        }
        c.TextCentred(c.w / 2, c.h - 14,
                      Hint("ENTER: PLAY   H: SETTINGS   O: ROMS FOLDER   R: LOOK AGAIN   F11: WINDOW   ESC: QUIT",
#if defined(__SWITCH__)
                           "A: PLAY   Y: SETTINGS   X: LOOK AGAIN   +: QUIT",
#else
                           "A: PLAY   Y: SETTINGS   X: LOOK AGAIN",
#endif
                           "TAP A GAME TO PLAY IT"),
                      1, Colour::Hint);
        if (touchUi)
        {
            // (The settings for fingers.)
            settingsRect = { c.w - 76, 3, 72, 20 };
            c.Fill(settingsRect.x, settingsRect.y, settingsRect.w, settingsRect.h, 0xFF22304C);
            c.TextCentred(settingsRect.x + settingsRect.w / 2, settingsRect.y + 6, "SETTINGS", 1, Colour::Bright);
        }
    }

    void DrawSettings(Canvas& c)
    {
        c.Fill(0, 0, c.w, c.h, settingsBack == PLAY ? 0xE00B0F18 : Colour::Back);
        c.Fill(0, 0, c.w, 26, Colour::Band);
        c.Fill(0, 26, c.w, 1, Colour::BandLine);
        c.TextCentred(c.w / 2, 6, "SETTINGS", 2, Colour::Title);
        if (touchUi)
        {
            // (A way back for fingers.)
            backRect = { 4, 3, 60, 20 };
            c.Fill(backRect.x, backRect.y, backRect.w, backRect.h, 0xFF22304C);
            c.TextCentred(backRect.x + backRect.w / 2, backRect.y + 6, "< BACK", 1, Colour::Bright);
        }
        const int width = std::min(c.w - 40, 560), x = (c.w - width) / 2;
        const auto& l = SettingsList();
        // (The chosen option and the lines under it are kept on the page.)
        struct Line { std::string text, value; int item; bool name; };
        std::vector<Line> out;
        int selFirst = 0, selLast = 0;
        for (int i = 0; i < (int)l.size(); i++)
        {
            if (i == settingsSel) selFirst = (int)out.size();
            out.push_back({ l[i].name, set.Shown(l[i]), i, true });
            for (const std::string& t : Wrap(l[i].text, width / 6 - 4)) out.push_back({ t, "", i, false });
            if (i == settingsSel) selLast = (int)out.size() - 1;
            out.push_back({ "", "", -1, false });
        }
        const int top = 36, bottom = c.h - 22, lineH = 10;
        const int fit = std::max(1, (bottom - top) / lineH);
        static int scroll = 0;
        if (selFirst < scroll) scroll = selFirst;
        if (selLast >= scroll + fit) scroll = selLast - fit + 1;
        scroll = std::max(0, std::min(scroll, (int)out.size() - fit));
        int y = top;
        for (int i = scroll; i < (int)out.size() && y + lineH <= bottom; i++, y += lineH)
        {
            const Line& ln = out[i];
            const bool sel = ln.item == settingsSel;
            if (ln.item >= 0) tapRects.push_back({ { x - 4, y - 2, width + 8, lineH }, ln.item });
            if (ln.name)
            {
                if (sel) c.Fill(x - 4, y - 2, width + 8, lineH, Colour::Chosen);
                c.Text(x, y, ln.text, 1, sel ? Colour::Bright : Colour::Text);
                const std::string v = sel ? "< " + ln.value + " >" : ln.value;
                c.Text(x + width - Canvas::TextWidth(v, 1), y, v, 1, sel ? Colour::Bright : Colour::Title);
            }
            else if (!ln.text.empty())
                c.Text(x + 12, y, ln.text, 1, sel ? Colour::Text : Colour::Hint);
        }
        c.TextCentred(c.w / 2, c.h - 14, Hint("UP/DOWN: CHOOSE   LEFT/RIGHT: CHANGE   ESC: BACK", "D-PAD: CHOOSE AND CHANGE   B: BACK", "TAP TO CHOOSE, AGAIN TO CHANGE"), 1, Colour::Hint);
    }

    void Draw()
    {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        if (core) DrawGame();
#ifdef GBAW_EMBEDDED
        if (core && screen == PLAY && !paused) DrawNav();
#endif

        // The menus, at about 360 lines whatever the window's size (and at least 600 wide, for
        // the game list); on a phone about 300, so they are bigger under a finger.
        int ow, oh;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
#if defined(GBAW_PHONE)
        int lines = 300;
#else
        int lines = 360;
#endif
        if (testMode && getenv("GBAW_TEST_LINES")) lines = std::max(100, atoi(getenv("GBAW_TEST_LINES"))); // (a phone's, on a PC)
        const int k = std::max(1, std::min(oh / lines, ow / 600));
        ui.Resize(std::max(1, ow / k), std::max(1, oh / k));
        ui.Clear();
        uiScale = k;
        uiOffX = (ow - ui.w * k) / 2;
        uiOffY = (oh - ui.h * k) / 2;
        tapRects.clear();
        artSpots.clear();
        backRect = UiRect();
        settingsRect = UiRect();
        touchLayout = Touch::Make(ui.w, ui.h);
        Touch::Apply(touchLayout, touchProfile, ui.w, ui.h);
        bool any = true;
        if (screen == LIST) DrawList(ui);
        else if (screen == SETTINGS) DrawSettings(ui);
#ifdef GBAW_EMBEDDED
        else if (paused && core)
        {
            DrawGlass(ow, oh); // (shown here, under the messages)
            any = false;
        }
#endif
        else if (paused)
        {
            std::string title = game.name;
            std::string hint = Hint("ENTER: CHOOSE   ESC: BACK TO THE GAME", "A: CHOOSE   B: BACK TO THE GAME", "TAP TO CHOOSE, OUTSIDE TO GO BACK");
#ifdef GBAW_EMBEDDED
            if (pausePage == P_RA)
            {
                title = "RETROACHIEVEMENTS";
                char detail[80] = {};
                if (ra && ra->detail && ra->detail(ra->ud, detail, sizeof detail) && detail[0]) hint = detail;
                else hint = Hint("LEFT/RIGHT: ON OR OFF   ESC: BACK", "LEFT/RIGHT: ON OR OFF   B: BACK", "TAP TO CHANGE");
            }
            else if (pausePage == P_LIST)
            {
                title = "ACHIEVEMENTS";
                if (pause.selected >= 0 && pause.selected < (int)achDesc.size() && !achDesc[pause.selected].empty())
                    hint = achDesc[pause.selected];
            }
#ifdef GBAW_CHEATS
            else if (pausePage == P_CHEATS)
            {
                title = "CHEATS";
                const int i = pause.selected;
                if (i >= 0 && i < GbaCheats::COUNT)
                    hint = cheats.Has(i) ? GbaCheats::About(i).note : "NOT FOR THIS VERSION OF THE GAME";
            }
#endif
            else if (IsSettingsPage())
            {
                title = SettingsPageName();
                hint = Hint("ARROWS: CHANGE   ESC: BACK", "D-PAD: CHOOSE AND CHANGE   B: BACK", "TAP TO CHANGE");
            }
            else if (pausePage == P_LOGIN)
            {
                title = "LOG IN";
                hint = Hint("TYPE, THEN ENTER   ESC: CANCEL", "TYPE WITH THE KEYBOARD   B: CANCEL", "TYPE, THEN TAP LOG IN");
            }
#endif
            pause.Draw(ui, title, hint, touchUi && !padLast);
#ifdef GBAW_EMBEDDED
            // The chosen setting's explanation, under the menu (as the settings screen has it).
            const int sel = pause.selected;
            if ((IsSettingsPage()) && sel >= 0 && sel < (int)pageSettings.size())
            {
                const UiRect& b = pause.box;
                const std::vector<std::string> lines = Wrap(pageSettings[sel]->text, std::max(10, (b.w - 12) / 6));
                const int h = (int)lines.size() * 10 + 6;
                const int y = std::min(b.y + b.h + 4, ui.h - h - 2);
                ui.Frame(b.x, y, b.w, h, 1, Colour::BoxEdge);
                ui.Fill(b.x, y, b.w, h, Colour::Box);
                for (size_t l = 0; l < lines.size(); l++) ui.TextCentred(b.x + b.w / 2, y + 4 + (int)l * 10, lines[l], 1, Colour::Hint);
            }
#endif
        }
#ifdef GBAW_EMBEDDED
        else if (touchUi) Touch::Draw(ui, touchLayout, touchHeld, fastToggle, navOpen);
#else
        else if (touchUi) Touch::Draw(ui, touchLayout, touchHeld, fastToggle);
#endif
        else any = false;
        const Uint32 now = SDL_GetTicks();
        if (set.Int("show_fps") && screen == PLAY && !paused)
        {
            if (now - fpsSince >= 1000)
            {
                fpsShown = (int)lround(fpsFrames * 1000.0 / std::max<Uint32>(1, now - fpsSince));
                fpsFrames = 0;
                fpsSince = now;
            }
            const std::string f = std::to_string(fpsShown) + " FPS";
            int fpsX = 2, fpsY = 2;
#ifdef __ANDROID__
            fpsX = std::max(2, ui.w / 20);
            fpsY = std::max(2, ui.h / 20);
#endif
            ui.Fill(fpsX, fpsY, Canvas::TextWidth(f, 1) + 6, 11, 0xA0000000);
            ui.Text(fpsX + 3, fpsY + 2, f, 1, Colour::Bright);
            any = true;
        }
        if (!toast.empty() && now < toastUntil)
        {
            const std::vector<std::string> lines = Wrap(toast, std::max(10, (ui.w - 40) / 6));
            int tw = 0;
            for (const std::string& l : lines) tw = std::max(tw, Canvas::TextWidth(l, 1));
            const int th = (int)lines.size() * 10 + 8, ty = ui.h - th - 28;
            ui.Fill((ui.w - tw) / 2 - 6, ty, tw + 12, th, 0xE0101828);
            int y = ty + 5;
            for (const std::string& l : lines)
            {
                ui.TextCentred(ui.w / 2, y, l, 1, Colour::Bright);
                y += 10;
            }
            any = true;
        }
        if (any)
        {
            if (!uiTex || uiTexW != ui.w || uiTexH != ui.h)
            {
                if (uiTex) SDL_DestroyTexture(uiTex);
                uiTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ui.w, ui.h);
                uiTexW = ui.w;
                uiTexH = ui.h;
                if (uiTex)
                {
                    SDL_SetTextureBlendMode(uiTex, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureScaleMode(uiTex, SDL_ScaleModeNearest);
                }
            }
            if (uiTex)
            {
                SDL_UpdateTexture(uiTex, nullptr, ui.px.data(), ui.w * 4);
                const SDL_Rect dst = { (ow - ui.w * k) / 2, (oh - ui.h * k) / 2, ui.w * k, ui.h * k };
                SDL_RenderCopy(ren, uiTex, nullptr, &dst);
            }
            for (const ArtSpot& a : artSpots)
                if (SDL_Texture* t = Art(a.code))
                {
                    // (The chosen game's at full brightness, the others a little dimmer.)
                    const Uint8 m = a.chosen ? 255 : 170;
                    SDL_SetTextureColorMod(t, m, m, m);
                    const SDL_Rect dst = { uiOffX + a.at.x * k, uiOffY + a.at.y * k, a.at.w * k, a.at.h * k };
                    SDL_RenderCopy(ren, t, nullptr, &dst);
                }
        }
    }

    // A game's art as a texture, read from the built-in JPEG the first time (nullptr if none).
    SDL_Texture* Art(const std::string& code)
    {
        if (code.empty()) return nullptr;
        auto it = artTex.find(code);
        if (it != artTex.end()) return it->second;
        SDL_Texture* t = nullptr;
        for (const WideArt& a : kWideArt)
            if (a.jpg && code == a.code)
            {
                int w = 0, h = 0, n = 0;
                if (unsigned char* px = stbi_load_from_memory(a.jpg, (int)a.size, &w, &h, &n, 4))
                {
                    t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
                    if (t)
                    {
                        SDL_UpdateTexture(t, nullptr, px, w * 4);
                        SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
                    }
                    stbi_image_free(px);
                }
            }
        artTex[code] = t;
        return t;
    }

    void Shot(const std::string& file)
    {
        int ow, oh;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!s) return;
        if (!SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch)) SDL_SaveBMP(s, file.c_str());
        SDL_FreeSurface(s);
        printf("shot %lld %s %dx%d extra %d\n", ScriptClock(), file.c_str(), ow, oh, wideExtra);
    }

    // -----------------------------------------------------------------------------------------
    // Events

    void Events(MenuIn& in)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                const bool down = e.type == SDL_KEYDOWN;
                const SDL_Scancode sc = e.key.keysym.scancode;
                if (sc < SDL_NUM_SCANCODES) key[sc] = down;
                if (!down) break;
                padLast = false;
                switch (sc)
                {
                case SDL_SCANCODE_AC_BACK: // (Android's back button or gesture)
                    if (!e.key.repeat) (screen == LIST ? in.escape : screen == PLAY && !paused ? in.menu : in.back) = true;
                    break;
#ifdef GBAW_EMBEDDED
                case SDL_SCANCODE_N: // (FULLDEX NAV, as in the DS games)
                    if (!e.key.repeat && screen == PLAY && !paused && core) navToggle = true;
                    break;
#endif
                case SDL_SCANCODE_UP: in.up = true; break;
                case SDL_SCANCODE_DOWN: in.down = true; break;
                case SDL_SCANCODE_LEFT: in.left = true; break;
                case SDL_SCANCODE_RIGHT: in.right = true; break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                case SDL_SCANCODE_X:
                    if (!e.key.repeat) in.accept = true;
                    break;
                case SDL_SCANCODE_Z:
                    if (!e.key.repeat) in.back = true;
                    break;
                case SDL_SCANCODE_BACKSPACE:
                    if (!e.key.repeat)
                    {
#ifdef GBAW_EMBEDDED
                        if (typing) in.erase = true;
                        else
#endif
                            in.back = true;
                    }
                    break;
                case SDL_SCANCODE_ESCAPE:
#ifdef GBAW_EMBEDDED
                    // (As the DS games' menu: ESC goes back a page, and from the first page to the game.)
                    if (!e.key.repeat) (screen == LIST ? in.escape : paused ? in.back : in.menu) = true;
#else
                    if (!e.key.repeat) (screen == LIST ? in.escape : in.menu) = true;
#endif
                    break;
                case SDL_SCANCODE_H: in.settings = true; break;
                case SDL_SCANCODE_O: in.folder = true; break;
                case SDL_SCANCODE_R:
                    if (screen == LIST) Rescan();
                    break;
                case SDL_SCANCODE_F11: ToggleFullscreen(); break;
                case SDL_SCANCODE_C:
                    if (!e.key.repeat && screen == PLAY && !paused) ZoomCycle();
                    break;
                default: break;
                }
                break;
            }
            case SDL_TEXTINPUT:
#ifdef GBAW_EMBEDDED
                if (typing)
                {
                    std::string& field = raField == 0 ? raUser : raPass;
                    field += e.text.text;
                    if (field.size() > 128) field.resize(128);
                }
#endif
                break;
            case SDL_CONTROLLERDEVICEADDED:
                pads.Open(e.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                pads.Close(e.cdevice.which);
                break;
            case SDL_FINGERDOWN:
            case SDL_FINGERMOTION:
            case SDL_FINGERUP:
                Finger(e.tfinger, in);
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT && e.button.which != SDL_TOUCH_MOUSEID &&
                    (screen != PLAY || paused))
                {
                    int ww = 0, wh = 0;
                    SDL_GetWindowSize(win, &ww, &wh);
                    if (ww > 0 && wh > 0)
                    {
                        // A trackpad reports mouse positions in window points. Finger() maps
                        // normalised positions onto the output pixels, including HiDPI windows.
                        SDL_TouchFingerEvent tap{};
                        tap.type = SDL_FINGERDOWN;
                        tap.x = float(e.button.x) / ww;
                        tap.y = float(e.button.y) / wh;
                        Finger(tap, in, false);
                    }
                }
                break;
            case SDL_CONTROLLERBUTTONDOWN:
            {
                padLast = true;
                touchUi = false;
                const bool byLabel = ByLabel();
                const int b = Labelled(e.cbutton.button);
                if (b == (byLabel ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B)) in.accept = true;
                if (b == (byLabel ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A)) in.back = true;
                if (b == SDL_CONTROLLER_BUTTON_Y && screen == LIST) in.settings = true;
                if (b == SDL_CONTROLLER_BUTTON_X && screen == LIST) Rescan();
#if defined(__SWITCH__)
                // (+ leaves the program from the game list, as Switch homebrew does.)
                if (b == SDL_CONTROLLER_BUTTON_START && screen == LIST) in.escape = true;
#endif
                // The pause menu: START (+), as in the DS games, as it is let go (pressed with
                // SELECT (-) it is the GBA's START instead); START BUTTON GBA START: the other way
                // round. The Guide (Home, PS) button and a stick click open it too.
                // (ZOOM AND LOOK AROUND: in a game it knows, a click of the right stick zooms instead.)
                if (b == SDL_CONTROLLER_BUTTON_RIGHTSTICK && screen == PLAY && !paused && ZoomOn()) ZoomCycle();
#ifdef GBAW_EMBEDDED
                // (FULLDEX NAV: a click of the left stick in a game, as in the DS games.)
                else if (b == SDL_CONTROLLER_BUTTON_LEFTSTICK && screen == PLAY && !paused && core) navToggle = true;
#endif
                else if (b == SDL_CONTROLLER_BUTTON_GUIDE || b == SDL_CONTROLLER_BUTTON_LEFTSTICK || b == SDL_CONTROLLER_BUTTON_RIGHTSTICK)
                    in.menu = true;
                if (screen == PLAY)
                {
                    const bool startIsGba = set.Get("gba_start") == "start";
                    const Uint32 now = SDL_GetTicks();
                    if (b == SDL_CONTROLLER_BUTTON_START)
                    {
                        plusHeld = true;
                        plusAt = now;
                    }
                    if (b == SDL_CONTROLLER_BUTTON_BACK) minusAt = now;
                    if ((b == SDL_CONTROLLER_BUTTON_START || b == SDL_CONTROLLER_BUTTON_BACK) && !comboOn &&
                        pads.Button(SDL_CONTROLLER_BUTTON_START) && pads.Button(SDL_CONTROLLER_BUTTON_BACK))
                    {
                        comboOn = plusCombo = true;
                        if (startIsGba) in.menu = true;
                    }
                }
                break;
            }
            case SDL_CONTROLLERBUTTONUP:
            {
                const int b = Labelled(e.cbutton.button);
                const Uint32 now = SDL_GetTicks();
                if (b == SDL_CONTROLLER_BUTTON_START && plusHeld)
                {
                    if (screen == PLAY && !plusCombo && set.Get("gba_start") != "start") in.menu = true;
                    if (!comboOn && now - plusAt < 150) plusPulse = now + 150;
                    plusHeld = plusCombo = false;
                }
                if (b == SDL_CONTROLLER_BUTTON_BACK && !comboOn && now - minusAt < 150) minusPulse = now + 150;
                if ((b == SDL_CONTROLLER_BUTTON_START || b == SDL_CONTROLLER_BUTTON_BACK) &&
                    !pads.Button(SDL_CONTROLLER_BUTTON_START) && !pads.Button(SDL_CONTROLLER_BUTTON_BACK))
                    comboOn = false;
                break;
            }
            case SDL_CONTROLLERAXISMOTION:
                // The left trigger opens the pause menu too (it has nothing else to do).
                if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
                {
                    const bool held = e.caxis.value > 16000;
                    if (held && !leftTrigger)
                    {
                        padLast = true;
                        in.menu = true;
                    }
                    leftTrigger = held;
                }
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                { std::fill(std::begin(key), std::end(key), false); fastInputFocused = false; fastButton.Block(); fingers.clear(); touchHeld = 0; }
                else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) fastInputFocused = true;
                break;
            default:
                break;
            }
        }
        pads.MenuDirs(in);
    }

    // A finger on the screen: the on-screen controls while playing, taps in the menus.
    void Finger(const SDL_TouchFingerEvent& f, MenuIn& in, bool fromFinger = true)
    {
        int ow, oh;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        const float x = (f.x * ow - uiOffX) / uiScale, y = (f.y * oh - uiOffY) / uiScale;
        if (fromFinger) touchUi = true;
        padLast = false;
        if (screen == PLAY && !paused)
        {
            if (f.type == SDL_FINGERUP)
                fingers.erase(f.fingerId);
            else
            {
                const uint32_t m = Touch::Hit(touchLayout, x, y);
                if (f.type == SDL_FINGERDOWN)
                {
                    if (m & (1u << Touch::MENU)) in.menu = true;
                    // (The >> button on the screen: a tap turns fast forward on, another off.)
                    if (m & (1u << Touch::FAST)) fastToggle = !fastToggle;
#ifdef GBAW_EMBEDDED
                    // (FULLDEX NAV: its button opens and closes the watch; a tap on an app uses it, one off the
                    // watch and the controls closes it.)
                    if (m & (1u << Touch::NAV)) navToggle = true;
                    else if (navOpen && core)
                    {
                        GbaGlass::Nav at;
                        at.centre = touchUi;
                        NavLook(at);
                        const int hit = GbaGlass::NavHit(at, ow, oh, (int)(f.x * ow), (int)(f.y * oh));
                        if (hit >= 0) { navSel = hit; NavUse(hit); return; }
                        if (hit == -2) return;
                        if (!(m & 0x3FF)) { navOpen = false; holdKeys = true; return; }
                    }
#endif
                    if (m & 0x3FF)
                    {
                        touchTap |= m & 0x3FF;
                        touchTapFrames = 3;
                    }
                }
                fingers[f.fingerId] = m & 0x3FF;
            }
            touchHeld = 0;
            for (const auto& kv : fingers) touchHeld |= kv.second;
            return;
        }
        fingers.clear();
        touchHeld = 0;
        if (f.type != SDL_FINGERDOWN) return;
        if (backRect.Has(x, y))
        {
            in.back = true;
            return;
        }
        if (settingsRect.Has(x, y))
        {
            in.settings = true;
            return;
        }
#ifdef GBAW_EMBEDDED
        if (screen == PLAY && paused)
        {
            // (The glass menu is the window's size: the tap in its pixels.)
            GbaGlass::Input gi;
            GbaGlass::Tap(glass, (int)(f.x * ow), (int)(f.y * oh), gi, glassSlider, glassPick);
            glassSel = glass.selected;
            if (gi.left) in.left = true;
            if (gi.right) in.right = true;
            if (gi.accept) in.accept = true;
            if (gi.back) in.back = true;
            return;
        }
#endif
        if (screen == PLAY && paused)
        {
            for (int i = 0; i < (int)pause.rows.size(); i++)
                if (pause.rows[i].Has(x, y))
                {
                    pause.selected = i < (int)pause.rowItem.size() ? pause.rowItem[i] : i;
                    in.accept = true;
                    return;
                }
            if (!pause.box.Has(x, y)) in.back = true;
            return;
        }
        for (const auto& r : tapRects)
            if (r.first.Has(x, y))
            {
                if (screen == LIST)
                {
                    listSel = r.second;
                    in.accept = true;
                }
                else if (screen == SETTINGS)
                {
                    if (settingsSel == r.second) in.right = true;
                    settingsSel = r.second;
                }
                return;
            }
        // (The list with no games, or no folder yet: a tap anywhere chooses the folder.)
        if (screen == LIST && (games.empty() || needFolder)) in.accept = true;
    }

    // Test scripts push their key presses as events, so they go the same way as real ones.
    long long ScriptClock() const { return realTime ? (long long)gameFrames : frame; }
    void Script()
    {
        const long long now = ScriptClock();
        for (ScriptLine& l : script)
        {
            const bool down = l.done == 0 && now >= l.frame, up = l.done == 1 && now >= l.frame + l.hold;
            if (l.what == "key" && (down || up))
            {
                SDL_Event e{};
                e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
                e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
                e.key.keysym.scancode = SDL_GetScancodeFromName(l.arg.c_str());
                e.key.keysym.sym = SDL_GetKeyFromScancode(e.key.keysym.scancode);
                SDL_PushEvent(&e);
                l.done++;
            }
            if (l.what == "click" && (down || up))
            {
                // "click FRAME X,Y [HOLD]": a mouse click, without synthetic finger events.
                int ww = 0, wh = 0;
                float x = 0, y = 0;
                SDL_GetWindowSize(win, &ww, &wh);
                sscanf(l.arg.c_str(), "%f,%f", &x, &y);
                SDL_Event e{};
                e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
                e.button.button = SDL_BUTTON_LEFT;
                e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
                e.button.x = int(x * ww);
                e.button.y = int(y * wh);
                SDL_PushEvent(&e);
                l.done++;
            }
            if (l.what == "touch" && (down || up))
            {
                // "touch FRAME X,Y [HOLD]": a finger at X, Y (0 to 1 across the window)
                SDL_Event e{};
                e.type = down ? SDL_FINGERDOWN : SDL_FINGERUP;
                e.tfinger.fingerId = 1;
                sscanf(l.arg.c_str(), "%f,%f", &e.tfinger.x, &e.tfinger.y);
                SDL_PushEvent(&e);
                l.done++;
            }
#ifdef GBAW_CHEATS
            if (l.what == "cheat" && down && core)
            {
                std::string name = l.arg;
                for (char& c : name) if (c == '_') c = ' ';
                for (int i = 0; i < GbaCheats::COUNT; i++)
                    if (name == GbaCheats::About(i).name) cheats.Set(i, !cheats.On(i));
                fprintf(stderr, "[gba] test: cheat %s, %d on\n", name.c_str(), cheats.OnCount());
                l.done = 2;
            }
            if (l.what == "give" && down && core)
            {
                unsigned item = 0, qty = 0;
                if (sscanf(l.arg.c_str(), "%u:%u", &item, &qty) == 2) cheats.Give(cheatBus, (uint16_t)item, (uint16_t)qty);
                l.done = 2;
            }
            if (l.what == "cheatreport" && down && core)
            {
                fprintf(stderr, "[gba] report %lld: %s\n", now, cheats.Report(cheatBus).c_str());
                l.done = 2;
            }
#endif
            // (Tests: what a game keeps in the RAM the widescreen patches use, 0x0203FF00-0x0203FFF0,
            // played without one: a hack that uses it can't take a patch there.)
            // (Test: EWRAM, IWRAM, I/O and VRAM, raw, into the file the line names: where the player is and what the
            // map holds, read by a script.)
            if (l.what == "memdump" && down && core && l.arg != "x")
            {
                if (FILE* f = fopen(l.arg.c_str(), "wb"))
                {
                    for (uint32_t a = 0x02000000; a < 0x02040000; a++) fputc((int)core->rawRead8(core, a, -1) & 0xFF, f);
                    for (uint32_t a = 0x03000000; a < 0x03008000; a++) fputc((int)core->rawRead8(core, a, -1) & 0xFF, f);
                    // (then the I/O registers and video memory: which layers use which tiles)
                    for (uint32_t a = 0x04000000; a < 0x04000400; a++) fputc((int)core->rawRead8(core, a, -1) & 0xFF, f);
                    for (uint32_t a = 0x06000000; a < 0x06018000; a++) fputc((int)core->rawRead8(core, a, -1) & 0xFF, f);
                    fclose(f);
                }
                l.done = 2;
            }
            if (l.what == "ramcheck" && down && core)
            {
                int used = 0;
                std::string first;
                for (uint32_t a = 0x0203FF00; a < 0x0203FFF0; a++)
                {
                    const unsigned v = (unsigned)core->rawRead8(core, a, -1) & 0xFF;
                    if (v && v != 0xFF && used++ < 8)
                    {
                        char b[24];
                        snprintf(b, sizeof b, " %X=%02X", (unsigned)a, v);
                        first += b;
                    }
                }
                fprintf(stderr, "[gba] ramcheck %lld: %d bytes used%s\n", now, used, first.c_str());
                // (and to the file the line names: a Windows build has no console)
                if (l.arg != "x")
                    if (FILE* f = fopen(l.arg.c_str(), "a"))
                    {
                        fprintf(f, "ramcheck %lld: %d bytes used%s\n", now, used, first.c_str());
                        fclose(f);
                    }
                l.done = 2;
            }
            if (l.what == "quit" && down)
            {
                quit = true;
                l.done = 2;
            }
        }
    }
    void ScriptShots()
    {
        const long long now = ScriptClock();
        for (ScriptLine& l : script)
            if (l.what == "shot" && l.done == 0 && now >= l.frame)
            {
                Shot(l.arg);
                l.done = 2;
                shotTaken = true;
            }
    }

    // -----------------------------------------------------------------------------------------

    int Run()
    {
        fpsSince = SDL_GetTicks();
        // Frame pacing: when the pictures come 60, 120, 180 or 240 times a second (vsync on a
        // screen at that rate), one GBA frame every 1, 2, 3 or 4 of them, so each is shown for the
        // same time; otherwise (144 Hz, no vsync, or the game not keeping up) by the clock, 60 a
        // second. The rate is measured every 120 pictures rather than taken from the screen's
        // mode: Android can hold an app to 60 on a 120 Hz screen, and vsync may not really be on.
        // A rate counts when it is within 2% of one of those twice running (pictures that only
        // happen to take about a 60th of a second each, with no vsync, wander more than that),
        // and holds until it is 10% off. By the clock until then.
        int refresh = 0;
        SDL_DisplayMode mode;
        if (!SDL_GetWindowDisplayMode(win, &mode)) refresh = mode.refresh_rate;
        int perFrame = 1; // pictures for each GBA frame
        const bool canLock = realTime && !getenv("GBAW_TEST_CLOCK");
        bool lockToVsync = false;
        int vsyncCount = 0, lastEven = 0;
        if (realTime) Note("screen %d Hz", refresh);
        Uint64 vsyncCheckStart = SDL_GetPerformanceCounter();
        int vsyncCheckFrames = 0;
        const Uint64 freq = SDL_GetPerformanceFrequency();
        Uint64 last = SDL_GetPerformanceCounter();
        double due = 0;
        // (For GBAW_TEST_REALTIME: how steady the pictures came, from the game's start to the
        // first screenshot, which takes long enough to show as a gap itself.)
        // (Also how evenly the game's frames were spaced: each one should come 1/60 s after the
        // one before; an uneven one shows as a judder.)
        Uint64 playStart = 0, playEnd = 0, lastGameFrameAt = 0;
        unsigned long long playFrames0 = 0, playFrames1 = 0;
        int pictures = 0, gaps = 0, spaced = 0, uneven = 0;
        double longest = 0;
        while (!quit)
        {
            if (testMode) Script();
#if defined(__ANDROID__)
            // Back from Android's folder picker: the chosen folder's games.
            if (pickingFolder && !Android::Picking())
            {
                pickingFolder = false;
                Folders("");
                Rescan();
            }
#endif
            MenuIn in;
            Events(in);
            if (screen == LIST) ListInput(in);
            else if (screen == SETTINGS) SettingsInput(in);
            else if (paused)
            {
                BuildPause();
                if (pause.items.empty()) pause.selected = 0;
                else if (pause.selected < 0 || pause.selected >= (int)pause.items.size())
                    pause.selected = std::max(0, std::min(pause.selected, (int)pause.items.size() - 1));
                PauseInput(in);
                BuildPause();
                if (!pause.items.empty() && (pause.selected < 0 || pause.selected >= (int)pause.items.size()))
                    pause.selected = (int)pause.items.size() - 1;
            }
            else if (in.menu) OpenPause();

            if (fastButton.Update(set.Get("fast_button") == "toggle", FastDown(), fastInputFocused && screen == PLAY && !paused, fastToggle))
            {
                set.values["fast_lock"] = fastToggle ? "1" : "0";
                set.Save(); set.ToHost("fast_lock");
            }
            const Uint64 now = SDL_GetPerformanceCounter();
            const double dt = (double)(now - last) / freq;
            last = now;
            const unsigned long long framesBefore = gameFrames;
            if (core && screen == PLAY && !paused)
            {
                int run = 1;
                if (lockToVsync)
                    run = ++vsyncCount >= perFrame ? 1 : 0;
                if (run && lockToVsync) vsyncCount = 0;
                if (realTime && !lockToVsync)
                {
                    due += dt * Fps;
                    run = (int)due;
                    due -= run;
                    if (run > 4)
                    {
                        run = 4; // (after a stall: carry on rather than racing to catch up)
                        due = 0;
                    }
                }
                int speed = FastForward() ? std::max(2, set.Int("ff_speed")) : SceneSpeed();
                {
                    const uint32_t dirMask = (1u << K_UP) | (1u << K_DOWN) | (1u << K_LEFT) | (1u << K_RIGHT);
                    const uint32_t dirs = GameKeys() & dirMask;
                    if (dirs & ~tapDirs) tapSince = SDL_GetTicks();
                    tapDirs = dirs;
                    // (Fast forward switched on, by the Toggle button or the menu, stays on whatever
                    // is pressed, as in the DS games (#111, #156): the quarter second at normal speed is
                    // for fast forward held on its button, where a quick tap in a menu moved twice.)
                    if (speed > 1 && dirs && !fastToggle && SDL_GetTicks() - tapSince < 250) speed = 1;
                }
                for (int i = 0; i < run * speed; i++) RunFrame(true, speed > 1, i == run * speed - 1);
                if (run > 0) { SceneFrame(); FogFrame(); DayNightFrame(); FollowerFrame(); }
            }
            else
                due = 0;
#ifdef GBAW_EMBEDDED
            if (raReady && ra && ra->pump) ra->pump(ra->ud, paused || screen != PLAY);
            if (raReady && ra && ra->pop)
            {
                char msg[128] = {};
                if (ra->pop(ra->ud, msg, sizeof msg) && msg[0]) Toast(msg, 4000);
            }
#endif
            Draw();
            if (testMode) ScriptShots();
            SDL_RenderPresent(ren);
            if (canLock && ++vsyncCheckFrames == 120)
            {
                const double secs = (double)(SDL_GetPerformanceCounter() - vsyncCheckStart) / SDL_GetPerformanceFrequency();
                const double rate = 120 / std::max(secs, 0.001);
                const int m = (int)lround(rate / 60);
                const bool even = m >= 1 && m <= 6 && std::fabs(rate - 60.0 * m) < 1.2 * m;
                const bool twice = even && m == lastEven;
                lastEven = even ? m : 0;
                if (twice && (!lockToVsync || m != perFrame))
                {
                    Note("pictures at %.1f a second: a frame every %d of them", rate, m);
                    lockToVsync = true;
                    perFrame = m;
                    vsyncCount = 0;
                }
                else if (lockToVsync && std::fabs(rate - 60.0 * perFrame) > 6.0 * perFrame)
                {
                    // (Off by more than 10%: not a one-off slow picture but the screen's rate
                    // changing, or the game not keeping up.)
                    Note("pictures at %.1f a second: pacing by the clock", rate);
                    lockToVsync = false;
                    due = 0;
                }
                vsyncCheckStart = SDL_GetPerformanceCounter();
                vsyncCheckFrames = 0;
            }
            if (testMode && realTime && core && screen == PLAY && !shotTaken)
            {
                if (!playStart)
                {
                    playStart = now;
                    playFrames0 = gameFrames;
                }
                else
                {
                    pictures++;
                    longest = std::max(longest, dt);
                    if (dt > 0.025) gaps++;
                }
                if (gameFrames != framesBefore)
                {
                    if (lastGameFrameAt)
                    {
                        spaced++;
                        if (std::fabs((double)(now - lastGameFrameAt) / freq * Fps - 1) > 0.15) uneven++;
                    }
                    lastGameFrameAt = now;
                }
                playEnd = now;
                playFrames1 = gameFrames;
            }
            if (realTime && !lockToVsync)
            {
                // (No vsync, or not at 60 Hz: don't spin faster than needed.)
                const double spent = (double)(SDL_GetPerformanceCounter() - now) / freq;
                if (spent < 0.002) SDL_Delay(1);
            }
            frame++;
        }
        if (testMode && realTime && playStart)
        {
            SDL_RendererInfo info{};
            SDL_GetRendererInfo(ren, &info);
            int ww = 0, wh = 0;
            SDL_GetRendererOutputSize(ren, &ww, &wh);
            const double secs = (double)(playEnd - playStart) / freq;
            const unsigned long long n = playFrames1 - playFrames0;
            char how[64];
            if (lockToVsync) snprintf(how, sizeof how, "a frame every %d vsync%s", perFrame, perFrame > 1 ? "s" : "");
            else snprintf(how, sizeof how, "by the clock");
            printf("realtime: %s, %dx%d, screen %d Hz, %s: %llu game frames in %.2f s (%.2f a second), "
                   "%d pictures, %d gaps over 25 ms, longest %.1f ms, %d of %d frames unevenly spaced (%.1f%%)\n",
                   info.name ? info.name : "?", ww, wh, refresh, how, n, secs, secs > 0 ? n / secs : 0.0, pictures, gaps,
                   longest * 1000, uneven, spaced, spaced ? 100.0 * uneven / spaced : 0.0);
            fflush(stdout);
        }
        if (core) Stop();
        return 0;
    }
};

// SDL started as the player wants it (the launcher's list sets some of these its own way).
bool InitSdl()
{
    mLogSetDefaultLogger(&gQuiet);
    // (Controllers keep working while another window has the focus, as in the DS project.)
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#if defined(GBAW_PHONE)
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    // Fingers are read one by one (the on-screen controls), not as a mouse; tilting the phone
    // must not move the player.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
#if defined(__ANDROID__)
    // (Controllers come through Android's own input, as in the DS project: with SDL's HIDAPI
    // drivers some pads go missing.)
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
#endif
#endif
#ifdef _WIN32
    // The program's own icon (resource 1) for its window.
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "1");
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "1");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
    {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

} // namespace

#ifdef GBAW_EMBEDDED
namespace GbaWide
{

// A ROM's header (its title at 0xA0, its game code at 0xAC): a Pokemon game, or a hack that kept
// the title or code of one. (Read from the file: the list's cache does not keep the header.)
static bool LooksLikePokemon(const std::string& file)
{
    FILE* f = fopen(file.c_str(), "rb");
    if (!f) return true;
    unsigned char h[0xB0] = {};
    const size_t n = fread(h, 1, sizeof h, f);
    fclose(f);
    if (n < sizeof h) return true;
    std::string title, code;
    for (int i = 0xA0; i < 0xAC && h[i]; i++) title += (char)toupper(h[i]);
    for (int i = 0xAC; i < 0xB0; i++) code += (char)h[i];
    if (title.find("POKEMON") != std::string::npos || title.find("POCKET") != std::string::npos) return true;
    if (Games::PatchForCode(code)) return true;
    // Other GBA Pokemon games and their hacks: Pinball (BPP), Mystery Dungeon (B24), Ultra Violet (PUV).
    static const char* const kOther[] = {"BPP", "B24", "PUV"};
    for (const char* o : kOther)
        if (!code.compare(0, 3, o)) return true;
    return false;
}

std::vector<ListGame> Find(const std::string& roms, const std::string& saves)
{
    std::vector<ListGame> out;
    Games::MakeDir(saves);
    // One cache per roms folder. The launcher looks at roms and at the app folder in turn;
    // a single shared cache would drop the first folder's results when the second was saved.
    unsigned long h = 2166136261u;
    for (unsigned char c : roms) h = (h ^ c) * 16777619u;
    char cache[64];
    snprintf(cache, sizeof cache, "/gba-rom-list-%08lx.txt", h);
    for (const Games::Game& g : Games::Scan(roms, saves, saves + cache))
    {
        // FullDex lists Pokemon games only (#152): a GBA ROM that is none of ours, takes no
        // widescreen patch, has no Pokemon game code and no POKEMON in its title (Wario Land 4)
        // is left out of the list.
        if (!g.known && g.patch.empty() && g.ips.empty() && !LooksLikePokemon(g.rom)) continue;
        ListGame l;
        l.key = g.key;
        l.name = g.name;
        l.file = g.ips.empty() ? g.rom : g.ips;
        // A ROM hack with a widescreen patch of its own (Radical Red, Unbound, ...) has its own picture when
        // one is built in under the patch's name (art/radicalred.jpg): only that hack's ROM takes that patch
        // (its safety list), so its game as sold keeps the game's own picture.
        const unsigned char* own = nullptr;
        size_t ownSize = 0;
        l.art = g.patch.empty() ? std::string() : Art(g.patch, own, ownSize) ? g.patch : Games::ArtCode(g.sha1);
        // (and its name, rather than the file's: RadicalRed.gba reads RADICALRED.)
        static const struct { const char* patch; const char* name; } kHackNames[] = {
            { "radicalred", "Radical Red" }, { "unbound", "Unbound" }, { "emeraldlegacy", "Emerald Legacy" },
            { "inclement", "Inclement Emerald" }, { "monomyth", "Monomyth" }, { "emeraldrogue", "Emerald Rogue" },
        };
        for (const auto& n : kHackNames)
            if (g.patch == n.patch) l.name = n.name;
        l.hasSave = g.hasSave;
        l.widescreen = !g.patch.empty();
        l.hack = !g.known;
        out.push_back(l);
    }
    return out;
}

bool Art(const std::string& name, const unsigned char*& jpeg, size_t& size)
{
    for (const WideArt& a : kWideArt)
        if (a.jpg && name == a.code)
        {
            jpeg = a.jpg;
            size = a.size;
            return true;
        }
    return false;
}

// On a computer the game list closed SDL before the game and starts it again after: SDL is closed
// here too, as the list left it. Left running, the list's own start would be a second one that
// finds nothing new, and SDL tells of the controllers only when it starts: the list then had none.
// (Not on a phone or the Switch: SDL stays up there between the list and the game.)
static void LeaveSdl()
{
#if !defined(GBAW_PHONE) && !defined(__SWITCH__) && !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
    SDL_Quit();
#endif
}
bool Play(const std::string& file, const Options& o)
{
    if (!InitSdl()) return false;
    std::unique_ptr<App> app(new App());
    app->embedded = true;
    app->ra = o.achievements;
    app->UseFolders(o.roms, o.saves, o.data);
    if (const char* script = getenv("GBAW_TEST_SCRIPT"))
        if (*script) app->LoadScript(script);
    if (!app->OpenWindow(o.fullscreen ? 0 : o.w, o.fullscreen ? 0 : o.h, o.fullscreen ? 1 : 0))
    {
        fprintf(stderr, "[gba] can't open a window: %s\n", SDL_GetError());
        app->Close();
        LeaveSdl();
        return false;
    }
    app->OpenAudio();
    for (int i = 0; i < SDL_NumJoysticks(); i++) app->pads.Open(i);
    app->Rescan();
    bool started = false;
    for (const Games::Game& g : app->games)
        if (g.rom == file || g.ips == file)
        {
            fprintf(stderr, "[gba] %s (%s, widescreen patch %s)\n", file.c_str(), g.key.c_str(), g.patch.empty() ? "none" : g.patch.c_str());
            started = app->Start(g);
            break;
        }
    if (started) app->Run();
    else fprintf(stderr, "[gba] %s: not a GBA game in %s\n", file.c_str(), o.roms.c_str());
    app->Close();
    LeaveSdl();
    return started;
}

}
#else
int main(int argc, char** argv)
{
    std::string dataDir, romArg, gameArg, scriptArg;
    int winW = 0, winH = 0;
    for (int i = 1; i < argc; i++)
    {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--data") dataDir = next();
        else if (a == "--rom") romArg = next();
        else if (a == "--game") gameArg = next();
        else if (a == "--script") scriptArg = next();
        else if (a == "--window")
        {
            const std::string s = next();
            sscanf(s.c_str(), "%dx%d", &winW, &winH);
        }
        else
        {
            fprintf(stderr, "usage: %s [--game emerald|ultraviolet|firered|leafgreen] [--rom FILE] [--data DIR] [--window WxH] [--script FILE]\n", argv[0]);
            return 1;
        }
    }
    if (!InitSdl()) return 1;
    static App app;
    app.Folders(dataDir);
    if (!scriptArg.empty() && !app.LoadScript(scriptArg)) return 1;
    if (!app.OpenWindow(winW, winH))
    {
        fprintf(stderr, "Can't open a window: %s\n", SDL_GetError());
        return 1;
    }
    app.OpenAudio();
    for (int i = 0; i < SDL_NumJoysticks(); i++) app.pads.Open(i);
    app.Rescan();
    if (!romArg.empty())
    {
        Games::Game g;
        g.rom = romArg;
        g.fileName = romArg.substr(romArg.find_last_of("/\\") + 1);
        std::vector<uint8_t> rom;
        if (Games::ReadFile(romArg, rom) && Games::LooksLikeGba(rom))
        {
            Games::Identify(g, rom);
            g.hasSave = true;
            app.Start(g);
        }
        else
            app.Toast("NOT A GBA ROM: " + romArg);
    }
    else if (!gameArg.empty())
    {
        bool found = false;
        for (const Games::Game& g : app.games)
            if (g.key == gameArg && !found)
            {
                found = true;
                app.Start(g);
            }
        if (!found) app.Toast("NOT IN THE ROMS FOLDER: " + gameArg);
    }
    const int rc = app.Run();
    SDL_Quit();
    return rc;
}
#endif
