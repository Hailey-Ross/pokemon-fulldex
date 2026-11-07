// NOW PLAYING: what the player's own music app (Spotify, YouTube Music in a browser, Media Player...)
// is playing, for the bar under the pause menu's tiles, and its previous / play-pause / next.
//
// Windows: the system's media controls (Windows.Media.Control, the ones the volume pop-up shows),
// reached through WinRT's plain COM interfaces. The compilers Fulldex is built with have no headers
// for them, so the few interfaces used are declared here, by their place in the vtable, as
// Microsoft's metadata lists them (windows-rs, Windows/Media/Control/mod.rs). combase.dll is
// loaded at run time: nothing new to link, and a Windows without it just shows no bar.
// Everything runs on its own thread (the calls are asynchronous and may take a moment); the game
// and the menu only read the latest Info and queue commands.
// Android: NowPlaying.java through JNI (song names need notification access, which the player gives
// in Settings; without it the bar's buttons still work, as media keys).
// Other systems: nothing yet (Info stays inactive), so the bar never shows.
#pragma once
#include <atomic>
#include <chrono>
#include <vector>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace NowPlaying
{
struct Info
{
    bool active = false;  // an app has a media session, playing or paused
    bool playing = false;
    bool canPrev = false, canNext = false;
    std::string title, artist, app;
    // The album art, if the app gives it: ARGB, artW x artH (at most 512 square); artVersion
    // changes with it, so the menu copies it only when it is new.
    std::vector<uint32_t> art;
    int artW = 0, artH = 0;
    unsigned artVersion = 0;
    bool needAccess = false; // Android: no notification access yet, so no song (the buttons still work)
};

#ifdef _WIN32
}
#include <windows.h>
#include <unknwn.h>
#include <objidl.h>
#include <wincodec.h>
namespace NowPlaying
{
namespace Detail
{
typedef struct HSTRING__* HSTR;
typedef HRESULT(WINAPI* RoInitializeFn)(int);
typedef HRESULT(WINAPI* RoGetActivationFactoryFn)(HSTR, REFIID, void**);
typedef HRESULT(WINAPI* WindowsCreateStringFn)(const wchar_t*, UINT32, HSTR*);
typedef HRESULT(WINAPI* WindowsDeleteStringFn)(HSTR);
typedef const wchar_t*(WINAPI* WindowsGetStringRawBufferFn)(HSTR, UINT32*);

// A WinRT method by its place in the object's vtable (0-5 are IUnknown and IInspectable).
template <typename... A>
inline HRESULT Call(void* obj, int slot, A... a)
{
    typedef HRESULT(STDMETHODCALLTYPE * Fn)(void*, A...);
    return (*reinterpret_cast<Fn**>(obj))[slot](obj, a...);
}
inline void Release(void* obj) { if (obj) static_cast<IUnknown*>(obj)->Release(); }

struct Api
{
    RoInitializeFn RoInitialize = nullptr;
    RoGetActivationFactoryFn RoGetActivationFactory = nullptr;
    WindowsCreateStringFn WindowsCreateString = nullptr;
    WindowsDeleteStringFn WindowsDeleteString = nullptr;
    WindowsGetStringRawBufferFn WindowsGetStringRawBuffer = nullptr;
    bool Load()
    {
        HMODULE m = LoadLibraryW(L"combase.dll");
        if (!m) return false;
        RoInitialize = (RoInitializeFn)(void*)GetProcAddress(m, "RoInitialize");
        RoGetActivationFactory = (RoGetActivationFactoryFn)(void*)GetProcAddress(m, "RoGetActivationFactory");
        WindowsCreateString = (WindowsCreateStringFn)(void*)GetProcAddress(m, "WindowsCreateString");
        WindowsDeleteString = (WindowsDeleteStringFn)(void*)GetProcAddress(m, "WindowsDeleteString");
        WindowsGetStringRawBuffer = (WindowsGetStringRawBufferFn)(void*)GetProcAddress(m, "WindowsGetStringRawBuffer");
        return RoInitialize && RoGetActivationFactory && WindowsCreateString && WindowsDeleteString && WindowsGetStringRawBuffer;
    }
    std::string Text(HSTR s)
    {
        if (!s) return "";
        UINT32 n = 0;
        const wchar_t* w = WindowsGetStringRawBuffer(s, &n);
        std::string out;
        if (w && n)
        {
            const int len = WideCharToMultiByte(CP_UTF8, 0, w, (int)n, nullptr, 0, nullptr, nullptr);
            out.resize(len > 0 ? len : 0);
            if (len > 0) WideCharToMultiByte(CP_UTF8, 0, w, (int)n, &out[0], len, nullptr, nullptr);
        }
        WindowsDeleteString(s);
        return out;
    }
};

// IIDs (Windows.Media.Control, and IAsyncInfo).
static const IID IID_ManagerStatics = { 0x2050c4ee, 0x11a0, 0x57de, { 0xae, 0xd7, 0xc9, 0x7c, 0x70, 0x33, 0x82, 0x45 } };
static const IID IID_AsyncInfo = { 0x00000036, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
// Vtable places.
enum
{
    STATICS_REQUEST = 6,                                        // RequestAsync
    MANAGER_CURRENT = 6,                                        // GetCurrentSession
    SESSION_APP = 6, SESSION_PROPERTIES = 7, SESSION_PLAYBACK = 9, // SourceAppUserModelId, TryGetMediaPropertiesAsync, GetPlaybackInfo
    SESSION_NEXT = 16, SESSION_PREV = 17, SESSION_TOGGLE = 20,  // TrySkipNextAsync, TrySkipPreviousAsync, TryTogglePlayPauseAsync
    PROPS_TITLE = 6, PROPS_ARTIST = 9, PROPS_THUMBNAIL = 15,      // Thumbnail: IRandomAccessStreamReference
    STREAMREF_OPEN = 6,                                         // IRandomAccessStreamReference::OpenReadAsync
    PLAYBACK_CONTROLS = 6, PLAYBACK_STATUS = 7,                 // status: 4 playing, 5 paused
    CONTROLS_NEXT = 12, CONTROLS_PREV = 13,                     // IsNextEnabled, IsPreviousEnabled
    ASYNC_RESULTS = 8,                                          // IAsyncOperation<T>::GetResults
    ASYNCINFO_STATUS = 7,                                       // IAsyncInfo::get_Status: 0 started, 1 completed
};

// Waits (up to ms) for an asynchronous operation and takes its result.
inline void* Await(void* op, int ms)
{
    if (!op) return nullptr;
    void* info = nullptr;
    void* result = nullptr;
    if (SUCCEEDED(static_cast<IUnknown*>(op)->QueryInterface(IID_AsyncInfo, &info)) && info)
    {
        int status = 0;
        for (int waited = 0; waited < ms; waited += 5)
        {
            if (FAILED(Call(info, ASYNCINFO_STATUS, &status)) || status != 0) break;
            Sleep(5);
        }
        if (status == 1) Call(op, ASYNC_RESULTS, &result);
        Release(info);
    }
    Release(op);
    return result;
}

// Waits (up to ms) for an operation whose result is not an object (IAsyncOperation<bool>), and lets it go.
inline void AwaitDone(void* op, int ms)
{
    if (!op) return;
    void* info = nullptr;
    if (SUCCEEDED(static_cast<IUnknown*>(op)->QueryInterface(IID_AsyncInfo, &info)) && info)
    {
        int status = 0;
        for (int waited = 0; waited < ms; waited += 5)
        {
            if (FAILED(Call(info, ASYNCINFO_STATUS, &status)) || status != 0) break;
            Sleep(5);
        }
        Release(info);
    }
    Release(op);
}

// The album art: the thumbnail's stream read through WIC (Windows' own image decoders: JPEG, PNG),
// made 32-bit BGRA (in memory the same as the menu's ARGB) at most 512 pixels square.
static const CLSID CLSID_WicFactory = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const IID IID_WicFactory = { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
static const GUID Wic32bppBGRA = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f } };
static const IID IID_Stream = { 0x0000000c, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
typedef HRESULT(WINAPI* CoCreateInstanceFn)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
typedef HRESULT(WINAPI* StreamOverFn)(IUnknown*, REFIID, void**);
inline bool Art(void* props, std::vector<uint32_t>& out, int& w, int& h)
{
    static CoCreateInstanceFn coCreate = (CoCreateInstanceFn)(void*)GetProcAddress(LoadLibraryW(L"ole32.dll"), "CoCreateInstance");
    static StreamOverFn streamOver = (StreamOverFn)(void*)GetProcAddress(LoadLibraryW(L"shcore.dll"), "CreateStreamOverRandomAccessStream");
    if (!coCreate || !streamOver) return false;
    bool ok = false;
    void* ref = nullptr;
    if (FAILED(Call(props, PROPS_THUMBNAIL, &ref)) || !ref) return false;
    void* op = nullptr;
    void* raStream = nullptr;
    if (SUCCEEDED(Call(ref, STREAMREF_OPEN, &op))) raStream = Await(op, 1500);
    Release(ref);
    if (!raStream) return false;
    IStream* stream = nullptr;
    IWICImagingFactory* wic = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* conv = nullptr;
    if (SUCCEEDED(streamOver(static_cast<IUnknown*>(raStream), IID_Stream, (void**)&stream)) && stream &&
        SUCCEEDED(coCreate(CLSID_WicFactory, nullptr, CLSCTX_INPROC_SERVER, IID_WicFactory, (void**)&wic)) && wic &&
        SUCCEEDED(wic->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) && decoder &&
        SUCCEEDED(decoder->GetFrame(0, &frame)) && frame)
    {
        UINT fw = 0, fh = 0;
        frame->GetSize(&fw, &fh);
        if (fw > 0 && fh > 0)
        {
            const UINT big = fw > fh ? fw : fh, side = big > 512 ? 512 : big;
            const UINT sw = fw * side / big ? fw * side / big : 1, sh = fh * side / big ? fh * side / big : 1;
            if (SUCCEEDED(wic->CreateBitmapScaler(&scaler)) && scaler &&
                SUCCEEDED(scaler->Initialize(frame, sw, sh, WICBitmapInterpolationModeFant)) &&
                SUCCEEDED(wic->CreateFormatConverter(&conv)) && conv &&
                SUCCEEDED(conv->Initialize(scaler, Wic32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
            {
                out.assign((size_t)sw * sh, 0);
                if (SUCCEEDED(conv->CopyPixels(nullptr, sw * 4, sw * sh * 4, reinterpret_cast<BYTE*>(out.data()))))
                {
                    w = (int)sw; h = (int)sh;
                    ok = true;
                }
            }
        }
    }
    if (conv) conv->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (wic) wic->Release();
    if (stream) stream->Release();
    Release(raStream);
    if (!ok) out.clear();
    return ok;
}

// The app's name, from its id ("Spotify.exe", "SpotifyAB.SpotifyMusic_...!Spotify", "Chrome"...).
inline std::string AppName(std::string id)
{
    const size_t bang = id.rfind('!');
    if (bang != std::string::npos) id = id.substr(bang + 1);
    if (id.size() > 4 && _stricmp(id.c_str() + id.size() - 4, ".exe") == 0) id.resize(id.size() - 4);
    std::string low = id;
    for (char& ch : low) ch = (char)tolower((unsigned char)ch);
    if (low.find("spotify") != std::string::npos) return "Spotify";
    if (low == "308046b0af4a39cb" || low.find("firefox") != std::string::npos) return "Firefox";
    if (low.find("msedge") != std::string::npos) return "Edge";
    if (low.find("chrome") != std::string::npos) return "Chrome";
    if (low.find("zunemusic") != std::string::npos) return "Media Player";
    if (low.find("itunes") != std::string::npos || low.find("applemusic") != std::string::npos) return "Apple Music";
    if (low.find("vlc") != std::string::npos) return "VLC";
    if (low.rfind("microsoft.", 0) == 0) id = id.substr(10);
    return id;
}
} // namespace Detail

struct Watcher
{
    std::mutex lock;
    Info info;
    std::atomic<int> command{ -1 };     // 0 previous, 1 play / pause, 2 next
    std::atomic<int> wanted{ 0 };        // how often to look: 0 not at all, 1 now and then, 2 often (menu open)
    std::atomic<bool> quit{ false };
    std::thread thread;

    void Run()
    {
        Detail::Api api;
        if (!api.Load()) return;
        api.RoInitialize(1); // (RO_INIT_MULTITHREADED)
        void* manager = nullptr;
        {
            const wchar_t* name = L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";
            Detail::HSTR cls = nullptr;
            void* statics = nullptr;
            if (SUCCEEDED(api.WindowsCreateString(name, (UINT32)wcslen(name), &cls)) &&
                SUCCEEDED(api.RoGetActivationFactory(cls, Detail::IID_ManagerStatics, &statics)) && statics)
            {
                void* op = nullptr;
                if (SUCCEEDED(Detail::Call(statics, Detail::STATICS_REQUEST, &op))) manager = Detail::Await(op, 3000);
                Detail::Release(statics);
            }
            if (cls) api.WindowsDeleteString(cls);
        }
        if (!manager) { fprintf(stderr, "[now playing] the system's media controls are not available\n"); return; }
        int idle = 0, misses = 0;
        std::string artKey;
        std::vector<uint32_t> art;
        int artW = 0, artH = 0, artTries = 0;
        unsigned artVersion = 0;
        while (!quit)
        {
            const int c = command.exchange(-1);
            const int w = wanted;
            if (c < 0 && (w == 0 || idle > 0)) { Sleep(50); if (idle > 0) idle -= 50; continue; }
            idle = w == 2 ? 400 : 2000;
            Info now;
            void* session = nullptr;
            if (SUCCEEDED(Detail::Call(manager, Detail::MANAGER_CURRENT, &session)) && session)
            {
                if (c >= 0)
                {
                    void* op = nullptr;
                    const int slot = c == 0 ? Detail::SESSION_PREV : c == 2 ? Detail::SESSION_NEXT : Detail::SESSION_TOGGLE;
                    // (These give IAsyncOperation<bool>: a yes / no, not an object to release.)
                    if (SUCCEEDED(Detail::Call(session, slot, &op))) Detail::AwaitDone(op, 500);
                    Sleep(150); // (the app takes a moment to change its song or state)
                }
                Detail::HSTR id = nullptr;
                if (SUCCEEDED(Detail::Call(session, Detail::SESSION_APP, &id))) now.app = Detail::AppName(api.Text(id));
                void* playback = nullptr;
                if (SUCCEEDED(Detail::Call(session, Detail::SESSION_PLAYBACK, &playback)) && playback)
                {
                    int status = 0;
                    Detail::Call(playback, Detail::PLAYBACK_STATUS, &status);
                    now.playing = status == 4;
                    now.active = status == 4 || status == 5;
                    void* controls = nullptr;
                    if (SUCCEEDED(Detail::Call(playback, Detail::PLAYBACK_CONTROLS, &controls)) && controls)
                    {
                        boolean next = 0, prev = 0;
                        Detail::Call(controls, Detail::CONTROLS_NEXT, &next);
                        Detail::Call(controls, Detail::CONTROLS_PREV, &prev);
                        now.canNext = next; now.canPrev = prev;
                        Detail::Release(controls);
                    }
                    Detail::Release(playback);
                }
                if (now.active)
                {
                    void* op = nullptr;
                    void* props = nullptr;
                    if (SUCCEEDED(Detail::Call(session, Detail::SESSION_PROPERTIES, &op))) props = Detail::Await(op, 1000);
                    if (props)
                    {
                        Detail::HSTR t = nullptr, a = nullptr;
                        if (SUCCEEDED(Detail::Call(props, Detail::PROPS_TITLE, &t))) now.title = api.Text(t);
                        if (SUCCEEDED(Detail::Call(props, Detail::PROPS_ARTIST, &a))) now.artist = api.Text(a);
                        // (The art read once a song, again a few times if the app gives it a moment later.)
                        const std::string key = now.app + "\n" + now.title + "\n" + now.artist;
                        if (key != artKey) { artKey = key; artTries = 0; art.clear(); artW = artH = 0; artVersion++; }
                        if (art.empty() && artTries < 4)
                        {
                            artTries++;
                            if (Detail::Art(props, art, artW, artH)) artVersion++;
                        }
                        now.art = art; now.artW = artW; now.artH = artH; now.artVersion = artVersion;
                        Detail::Release(props);
                    }
                    if (now.title.empty()) now.active = false; // (a session with nothing in it)
                }
                Detail::Release(session);
            }
            // (While an app changes song or between pause and play it can show no song for a moment:
            // the last one is kept for a few looks, so the bar does not vanish and the tiles jump.)
            if (!now.active && misses < 4)
            {
                std::lock_guard<std::mutex> g(lock);
                if (info.active) { misses++; if (c == 1) info.playing = !info.playing; continue; }
            }
            if (now.active) misses = 0;
            std::lock_guard<std::mutex> g(lock);
            info = now;
        }
        Detail::Release(manager);
    }
};

inline Watcher& Get() { static Watcher w; return w; }
inline void Start() { Watcher& w = Get(); if (!w.thread.joinable()) w.thread = std::thread([&w] { w.Run(); }); }
inline void Stop() { Watcher& w = Get(); w.quit = true; if (w.thread.joinable()) w.thread.join(); }
inline void Want(int how) { Get().wanted = how; }
inline Info Now() { Watcher& w = Get(); std::lock_guard<std::mutex> g(w.lock); return w.info; }
inline void Command(int c) { Get().command = c; }
inline bool Supported() { return true; }
inline void OpenSettings() {}
#elif defined(__ANDROID__)
}
#include <jni.h>
#include <SDL_system.h>
namespace NowPlaying
{
// Android: NowPlaying.java (tools/android/allgames), its static methods through JNI on this
// header's own thread (attached to Java), the class found once on SDL's thread, where the app's
// classes can be found.
struct Watcher
{
    std::mutex lock;
    Info info;
    std::atomic<int> command{ -1 };
    std::atomic<int> wanted{ 0 };
    std::atomic<bool> quit{ false };
    std::thread thread;
    JavaVM* vm = nullptr;
    jclass cls = nullptr;
    jmethodID infoM = nullptr, artM = nullptr, commandM = nullptr, settingsM = nullptr;

    bool Find()
    {
        JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
        if (!env || env->GetJavaVM(&vm) != JNI_OK) return false;
        jclass local = env->FindClass("io/github/mamesj/pokemonds/NowPlaying");
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if (!local) return false;
        cls = (jclass)env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        infoM = env->GetStaticMethodID(cls, "info", "()Ljava/lang/String;");
        artM = env->GetStaticMethodID(cls, "art", "()[I");
        commandM = env->GetStaticMethodID(cls, "command", "(I)V");
        settingsM = env->GetStaticMethodID(cls, "openSettings", "()V");
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        return infoM && artM && commandM && settingsM;
    }
    static bool Failed(JNIEnv* env)
    {
        if (!env->ExceptionCheck()) return false;
        env->ExceptionClear();
        return true;
    }
    void Run()
    {
        JNIEnv* env = nullptr;
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        int idle = 0, misses = 0;
        std::string artKey;
        std::vector<uint32_t> art;
        int artW = 0, artH = 0, artTries = 0;
        unsigned artVersion = 0;
        while (!quit)
        {
            const int c = command.exchange(-1);
            const int w = wanted;
            if (c < 0 && (w == 0 || idle > 0)) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); if (idle > 0) idle -= 50; continue; }
            idle = w == 2 ? 400 : 2000;
            if (c >= 0)
            {
                env->CallStaticVoidMethod(cls, commandM, (jint)c);
                Failed(env);
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
            Info now;
            jstring js = (jstring)env->CallStaticObjectMethod(cls, infoM);
            std::string text;
            if (!Failed(env) && js)
            {
                const char* utf = env->GetStringUTFChars(js, nullptr);
                if (utf) { text = utf; env->ReleaseStringUTFChars(js, utf); }
                env->DeleteLocalRef(js);
            }
            if (text == "!") now.needAccess = true;
            else if (!text.empty())
            {
                std::vector<std::string> f;
                size_t at = 0;
                for (size_t nl; (nl = text.find('\n', at)) != std::string::npos; at = nl + 1) f.push_back(text.substr(at, nl - at));
                f.push_back(text.substr(at));
                if (f.size() >= 6)
                {
                    now.active = true;
                    now.playing = f[0] == "1";
                    now.canPrev = f[1] == "1";
                    now.canNext = f[2] == "1";
                    now.title = f[3]; now.artist = f[4]; now.app = f[5];
                    const std::string key = now.app + "\n" + now.title + "\n" + now.artist;
                    if (key != artKey) { artKey = key; artTries = 0; art.clear(); artW = artH = 0; artVersion++; }
                    if (art.empty() && artTries < 4)
                    {
                        artTries++;
                        jintArray a = (jintArray)env->CallStaticObjectMethod(cls, artM);
                        if (!Failed(env) && a)
                        {
                            const jsize n = env->GetArrayLength(a);
                            std::vector<jint> buf(n > 0 ? n : 0);
                            if (n > 2) env->GetIntArrayRegion(a, 0, n, buf.data());
                            if (n > 2 && buf[0] > 0 && buf[1] > 0 && (jsize)(2 + buf[0] * buf[1]) == n)
                            {
                                artW = buf[0]; artH = buf[1];
                                art.assign(buf.begin() + 2, buf.end());
                                artVersion++;
                            }
                            env->DeleteLocalRef(a);
                        }
                    }
                    now.art = art; now.artW = artW; now.artH = artH; now.artVersion = artVersion;
                }
            }
            // (As on Windows: a moment with no song keeps the last one for a few looks.)
            if (!now.active && !now.needAccess && misses < 4)
            {
                std::lock_guard<std::mutex> g(lock);
                if (info.active) { misses++; if (c == 1) info.playing = !info.playing; continue; }
            }
            if (now.active) misses = 0;
            std::lock_guard<std::mutex> g(lock);
            info = now;
        }
        vm->DetachCurrentThread();
    }
};

inline Watcher& Get() { static Watcher w; return w; }
inline void Start()
{
    Watcher& w = Get();
    if (w.thread.joinable() || !w.Find()) return;
    w.thread = std::thread([&w] { w.Run(); });
}
inline void Stop() { Watcher& w = Get(); w.quit = true; if (w.thread.joinable()) w.thread.join(); }
inline void Want(int how) { Get().wanted = how; }
inline Info Now() { Watcher& w = Get(); std::lock_guard<std::mutex> g(w.lock); return w.info; }
inline void Command(int c) { Get().command = c; }
inline bool Supported() { return true; }
// SHOW SONG NAMES: Android's notification access settings (from SDL's thread, where it is asked).
inline void OpenSettings()
{
    Watcher& w = Get();
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env || !w.cls) return;
    env->CallStaticVoidMethod(w.cls, w.settingsM);
    if (env->ExceptionCheck()) env->ExceptionClear();
}
#else
inline void Start() {}
inline void Stop() {}
inline void Want(int) {}
inline Info Now() { return Info(); }
inline void Command(int) {}
inline bool Supported() { return false; }
inline void OpenSettings() {}
#endif
} // namespace NowPlaying
