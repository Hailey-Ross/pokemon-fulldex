// The GBA games inside a Pokemon Fulldex build: finding them, and playing one in its own window.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "embed.h"
#include "wide_rom.h"
#include "box_art.h"

#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba/gba/interface.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/crc32.h>
#include <mgba-util/vfs.h>

extern "C" {
#include "pk_wide.h" // PokeDaisy's EXPAND (third_party/pokedaisy)
}

#include <SDL.h>

#include "ui.h" // Fulldex's: the pause menu looks like the DS games' one

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <map>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

namespace GbaWide
{

namespace
{

constexpr int kMaxWidth = GBA_VIDEO_HORIZONTAL_PIXELS + 2 * GBA_VIDEO_WIDE_MAX_EXTRA;
constexpr int kHeight = GBA_VIDEO_VERTICAL_PIXELS;
constexpr int kGameMaxExtra = 112; // the patches use at most this much
constexpr double kFps = 16777216.0 / 280896.0;

bool ReadWhole(const std::string& path, std::vector<uint8_t>& out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 64 * 1024 * 1024)
    {
        fclose(f);
        return false;
    }
    out.resize((size_t)n);
    const bool ok = fread(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

bool Exists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string Lower(std::string s)
{
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

std::string Upper(std::string s)
{
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

std::string Stem(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

// The file's name as a save name, as Pokemon Fulldex names them: lower case, other characters as one '-'.
std::string Slug(const std::string& s)
{
    std::string out;
    for (unsigned char c : s)
    {
        if (isalnum(c)) out += (char)tolower(c);
        else if (!out.empty() && out.back() != '-') out += '-';
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "game" : out;
}

bool IsIps(const std::string& path)
{
    return path.size() > 4 && Lower(path.substr(path.size() - 4)) == ".ips";
}

// Ultra Violet (a FireRed 1.0 hack), as Fulldex has it: its .gba, or its .ips over FireRed 1.0; told by its file's name.
bool IsUltraViolet(const std::string& path)
{
    std::string s;
    for (unsigned char c : Stem(path))
        if (isalnum(c)) s += (char)tolower(c);
    return s.find("ultraviolet") != std::string::npos;
}

// A clean FireRed 1.0 in the folder, for an .ips beside it.
std::string FireRedIn(const std::string& folder)
{
    std::string found;
    DIR* d = opendir(folder.c_str());
    if (!d) return found;
    while (dirent* e = readdir(d))
    {
        const std::string n = e->d_name;
        if (n.size() <= 4 || Lower(n.substr(n.size() - 4)) != ".gba") continue;
        uint8_t head[0xC0];
        FILE* f = fopen((folder + "/" + n).c_str(), "rb");
        if (!f) continue;
        const bool read = fread(head, 1, sizeof head, f) == sizeof head;
        fclose(f);
        if (!read || memcmp(head + 0xAC, "BPRE", 4) || head[0xBC] != 0) continue;
        std::vector<uint8_t> rom;
        const Profile* p = ReadWhole(folder + "/" + n, rom) ? FindProfile(rom) : nullptr;
        if (p && !strcmp(p->version, "firered"))
        {
            found = folder + "/" + n;
            break;
        }
    }
    closedir(d);
    return found;
}

// The game as it is played before widescreen: the ROM, or an .ips put on FireRed 1.0 (`base`, else one beside it).
bool LoadGame(const std::string& path, const std::string& base, std::vector<uint8_t>& rom)
{
    if (!IsIps(path)) return ReadWhole(path, rom);
    std::vector<uint8_t> ips;
    if (!ReadWhole(path, ips)) return false;
    const size_t slash = path.find_last_of("/\\");
    const std::string from = !base.empty() ? base : FireRedIn(slash == std::string::npos ? "." : path.substr(0, slash));
    if (from.empty() || !ReadWhole(from, rom)) return false;
    return ApplyIps(ips.data(), ips.size(), rom);
}

// A ROM's save name and list name: the patch's when it fits, the file's own otherwise.
void KeyAndName(const std::string& path, const std::vector<uint8_t>& rom, std::string& key, std::string& name)
{
    if (IsUltraViolet(path))
    {
        key = "ultraviolet";
        name = "Pokemon Ultra Violet";
        return;
    }
    if (const Profile* p = FindProfile(rom))
    {
        key = p->key;
        name = p->name;
        return;
    }
    key = Slug(Stem(path));
    name = Upper(Stem(path));
}

// gba-settings.ini: key=value lines
struct Settings
{
    std::string path;
    std::map<std::string, std::string> values;

    void Load(const std::string& dir)
    {
        path = dir + "/gba-settings.ini";
        if (FILE* f = fopen(path.c_str(), "r"))
        {
            char buf[512];
            while (fgets(buf, sizeof buf, f))
            {
                std::string l = buf;
                while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
                const size_t eq = l.find('=');
                if (eq != std::string::npos) values[l.substr(0, eq)] = l.substr(eq + 1);
            }
            fclose(f);
        }
    }
    std::string Get(const std::string& key, const std::string& fallback)
    {
        auto it = values.find(key);
        if (it == values.end() || it->second.empty())
        {
            values[key] = fallback;
            return fallback;
        }
        return it->second;
    }
    void Save() const
    {
        if (FILE* f = fopen(path.c_str(), "w"))
        {
            for (const auto& kv : values) fprintf(f, "%s=%s\n", kv.first.c_str(), kv.second.c_str());
            fclose(f);
        }
    }
};

// Extra pixels per side for a widescreen setting: off, auto (the window's shape) or a ratio like 16:9.
int ExtraFor(const std::string& shape, int windowW, int windowH)
{
    double aspect = 0;
    if (shape == "auto")
    {
        if (windowW > 0 && windowH > 0) aspect = (double)windowW / windowH;
    }
    else
    {
        int a = 0, b = 0;
        if (sscanf(shape.c_str(), "%d:%d", &a, &b) == 2 && a > 0 && b > 0) aspect = (double)a / b;
    }
    if (aspect <= 0) return 0;
    const int extra = (int)std::floor((kHeight * aspect - GBA_VIDEO_HORIZONTAL_PIXELS) / 2);
    return std::max(0, std::min(kGameMaxExtra, extra));
}

void QuietLog(struct mLogger*, int, enum mLogLevel, const char*, va_list) {}
struct mLogger gQuiet = { QuietLog, nullptr };

// RetroAchievements' view of GBA memory (see AchievementsHost::Read).
uint32_t ReadMemory(void* userdata, uint32_t address, uint8_t* buffer, uint32_t bytes)
{
    auto* core = (struct mCore*)userdata;
    uint32_t i = 0;
    for (; i < bytes; i++)
    {
        const uint32_t a = address + i;
        uint32_t bus;
        if (a < 0x8000) bus = 0x03000000 + a;
        else if (a < 0x48000) bus = 0x02000000 + (a - 0x8000);
        else if (a < 0x58000) bus = 0x0E000000 + (a - 0x48000);
        else break;
        buffer[i] = (uint8_t)core->busRead8(core, bus);
    }
    return i;
}

// Keyboard and controller to GBA keys
enum { KEY_A, KEY_B, KEY_SELECT, KEY_START, KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN, KEY_R, KEY_L };

uint32_t KeyboardKeys(const Uint8* k)
{
    uint32_t keys = 0;
    auto set = [&](int bit, bool on) { if (on) keys |= 1u << bit; };
    set(KEY_A, k[SDL_SCANCODE_X]);
    set(KEY_B, k[SDL_SCANCODE_Z]);
    set(KEY_SELECT, k[SDL_SCANCODE_BACKSPACE] || k[SDL_SCANCODE_RSHIFT]);
    set(KEY_START, k[SDL_SCANCODE_RETURN]);
    set(KEY_RIGHT, k[SDL_SCANCODE_RIGHT]);
    set(KEY_LEFT, k[SDL_SCANCODE_LEFT]);
    set(KEY_UP, k[SDL_SCANCODE_UP]);
    set(KEY_DOWN, k[SDL_SCANCODE_DOWN]);
    set(KEY_R, k[SDL_SCANCODE_S]);
    set(KEY_L, k[SDL_SCANCODE_A]);
    return keys;
}

// Nintendo pads put A where SDL's B is: with "labels" the button marked A is A.
bool SwapAB(SDL_GameController* pad, bool byLabel)
{
    if (!pad || !byLabel) return false;
#if SDL_VERSION_ATLEAST(2, 0, 12)
    const SDL_GameControllerType t = SDL_GameControllerGetType(pad);
    if (t == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO) return true;
#if SDL_VERSION_ATLEAST(2, 24, 0)
    if (t == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR) return true;
#endif
#endif
    return false;
}

uint32_t PadKeys(SDL_GameController* pad, bool byLabel)
{
    if (!pad) return 0;
    uint32_t keys = 0;
    auto b = [&](SDL_GameControllerButton btn) { return SDL_GameControllerGetButton(pad, btn) != 0; };
    const bool swap = SwapAB(pad, byLabel);
    const Sint16 lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    const Sint16 ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    auto set = [&](int bit, bool on) { if (on) keys |= 1u << bit; };
    set(KEY_A, b(swap ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A));
    set(KEY_B, b(swap ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B));
    set(KEY_SELECT, b(SDL_CONTROLLER_BUTTON_BACK));
    set(KEY_START, b(SDL_CONTROLLER_BUTTON_START));
    set(KEY_RIGHT, b(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx > 16000);
    set(KEY_LEFT, b(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || lx < -16000);
    set(KEY_UP, b(SDL_CONTROLLER_BUTTON_DPAD_UP) || ly < -16000);
    set(KEY_DOWN, b(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ly > 16000);
    set(KEY_R, b(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
    set(KEY_L, b(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
    return keys;
}

// Where the picture goes in the window for a scaling setting: sharp and smooth fill it keeping the
// shape, whole uses whole multiples only.
SDL_Rect Place(int winW, int winH, int picW, int picH, const std::string& scaling)
{
    double scale = std::min((double)winW / picW, (double)winH / picH);
    if (scaling == "whole" && scale >= 1) scale = std::floor(scale);
    SDL_Rect r;
    r.w = (int)(picW * scale);
    r.h = (int)(picH * scale);
    r.x = (winW - r.w) / 2;
    r.y = (winH - r.h) / 2;
    return r;
}

// One step through a list of choices, wrapping round.
template <size_t N>
std::string Step(const char* const (&choices)[N], const std::string& now, int dir)
{
    size_t i = 0;
    while (i < N && now != choices[i]) i++;
    if (i == N) i = 0;
    return choices[(i + N + (dir < 0 ? -1 : 1)) % N];
}

const char* const kShapes[] = { "auto", "off", "16:10", "16:9", "18:9", "20:9", "21:9" };
const char* const kScalings[] = { "sharp", "smooth", "whole" };

std::string ScalingLabel(const std::string& s)
{
    return s == "smooth" ? "SMOOTH" : s == "whole" ? "WHOLE PIXELS" : "SHARP";
}

// The pause menu's rows
enum Row { ROW_RESUME, ROW_WIDE, ROW_SCALING, ROW_VOLUME, ROW_FPS, ROW_FF, ROW_RA, ROW_RESET, ROW_QUIT };

std::string Fit(const std::string& s, size_t n)
{
    return s.size() <= n ? s : s.substr(0, n - 1) + ".";
}

}

std::vector<ListGame> Find(const std::string& roms, const std::string& saves)
{
    std::vector<ListGame> out;
    DIR* d = opendir(roms.c_str());
    if (!d) return out;
    std::vector<std::string> files;
    while (dirent* e = readdir(d))
    {
        const std::string n = e->d_name;
        if (n.size() > 4 && (Lower(n.substr(n.size() - 4)) == ".gba" || (IsIps(n) && IsUltraViolet(n))))
            files.push_back(roms + "/" + n);
    }
    closedir(d);
    std::sort(files.begin(), files.end());
    for (const std::string& file : files)
    {
        std::vector<uint8_t> rom;
        if (!LoadGame(file, "", rom)) continue;
        ListGame g;
        g.file = file;
        KeyAndName(file, rom, g.key, g.name);
        g.hasSave = Exists(saves + "/" + g.key + ".sav");
        if (!IsUltraViolet(file) && FindProfile(rom)) g.art = g.key;
        out.push_back(std::move(g));
    }
    std::vector<const char*> art;
    for (const ListGame& g : out)
        if (!g.art.empty()) art.push_back(g.art.c_str());
    FetchBoxArt(art.data(), art.size(), 2000);
    return out;
}

bool Art(const std::string& name, const unsigned char*& jpeg, size_t& size)
{
    return CachedBoxArt(name, jpeg, size);
}

bool Play(const std::string& romPath, Options& o)
{
    std::vector<uint8_t> original;
    if (!LoadGame(romPath, o.base, original))
    {
        fprintf(stderr, IsIps(romPath) ? "[gba] %s needs a clean FireRed 1.0 beside it\n" : "[gba] can't read %s\n",
                romPath.c_str());
        return false;
    }
    std::string key, name;
    KeyAndName(romPath, original, key, name);
    std::vector<uint8_t> rom = original;
    const bool testClean = getenv("GBAW_TEST_NO_PATCH") != nullptr; // (test: the game as it is, no widescreen at all)
    const Profile* profile = testClean ? nullptr : FindProfile(rom);
    if (profile && !ApplyWidePatch(*profile, rom))
    {
        fprintf(stderr, "[gba] the widescreen patch doesn't fit: plays at 3:2\n");
        rom = original;
        profile = nullptr;
    }
    // Our patch where it fits; otherwise PokeDaisy's EXPAND draws the sides for the games and hacks it knows
    const bool expand = !profile && !testClean && pk_wide_configure(doCrc32(rom.data(), rom.size()));
    struct ExpandOff
    {
        bool on;
        ~ExpandOff()
        {
            if (!on) return;
            pk_wide_configure(0);
            pk_wide_extras_reset();
        }
    } expandOff{ expand };
    fprintf(stderr, "[gba] %s: %s\n", name.c_str(),
            profile ? profile->version : expand ? "expand (PokeDaisy)" : "no widescreen for this ROM");

    Settings settings;
    settings.Load(o.data);
    std::string shape = settings.Get("widescreen", "auto");
    std::string scaling = settings.Get("scaling", "sharp");
    bool showFps = settings.Get("show_fps", "0") == "1";
    int volume = std::max(0, std::min(100, atoi(settings.Get("volume", "100").c_str())));
    const bool byLabel = settings.Get("buttons", "labels") != "positions";
    int ffSpeed = std::max(2, std::min(8, atoi(settings.Get("fast_forward", "3").c_str())));
    auto saveSettings = [&]() {
        settings.values["widescreen"] = shape;
        settings.values["scaling"] = scaling;
        settings.values["show_fps"] = showFps ? "1" : "0";
        settings.values["volume"] = std::to_string(volume);
        settings.values["fast_forward"] = std::to_string(ffSpeed);
        settings.Save();
    };
    saveSettings();

    mLogSetDefaultLogger(&gQuiet);
    struct VFile* vf = VFileMemChunk(rom.data(), rom.size());
    struct mCore* core = vf ? mCoreFindVF(vf) : nullptr;
    if (!core || !core->init(core))
    {
        if (vf) vf->close(vf);
        fprintf(stderr, "[gba] no core for %s\n", romPath.c_str());
        return false;
    }
    mCoreInitConfig(core, nullptr);
    std::vector<uint32_t> pixels((size_t)kMaxWidth * kHeight);
    core->setVideoBuffer(core, (mColor*)pixels.data(), kMaxWidth);
    std::vector<uint32_t> frame240((size_t)GBA_VIDEO_HORIZONTAL_PIXELS * kHeight), wideOut; // EXPAND's in and out
    int wideOutW = 0;
    if (!core->loadROM(core, vf))
    {
        core->deinit(core);
        fprintf(stderr, "[gba] %s isn't a ROM mGBA can load\n", romPath.c_str());
        return false;
    }
    if (o.save.empty()) MKDIR(o.saves.c_str());
    const std::string savePath = o.save.empty() ? o.saves + "/" + key + ".sav" : o.save;
    mCoreLoadSaveFile(core, savePath.c_str(), false);

    SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    int winW = o.w > 0 ? o.w : 1280, winH = o.h > 0 ? o.h : 720;
    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (o.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_Window* window = SDL_CreateWindow(name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH, flags);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : nullptr;
    SDL_Texture* texture = nullptr;
    auto makeTexture = [&]() {
        if (texture) SDL_DestroyTexture(texture);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, scaling == "smooth" ? "1" : "0");
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, kMaxWidth, kHeight);
    };
    if (renderer) makeTexture();
    if (!texture)
    {
        fprintf(stderr, "[gba] no window: %s\n", SDL_GetError());
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        mCoreConfigDeinit(&core->config);
        core->deinit(core);
        return false;
    }
    // The menu, the messages and the FPS counter: Fulldex's canvas over the game
    Canvas canvas;
    SDL_Texture* overlay = nullptr;
    int overlayW = 0, overlayH = 0;

    // Sound: the core's rate converted to the device's
    SDL_AudioSpec want{}, have{};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    SDL_AudioStream* stream = nullptr;
    unsigned streamRate = 0;
    if (audio) SDL_PauseAudioDevice(audio, 0);

    SDL_GameController* pad = nullptr;
    for (int i = 0; i < SDL_NumJoysticks() && !pad; i++)
        if (SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);

    int extra = -1;
    auto applyShape = [&]() {
        int w = 0, h = 0;
        SDL_GetRendererOutputSize(renderer, &w, &h);
        const int want = profile ? ExtraFor(shape, w, h) : expand ? std::min(PK_WIDE_MAX_MARGIN, ExtraFor(shape, w, h)) : 0;
        if (want == extra) return;
        extra = want;
        if (expand) return; // (mGBA stays at 240: EXPAND draws the sides)
        mCoreConfigSetIntValue(&core->config, "gba.wideExtra", extra);
        core->reloadConfigOption(core, "gba.wideExtra", nullptr);
    };
    applyShape();
    core->reset(core);

    AchievementsHost* ra = o.achievements;
    const bool raOn = ra && ra->begin && ra->begin(ra->ud, original.data(), original.size(), core, ReadMemory);

    // Test hooks: quit after a number of frames (or loops, menu open), save a frame every 300, tap A now and then,
    // open the menu ("600ra": its achievements page).
    const char* testFrames = getenv("GBAW_TEST_FRAMES");
    const char* testDump = getenv("GBAW_TEST_DUMP");
    const bool testPress = getenv("GBAW_TEST_PRESS") != nullptr;
    const char* testMenu = getenv("GBAW_TEST_MENU");
    const long testLimit = testFrames ? atol(testFrames) : 0;
    const long testMenuAt = testMenu ? atol(testMenu) : -1;
    const bool testMenuRa = testMenu && strstr(testMenu, "ra");
    // GBAW_TEST_KEYS="<frame>:<KEY>[+<KEY>][:<frames held>],...", GBAW_TEST_DUMP_EVERY=<frames> (default 300)
    struct TestKey { long at, hold; uint32_t keys; };
    std::vector<TestKey> testKeys;
    if (const char* s = getenv("GBAW_TEST_KEYS"))
    {
        static const char* const names[] = { "A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN", "R", "L" };
        std::string all = s;
        for (size_t at = 0; at < all.size();)
        {
            size_t end = all.find(',', at);
            if (end == std::string::npos) end = all.size();
            const std::string item = all.substr(at, end - at);
            at = end + 1;
            TestKey k{ atol(item.c_str()), 4, 0 };
            const size_t c1 = item.find(':'), c2 = item.find(':', c1 + 1);
            if (c1 == std::string::npos) continue;
            const std::string keyList = item.substr(c1 + 1, c2 == std::string::npos ? std::string::npos : c2 - c1 - 1);
            if (c2 != std::string::npos) k.hold = atol(item.c_str() + c2 + 1);
            for (size_t p = 0; p <= keyList.size();)
            {
                size_t q = keyList.find('+', p);
                if (q == std::string::npos) q = keyList.size();
                for (int i = 0; i < 10; i++)
                    if (keyList.compare(p, q - p, names[i]) == 0) k.keys |= 1u << i;
                p = q + 1;
            }
            testKeys.push_back(k);
        }
    }
    const char* testEvery = getenv("GBAW_TEST_DUMP_EVERY");
    const long testDumpEvery = std::max(1L, testEvery ? atol(testEvery) : 300L);
    const char* testFrom = getenv("GBAW_TEST_DUMP_FROM"); // only frames from this one on
    // GBAW_TEST_RAM=<frame>,...: EWRAM and IWRAM written to <dump prefix>-ram-<frame>-{ewram,iwram}.bin (finding addresses)
    std::vector<long> testRam;
    if (const char* s = getenv("GBAW_TEST_RAM"))
        for (const char* p = s; *p;)
        {
            testRam.push_back(atol(p));
            while (*p && *p != ',') p++;
            if (*p) p++;
        }
    const long testDumpFrom = testFrom ? atol(testFrom) : 1;
    long frameCount = 0, loops = 0;

    // Pause menu state
    enum { PAGE_NONE, PAGE_MAIN, PAGE_RA } page = PAGE_NONE;
    Menu menu;
    std::vector<Row> rows;
    std::string toast;
    Uint32 toastUntil = 0;
    std::string fpsText;
    bool startHeld = false, selectHeld = false;

    std::vector<int16_t> samples;
    const double frameTime = 1.0 / kFps;
    const Uint64 freq = SDL_GetPerformanceFrequency();
    double next = (double)SDL_GetPerformanceCounter() / freq;
    Uint32 fpsStart = SDL_GetTicks();
    int fpsFrames = 0;
    bool running = true;
    auto openMenu = [&]() {
        page = PAGE_MAIN;
        menu.open = true;
        menu.selected = 0;
        menu.scroll = 0;
        core->setKeys(core, 0);
        if (audio) SDL_ClearQueuedAudio(audio);
    };
    auto closeMenu = [&]() {
        page = PAGE_NONE;
        menu.open = false;
        saveSettings();
        next = (double)SDL_GetPerformanceCounter() / freq;
    };
    while (running)
    {
        MenuInput in;
        bool toggle = false;
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN && !e.key.repeat)
            {
                const SDL_Scancode s = e.key.keysym.scancode;
                if (s == SDL_SCANCODE_ESCAPE) toggle = true;
                else if (s == SDL_SCANCODE_F11 || (s == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT)))
                    SDL_SetWindowFullscreen(window, (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                else if (s == SDL_SCANCODE_UP) in.up = true;
                else if (s == SDL_SCANCODE_DOWN) in.down = true;
                else if (s == SDL_SCANCODE_LEFT) in.left = true;
                else if (s == SDL_SCANCODE_RIGHT) in.right = true;
                else if (s == SDL_SCANCODE_RETURN || s == SDL_SCANCODE_X) in.accept = true;
                else if (s == SDL_SCANCODE_Z || s == SDL_SCANCODE_BACKSPACE) in.back = true;
            }
            else if (e.type == SDL_CONTROLLERBUTTONDOWN)
            {
                const bool swap = SwapAB(pad, byLabel);
                switch (e.cbutton.button)
                {
                case SDL_CONTROLLER_BUTTON_GUIDE: toggle = true; break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP: in.up = true; break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: in.down = true; break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: in.left = true; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: in.right = true; break;
                case SDL_CONTROLLER_BUTTON_A: (swap ? in.back : in.accept) = true; break;
                case SDL_CONTROLLER_BUTTON_B: (swap ? in.accept : in.back) = true; break;
                case SDL_CONTROLLER_BUTTON_START: startHeld = true; break;
                case SDL_CONTROLLER_BUTTON_BACK: selectHeld = true; break;
                default: break;
                }
                // Start and Select together, as the DS games' + and -
                if (startHeld && selectHeld) toggle = true;
            }
            else if (e.type == SDL_CONTROLLERBUTTONUP)
            {
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_START) startHeld = false;
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) selectHeld = false;
            }
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) applyShape();
            else if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(e.cdevice.which);
            else if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad
                     && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)))
            {
                SDL_GameControllerClose(pad);
                pad = nullptr;
            }
        }
        if (testMenuAt >= 0 && loops == testMenuAt) toggle = true;
        loops++;
        if (toggle)
        {
            if (page == PAGE_NONE)
            {
                openMenu();
                if (testMenuRa && loops == testMenuAt + 1 && ra) page = PAGE_RA;
            }
            else if (page == PAGE_RA) { page = PAGE_MAIN; menu.selected = 0; menu.scroll = 0; }
            else closeMenu();
        }

        if (page != PAGE_NONE)
        {
            // Paused: the menu takes the buttons
            if (in.back)
            {
                if (page == PAGE_RA) { page = PAGE_MAIN; menu.selected = 0; menu.scroll = 0; }
                else closeMenu();
            }
            int dir = 0;
            const int hit = page == PAGE_NONE ? -1 : menu.Update(in, dir);
            if (hit >= 0 && page == PAGE_MAIN && hit < (int)rows.size())
            {
                switch (rows[hit])
                {
                case ROW_RESUME: closeMenu(); break;
                case ROW_WIDE:
                    shape = Step(kShapes, shape, dir);
                    applyShape();
                    break;
                case ROW_SCALING:
                    scaling = Step(kScalings, scaling, dir);
                    makeTexture();
                    break;
                case ROW_VOLUME: volume = std::max(0, std::min(100, volume + (dir < 0 ? -10 : 10))); break;
                case ROW_FPS: showFps = !showFps; break;
                case ROW_FF: ffSpeed = dir < 0 ? (ffSpeed <= 2 ? 8 : ffSpeed - 1) : (ffSpeed >= 8 ? 2 : ffSpeed + 1); break;
                case ROW_RA:
                    if (dir == 0)
                    {
                        page = PAGE_RA;
                        menu.selected = 0;
                        menu.scroll = 0;
                    }
                    break;
                case ROW_RESET:
                    if (dir == 0)
                    {
                        core->reset(core);
                        if (expand) pk_wide_reset();
                        if (raOn && ra->reset) ra->reset(ra->ud);
                        closeMenu();
                    }
                    break;
                case ROW_QUIT:
                    if (dir == 0) running = false;
                    break;
                }
            }
            else if (hit == 0 && page == PAGE_RA && dir == 0)
            {
                page = PAGE_MAIN;
                menu.selected = 0;
                menu.scroll = 0;
            }
            if (raOn && ra->pump) ra->pump(ra->ud, 1);
        }
        else
        {
            const Uint8* kb = SDL_GetKeyboardState(nullptr);
            const bool fast = kb[SDL_SCANCODE_TAB] || (pad && SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000);
            uint32_t keys = KeyboardKeys(kb) | PadKeys(pad, byLabel);
            if (testPress && frameCount % 120 >= 60 && frameCount % 120 < 64) keys |= 1u << KEY_A;
            for (const TestKey& k : testKeys)
                if (frameCount >= k.at && frameCount < k.at + k.hold) keys |= k.keys;
            core->setKeys(core, keys);

            const int runs = fast ? ffSpeed : 1;
            for (int i = 0; i < runs; i++)
            {
                core->runFrame(core);
                if (raOn && ra->frame) ra->frame(ra->ud);
                if (expand)
                {
                    for (int y = 0; y < kHeight; y++)
                        memcpy(&frame240[(size_t)y * GBA_VIDEO_HORIZONTAL_PIXELS], &pixels[(size_t)y * kMaxWidth],
                               GBA_VIDEO_HORIZONTAL_PIXELS * 4);
                    if (i + 1 < runs) pk_wide_skip(core, frame240.data()); // (fast forward: frames not shown)
                    else
                    {
                        wideOutW = GBA_VIDEO_HORIZONTAL_PIXELS + 2 * std::max(0, extra);
                        wideOut.resize((size_t)wideOutW * kHeight);
                        pk_wide_frame(core, frame240.data(), wideOut.data(), std::max(0, extra), nullptr);
                    }
                }
            }
            frameCount += runs;
            fpsFrames += runs;
            if (raOn && ra->pump) ra->pump(ra->ud, 0);

            // Sound, only at normal speed
            struct mAudioBuffer* buffer = core->getAudioBuffer(core);
            const size_t avail = mAudioBufferAvailable(buffer);
            samples.resize(avail * 2);
            const size_t got = mAudioBufferRead(buffer, samples.data(), avail);
            const unsigned rate = core->audioSampleRate(core);
            if (audio && !fast && got)
            {
                if (!stream || rate != streamRate)
                {
                    if (stream) SDL_FreeAudioStream(stream);
                    stream = SDL_NewAudioStream(AUDIO_S16SYS, 2, (int)rate, have.format, have.channels, have.freq);
                    streamRate = rate;
                }
                if (volume != 100)
                    for (size_t i = 0; i < got * 2; i++) samples[i] = (int16_t)(samples[i] * volume / 100);
                if (stream && SDL_AudioStreamPut(stream, samples.data(), (int)(got * 4)) == 0)
                {
                    std::vector<uint8_t> conv(SDL_AudioStreamAvailable(stream));
                    const int n = SDL_AudioStreamGet(stream, conv.data(), (int)conv.size());
                    // Keep at most about 0.15 s queued so the sound stays with the picture
                    if (n > 0 && SDL_GetQueuedAudioSize(audio) < (Uint32)(have.freq * 4 * 15 / 100))
                        SDL_QueueAudio(audio, conv.data(), (Uint32)n);
                }
            }
        }

        char msg[256];
        if (raOn && ra->pop && ra->pop(ra->ud, msg, sizeof msg))
        {
            toast = msg;
            toastUntil = SDL_GetTicks() + 4000;
        }

        unsigned w = 0, h = 0;
        core->currentVideoSize(core, &w, &h);
        const uint32_t* show = pixels.data();
        int showPitch = kMaxWidth;
        if (expand && wideOutW)
        {
            w = (unsigned)wideOutW;
            show = wideOut.data();
            showPitch = wideOutW;
        }
        if (testDump && page == PAGE_NONE && frameCount % testDumpEvery == 0 && frameCount >= testDumpFrom && frameCount > 0)
        {
            const std::string path = std::string(testDump) + "-" + std::to_string(frameCount) + ".ppm";
            if (FILE* f = fopen(path.c_str(), "wb"))
            {
                fprintf(f, "P6\n%u %u\n255\n", w, h);
                for (unsigned y = 0; y < h; y++)
                    for (unsigned x = 0; x < w; x++)
                    {
                        const uint32_t c = show[(size_t)y * showPitch + x];
                        const unsigned char rgb[3] = { (unsigned char)c, (unsigned char)(c >> 8), (unsigned char)(c >> 16) };
                        fwrite(rgb, 1, 3, f);
                    }
                fclose(f);
            }
        }
        if (testDump && page == PAGE_NONE)
            for (long at : testRam)
                if (frameCount == at)
                {
                    const struct { const char* name; uint32_t base, size; } areas[] = { { "ewram", 0x02000000, 0x40000 }, { "iwram", 0x03000000, 0x8000 } };
                    for (const auto& a : areas)
                        if (FILE* f = fopen((std::string(testDump) + "-ram-" + std::to_string(at) + "-" + a.name + ".bin").c_str(), "wb"))
                        {
                            std::vector<uint8_t> buf(a.size);
                            for (uint32_t i = 0; i < a.size; i++) buf[i] = (uint8_t)core->busRead8(core, a.base + i);
                            fwrite(buf.data(), 1, buf.size(), f);
                            fclose(f);
                        }
                }
        if (testLimit && (frameCount >= testLimit || loops >= 2 * testLimit)) running = false;

        const SDL_Rect area = { 0, 0, (int)w, (int)h };
        SDL_UpdateTexture(texture, &area, show, showPitch * 4);
        int outW = 0, outH = 0;
        SDL_GetRendererOutputSize(renderer, &outW, &outH);
        const SDL_Rect src = { 0, 0, (int)w, (int)h };
        const SDL_Rect dst = Place(outW, outH, (int)w, (int)h, scaling);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, &src, &dst);

        // The overlay: menu, message and FPS counter
        canvas.Resize(outW, outH);
        canvas.Clear();
        const int scale = std::max(1, outH / 240);
        if (page == PAGE_MAIN)
        {
            rows.clear();
            menu.items.clear();
            auto add = [&](Row r, const std::string& label, const std::string& value = "", int slider = -1) {
                rows.push_back(r);
                MenuItem it;
                it.label = label;
                it.value = value;
                it.slider = slider;
                menu.items.push_back(it);
            };
            add(ROW_RESUME, "RESUME");
            if (profile || expand) add(ROW_WIDE, "WIDESCREEN", Upper(shape));
            add(ROW_SCALING, "SCALING", ScalingLabel(scaling));
            add(ROW_VOLUME, "VOLUME", std::to_string(volume) + "%", volume / 10);
            add(ROW_FPS, "FPS COUNTER", showFps ? "ON" : "OFF");
            add(ROW_FF, "FAST FORWARD", std::to_string(ffSpeed) + "X");
            if (raOn) add(ROW_RA, "ACHIEVEMENTS");
            add(ROW_RESET, "RESET GAME");
            add(ROW_QUIT, "QUIT");
            menu.selected = std::min(menu.selected, (int)menu.items.size() - 1);
            menu.Draw(canvas, name, profile || expand ? "ESC OR B: BACK TO THE GAME" : "NO WIDESCREEN FOR THIS ROM");
        }
        else if (page == PAGE_RA)
        {
            menu.items.clear();
            MenuItem back;
            back.label = "BACK";
            menu.items.push_back(back);
            std::vector<std::string> notes(1);
            const int n = ra->count ? ra->count(ra->ud) : 0;
            for (int i = 0; i < n; i++)
            {
                char title[128] = {}, description[256] = {}, progress[64] = {};
                unsigned points = 0;
                int unlocked = 0;
                if (!ra->at(ra->ud, i, title, sizeof title, description, sizeof description, progress, sizeof progress, &points, &unlocked)) continue;
                MenuItem it;
                it.label = Fit(Upper(title), 20);
                it.value = unlocked ? "UNLOCKED" : std::to_string(points) + " POINTS";
                it.info = true;
                menu.items.push_back(it);
                notes.push_back(Upper(description) + (progress[0] ? "  " + std::string(progress) : ""));
            }
            menu.selected = std::min(menu.selected, (int)menu.items.size() - 1);
            char line[256] = {};
            if (!(ra->account && ra->account(ra->ud, line, sizeof line))) snprintf(line, sizeof line, "NOT LOGGED IN");
            char gameLine[256] = {};
            const std::string title = ra->game && ra->game(ra->ud, gameLine, sizeof gameLine) ? Upper(gameLine) : "ACHIEVEMENTS";
            menu.Draw(canvas, Fit(title, 30), Upper(line), menu.selected > 0 ? notes[menu.selected] : "", 260);
        }
        if (!toast.empty() && SDL_GetTicks() < toastUntil)
        {
            const std::string t = Fit(Upper(toast), 60);
            const int tw = Canvas::TextWidth(t, scale) + 12 * scale;
            const int ty = outH - 18 * scale;
            canvas.Fill((outW - tw) / 2, ty, tw, 13 * scale, 0xE0101010);
            canvas.Text((outW - tw) / 2 + 6 * scale, ty + 3 * scale, t, scale, 0xFFFFD75E);
        }
        if (showFps && !fpsText.empty())
            canvas.Text(outW - Canvas::TextWidth(fpsText, scale) - 4 * scale, 4 * scale, fpsText, scale, 0xFFFFFFFF);
        if (canvas.Dirty())
        {
            if (!overlay || overlayW != outW || overlayH != outH)
            {
                if (overlay) SDL_DestroyTexture(overlay);
                overlay = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, outW, outH);
                SDL_SetTextureBlendMode(overlay, SDL_BLENDMODE_BLEND);
                overlayW = outW;
                overlayH = outH;
            }
            if (overlay)
            {
                SDL_UpdateTexture(overlay, nullptr, canvas.px.data(), outW * 4);
                SDL_RenderCopy(renderer, overlay, nullptr, nullptr);
            }
        }
        // (Test: our window's picture once while the menu is open; only this window is read.)
        static bool menuShot = false;
        if (testDump && page != PAGE_NONE && !menuShot && loops > testMenuAt + 2)
        {
            menuShot = true;
            std::vector<uint32_t> shot((size_t)outW * outH);
            if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, shot.data(), outW * 4) == 0)
                if (FILE* f = fopen((std::string(testDump) + "-menu.ppm").c_str(), "wb"))
                {
                    fprintf(f, "P6\n%d %d\n255\n", outW, outH);
                    for (uint32_t c : shot)
                    {
                        const unsigned char rgb[3] = { (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c };
                        fwrite(rgb, 1, 3, f);
                    }
                    fclose(f);
                }
        }
        SDL_RenderPresent(renderer);

        if (SDL_GetTicks() - fpsStart >= 1000)
        {
            fpsText = std::to_string(fpsFrames) + " FPS";
            fpsStart = SDL_GetTicks();
            fpsFrames = 0;
        }

        next += frameTime;
        const double now = (double)SDL_GetPerformanceCounter() / freq;
        if (next > now) SDL_Delay((Uint32)((next - now) * 1000));
        else if (now - next > 0.25) next = now;
    }

    saveSettings();
    if (stream) SDL_FreeAudioStream(stream);
    if (audio) SDL_CloseAudioDevice(audio);
    if (pad) SDL_GameControllerClose(pad);
    if (overlay) SDL_DestroyTexture(overlay);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    mCoreConfigDeinit(&core->config);
    core->deinit(core);
    return true;
}

}
