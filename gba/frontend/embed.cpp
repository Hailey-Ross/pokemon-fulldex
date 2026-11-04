// The GBA games inside a Pokemon Fulldex build: finding them, and playing one in its own window.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "embed.h"
#include "wide_rom.h"

#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba/gba/interface.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/vfs.h>

#include <SDL.h>

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

std::string Stem(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

// A ROM's save name and list name: the patch's when it fits, the file's own otherwise.
void KeyAndName(const std::string& path, const std::vector<uint8_t>& rom, std::string& key, std::string& name)
{
    if (const Profile* p = FindProfile(rom))
    {
        key = p->key;
        name = p->name;
        return;
    }
    key = Stem(path);
    name = key;
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

// "labels": the button marked A is A (Nintendo pads swap SDL's positional A/B); "positions": by place.
uint32_t PadKeys(SDL_GameController* pad, bool byLabel)
{
    if (!pad) return 0;
    uint32_t keys = 0;
    auto b = [&](SDL_GameControllerButton btn) { return SDL_GameControllerGetButton(pad, btn) != 0; };
    bool swap = false;
#if SDL_VERSION_ATLEAST(2, 0, 12)
    const SDL_GameControllerType t = SDL_GameControllerGetType(pad);
    swap = byLabel && (t == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO
#if SDL_VERSION_ATLEAST(2, 24, 0)
                       || t == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR
#endif
                       );
#endif
    const bool a = b(swap ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A);
    const bool bb = b(swap ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B);
    const Sint16 lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    const Sint16 ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    auto set = [&](int bit, bool on) { if (on) keys |= 1u << bit; };
    set(KEY_A, a);
    set(KEY_B, bb);
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
        if (n.size() > 4 && Lower(n.substr(n.size() - 4)) == ".gba") files.push_back(roms + "/" + n);
    }
    closedir(d);
    std::sort(files.begin(), files.end());
    for (const std::string& file : files)
    {
        std::vector<uint8_t> rom;
        if (!ReadWhole(file, rom)) continue;
        ListGame g;
        g.file = file;
        KeyAndName(file, rom, g.key, g.name);
        g.hasSave = Exists(saves + "/" + g.key + ".sav");
        out.push_back(std::move(g));
    }
    return out;
}

bool Art(const std::string&, const unsigned char*& jpeg, size_t& size)
{
    jpeg = nullptr;
    size = 0;
    return false;
}

bool Play(const std::string& romPath, Options& o)
{
    std::vector<uint8_t> original;
    if (!ReadWhole(romPath, original))
    {
        fprintf(stderr, "[gba] can't read %s\n", romPath.c_str());
        return false;
    }
    std::string key, name;
    KeyAndName(romPath, original, key, name);
    std::vector<uint8_t> rom = original;
    const Profile* profile = FindProfile(rom);
    if (profile && !ApplyWidePatch(*profile, rom))
    {
        fprintf(stderr, "[gba] the widescreen patch doesn't fit: plays at 3:2\n");
        rom = original;
        profile = nullptr;
    }
    fprintf(stderr, "[gba] %s: %s\n", name.c_str(), profile ? profile->version : "no widescreen for this ROM");

    Settings settings;
    settings.Load(o.data);
    const std::string shape = settings.Get("widescreen", "auto");
    const std::string scaling = settings.Get("scaling", "sharp");
    const bool showFps = settings.Get("show_fps", "0") == "1";
    const int volume = std::max(0, std::min(100, atoi(settings.Get("volume", "100").c_str())));
    const bool byLabel = settings.Get("buttons", "labels") != "positions";
    const int ffSpeed = std::max(2, std::min(8, atoi(settings.Get("fast_forward", "3").c_str())));
    settings.Save();

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
    if (!core->loadROM(core, vf))
    {
        core->deinit(core);
        fprintf(stderr, "[gba] %s isn't a ROM mGBA can load\n", romPath.c_str());
        return false;
    }
    MKDIR(o.saves.c_str());
    const std::string savePath = o.saves + "/" + key + ".sav";
    mCoreLoadSaveFile(core, savePath.c_str(), false);

    SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, scaling == "smooth" ? "1" : "0");
    int winW = o.w > 0 ? o.w : 1280, winH = o.h > 0 ? o.h : 720;
    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (o.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_Window* window = SDL_CreateWindow(name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH, flags);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : nullptr;
    SDL_Texture* texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, kMaxWidth, kHeight) : nullptr;
    if (!texture)
    {
        fprintf(stderr, "[gba] no window: %s\n", SDL_GetError());
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        core->deinit(core);
        return false;
    }

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
        const int want = profile ? ExtraFor(shape, w, h) : 0;
        if (want == extra) return;
        extra = want;
        mCoreConfigSetIntValue(&core->config, "gba.wideExtra", extra);
        core->reloadConfigOption(core, "gba.wideExtra", nullptr);
    };
    applyShape();
    core->reset(core);

    AchievementsHost* ra = o.achievements;
    const bool raOn = ra && ra->begin && ra->begin(ra->ud, original.data(), original.size(), core, ReadMemory);

    // Test hooks: quit after a number of frames, save a frame every 300, tap A now and then.
    const char* testFrames = getenv("GBAW_TEST_FRAMES");
    const char* testDump = getenv("GBAW_TEST_DUMP");
    const bool testPress = getenv("GBAW_TEST_PRESS") != nullptr;
    const long testLimit = testFrames ? atol(testFrames) : 0;
    long frameCount = 0;

    std::vector<int16_t> samples;
    const double frameTime = 1.0 / kFps;
    const Uint64 freq = SDL_GetPerformanceFrequency();
    double next = (double)SDL_GetPerformanceCounter() / freq;
    Uint32 fpsStart = SDL_GetTicks();
    int fpsFrames = 0;
    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false;
            else if (e.type == SDL_KEYDOWN && (e.key.keysym.scancode == SDL_SCANCODE_F11
                     || (e.key.keysym.scancode == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT))))
                SDL_SetWindowFullscreen(window, (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) applyShape();
            else if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(e.cdevice.which);
            else if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad
                     && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)))
            {
                SDL_GameControllerClose(pad);
                pad = nullptr;
            }
        }
        const Uint8* kb = SDL_GetKeyboardState(nullptr);
        const bool fast = kb[SDL_SCANCODE_TAB] || (pad && SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000);
        uint32_t keys = KeyboardKeys(kb) | PadKeys(pad, byLabel);
        if (testPress && frameCount % 120 >= 60 && frameCount % 120 < 64) keys |= 1u << KEY_A;
        core->setKeys(core, keys);

        const int runs = fast ? ffSpeed : 1;
        for (int i = 0; i < runs; i++)
        {
            core->runFrame(core);
            if (raOn && ra->frame) ra->frame(ra->ud);
        }
        frameCount += runs;
        if (testDump && frameCount % 300 == 0)
        {
            unsigned w = 0, h = 0;
            core->currentVideoSize(core, &w, &h);
            const std::string path = std::string(testDump) + "-" + std::to_string(frameCount) + ".ppm";
            if (FILE* f = fopen(path.c_str(), "wb"))
            {
                fprintf(f, "P6\n%u %u\n255\n", w, h);
                for (unsigned y = 0; y < h; y++)
                    for (unsigned x = 0; x < w; x++)
                    {
                        const uint32_t c = pixels[(size_t)y * kMaxWidth + x];
                        const unsigned char rgb[3] = { (unsigned char)c, (unsigned char)(c >> 8), (unsigned char)(c >> 16) };
                        fwrite(rgb, 1, 3, f);
                    }
                fclose(f);
            }
        }
        if (testLimit && frameCount >= testLimit) running = false;
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

        unsigned w = 0, h = 0;
        core->currentVideoSize(core, &w, &h);
        SDL_UpdateTexture(texture, nullptr, pixels.data(), kMaxWidth * 4);
        int outW = 0, outH = 0;
        SDL_GetRendererOutputSize(renderer, &outW, &outH);
        const SDL_Rect src = { 0, 0, (int)w, (int)h };
        const SDL_Rect dst = Place(outW, outH, (int)w, (int)h, scaling);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, &src, &dst);
        SDL_RenderPresent(renderer);

        fpsFrames += runs;
        if (SDL_GetTicks() - fpsStart >= 1000)
        {
            std::string title = name;
            if (showFps) title += "  " + std::to_string(fpsFrames) + " fps";
            char msg[256];
            if (raOn && ra->pop && ra->pop(ra->ud, msg, sizeof msg)) title += "  " + std::string(msg);
            SDL_SetWindowTitle(window, title.c_str());
            fpsStart = SDL_GetTicks();
            fpsFrames = 0;
        }

        next += frameTime;
        const double now = (double)SDL_GetPerformanceCounter() / freq;
        if (next > now) SDL_Delay((Uint32)((next - now) * 1000));
        else if (now - next > 0.25) next = now;
    }

    if (stream) SDL_FreeAudioStream(stream);
    if (audio) SDL_CloseAudioDevice(audio);
    if (pad) SDL_GameControllerClose(pad);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    mCoreConfigDeinit(&core->config);
    core->deinit(core);
    return true;
}

}
