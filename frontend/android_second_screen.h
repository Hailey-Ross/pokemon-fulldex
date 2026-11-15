#pragma once
#include <algorithm>
#include <cstdint>

namespace DsDisplayState
{
constexpr uint64_t Ready = 1ULL << 32;
constexpr uint64_t AcceptingFrames = 1ULL << 33;
inline int Mode(uint64_t state) { return (state >> 28) & 7; }
inline const char* ModeName(int mode)
{
    return mode == 4 ? "COMPANION" : mode == 3 ? "DS SWAPPED" : mode == 2 ? "DS SPLIT" : mode == 1 ? "EXTRA TOUCH" : "OFF";
}
inline bool Physical(uint64_t state) { return (state & Ready) && (Mode(state) == 2 || Mode(state) == 3); }
inline bool Swapped(uint64_t state) { return (state & Ready) && Mode(state) == 3; }
inline bool Touch(uint64_t state, int& x, int& y)
{
    if (!(state & Ready) || (Mode(state) == 0 || Mode(state) >= 3) || !(state & (1ULL << 24))) return false;
    x = state & 4095;
    y = (state >> 12) & 4095;
    return x < 256 && y < 192;
}
}

#if defined(__ANDROID__) && (defined(SSR_PC_LAUNCHER) || defined(GBAW_EMBEDDED))
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

// Draws the second display's pictures into its surface, on a thread of its own: the game's thread
// only copies the newest picture into a mailbox (a picture not drawn yet is replaced by the next).
// Scaled as the Java fallback and DsScreenGeometry.touch do (fit, centred, nearest pixel), so touches
// land where they did.
class SecondScreenPainter
{
    std::mutex m;
    std::condition_variable cv;
    std::thread worker;
    bool quit = false, pending = false;
    std::vector<uint32_t> next, cur;
    int nextW = 0, nextH = 0;
    ANativeWindow* window = nullptr; // (one reference held here)
    int windowSerial = 0;
    std::vector<int> mapX;
    void Run()
    {
        for (;;)
        {
            ANativeWindow* win = nullptr;
            int w = 0, h = 0, serial = 0;
            {
                std::unique_lock<std::mutex> hold(m);
                cv.wait(hold, [&] { return quit || (pending && window); });
                if (quit) return;
                cur.swap(next);
                w = nextW; h = nextH; pending = false;
                win = window; serial = windowSerial;
                ANativeWindow_acquire(win);
            }
            if (Draw(win, w, h)) { postedW = w; postedH = h; postedSerial = serial; }
            ANativeWindow_release(win);
        }
    }
    bool Draw(ANativeWindow* win, int w, int h)
    {
        if (w <= 0 || h <= 0 || cur.size() < (size_t)w * h) return false;
        if (ANativeWindow_setBuffersGeometry(win, 0, 0, WINDOW_FORMAT_RGBX_8888) != 0) return false;
        ANativeWindow_Buffer buf;
        if (ANativeWindow_lock(win, &buf, nullptr) != 0) return false;
        const int W = buf.width, H = buf.height;
        uint32_t* out = (uint32_t*)buf.bits;
        if (W > 0 && H > 0 && out)
        {
            const float scale = std::min(W / (float)w, H / (float)h);
            const int dw = std::max(1, std::min(W, (int)(w * scale))), dh = std::max(1, std::min(H, (int)(h * scale)));
            const int left = (W - dw) / 2, top = (H - dh) / 2;
            mapX.resize(dw);
            for (int x = 0; x < dw; x++) mapX[x] = std::min(w - 1, (int)(x / scale));
            for (int y = 0; y < H; y++)
            {
                uint32_t* row = out + (size_t)y * buf.stride;
                if (y < top || y >= top + dh) { std::fill(row, row + W, 0xFF000000u); continue; }
                std::fill(row, row + left, 0xFF000000u);
                std::fill(row + left + dw, row + W, 0xFF000000u);
                const uint32_t* src = cur.data() + (size_t)std::min(h - 1, (int)((y - top) / scale)) * w;
                uint32_t* dst = row + left;
                for (int x = 0; x < dw; x++)
                {
                    const uint32_t p = src[mapX[x]]; // (0xAARRGGBB to the surface's R, G, B, X in memory)
                    dst[x] = 0xFF000000u | (p & 0x0000FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
                }
            }
        }
        return ANativeWindow_unlockAndPost(win) == 0;
    }
public:
    std::atomic<int> postedSerial { 0 }, postedW { 0 }, postedH { 0 };
    ~SecondScreenPainter() { Stop(); }
    // A new surface (its reference passes to this), or none.
    void SetWindow(ANativeWindow* win, int serial)
    {
        {
            std::lock_guard<std::mutex> hold(m);
            if (window) ANativeWindow_release(window);
            window = win; windowSerial = win ? serial : 0;
            pending = false;
        }
        if (win && !worker.joinable()) { quit = false; worker = std::thread([this] { Run(); }); }
    }
    bool HasWindow() { std::lock_guard<std::mutex> hold(m); return window != nullptr; }
    void Put(const uint32_t* px, int w, int h)
    {
        {
            std::lock_guard<std::mutex> hold(m);
            if (!window) return;
            next.assign(px, px + (size_t)w * h);
            nextW = w; nextH = h; pending = true;
        }
        cv.notify_one();
    }
    void Stop()
    {
        { std::lock_guard<std::mutex> hold(m); quit = true; }
        cv.notify_one();
        if (worker.joinable()) worker.join();
        std::lock_guard<std::mutex> hold(m);
        if (window) ANativeWindow_release(window);
        window = nullptr; windowSerial = 0; pending = false;
    }
};

// All JNI calls stay on SDL's main thread. Java owns the second window; the pictures go into its
// surface through SecondScreenPainter, or (a Java side without the surface) through a bounded
// latest-frame mailbox Java draws. Java never accesses the emulator or its mutable framebuffer.
class AndroidSecondScreen
{
    JNIEnv* env = nullptr;
    jclass activity = nullptr;
    jintArray frame = nullptr;
    jmethodID stateMethod = nullptr, modeMethod = nullptr, runningMethod = nullptr, frameMethod = nullptr;
    jmethodID surfaceMethod = nullptr, serialMethod = nullptr, shownMethod = nullptr;
    bool running = false;
    int framePixels=256*192;
    SecondScreenPainter painter;
    int surfaceSerial = 0, reportedSerial = 0, reportedW = 0, reportedH = 0;
    bool Failed()
    {
        if (!env || !env->ExceptionCheck()) return false;
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    // The surface, if Java has one up: true when this picture went to it.
    bool ToSurface(const uint32_t* pixels, int width, int height)
    {
        if (!surfaceMethod) return false;
        const jint serial = env->CallStaticIntMethod(activity, serialMethod);
        if (Failed()) { surfaceMethod = nullptr; painter.Stop(); return false; }
        if (serial != surfaceSerial)
        {
            painter.SetWindow(nullptr, 0);
            surfaceSerial = serial; reportedSerial = 0;
            jobject surface = env->CallStaticObjectMethod(activity, surfaceMethod);
            if (Failed()) surface = nullptr;
            if (surface)
            {
                if (ANativeWindow* win = ANativeWindow_fromSurface(env, surface)) painter.SetWindow(win, serial);
                env->DeleteLocalRef(surface);
            }
        }
        if (!painter.HasWindow()) return false;
        painter.Put(pixels, width, height);
        const int posted = painter.postedSerial, pw = painter.postedW, ph = painter.postedH;
        if (posted == surfaceSerial && (reportedSerial != posted || reportedW != pw || reportedH != ph))
        {
            env->CallStaticVoidMethod(activity, shownMethod, (jint)posted, (jint)pw, (jint)ph);
            if (!Failed()) { reportedSerial = posted; reportedW = pw; reportedH = ph; }
        }
        return true;
    }
public:
    explicit AndroidSecondScreen(bool enabled)
    {
        if (!enabled) return;
        env = (JNIEnv*)SDL_AndroidGetJNIEnv();
        if (!env) return;
        jobject host = (jobject)SDL_AndroidGetActivity();
        if (!host) return;
        jclass local = env->GetObjectClass(host);
        activity = (jclass)env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        env->DeleteLocalRef(host);
        if (!activity || Failed()) return;
        stateMethod = env->GetStaticMethodID(activity, "secondScreenState", "()J");
        if (Failed()) return;
        modeMethod = env->GetStaticMethodID(activity, "setSecondScreenMode", "(I)V");
        if (Failed()) return;
        runningMethod = env->GetStaticMethodID(activity, "setDsRunning", "(Z)V");
        if (Failed()) return;
        frameMethod = env->GetStaticMethodID(activity, "submitDsFrame", "([III)V");
        if (Failed()) return;
        // (The surface: optional; without it the pictures go through Java as before.)
        surfaceMethod = env->GetStaticMethodID(activity, "secondScreenSurface", "()Ljava/lang/Object;");
        if (!Failed()) serialMethod = env->GetStaticMethodID(activity, "secondScreenSurfaceSerial", "()I");
        if (serialMethod && !Failed()) shownMethod = env->GetStaticMethodID(activity, "secondScreenShown", "(III)V");
        if (!shownMethod || Failed()) surfaceMethod = serialMethod = shownMethod = nullptr;
        jintArray localFrame = env->NewIntArray(256 * 192);
        if (Failed() || !localFrame) return;
        frame = (jintArray)env->NewGlobalRef(localFrame);
        env->DeleteLocalRef(localFrame);
        if (Failed() || !frame) return;
        env->CallStaticVoidMethod(activity, runningMethod, JNI_TRUE);
        running = !Failed();
    }
    ~AndroidSecondScreen()
    {
        Stop();
        if (env && frame) env->DeleteGlobalRef(frame);
        if (env && activity) env->DeleteGlobalRef(activity);
    }
    void Stop()
    {
        painter.Stop();
        surfaceSerial = reportedSerial = 0;
        if (running) { env->CallStaticVoidMethod(activity, runningMethod, JNI_FALSE); Failed(); }
        running = false;
    }
    uint64_t State()
    {
        if (!running) return 0;
        jlong value = env->CallStaticLongMethod(activity, stateMethod);
        return Failed() ? 0 : (uint64_t)value;
    }
    void SetMode(int mode)
    {
        if (!running) return;
        env->CallStaticVoidMethod(activity, modeMethod, (jint)mode);
        Failed();
    }
    void Submit(const uint32_t* pixels,int width=256,int height=192)
    {
        if (!running || !pixels || width<=0 || height<=0 || width>1024 || height>768 || !(State() & DsDisplayState::AcceptingFrames)) return;
        if (ToSurface(pixels, width, height)) return;
        const int count=width*height;
        if(count!=framePixels)
        {
            jintArray local=env->NewIntArray(count);
            if(Failed()||!local)return;
            if(frame)env->DeleteGlobalRef(frame);
            frame=(jintArray)env->NewGlobalRef(local);env->DeleteLocalRef(local);
            if(Failed()||!frame)return;
            framePixels=count;
        }
        env->SetIntArrayRegion(frame, 0, count, (const jint*)pixels);
        if (Failed()) return;
        env->CallStaticVoidMethod(activity, frameMethod, frame, (jint)width, (jint)height);
        Failed();
    }
};
#else
class AndroidSecondScreen
{
public:
    explicit AndroidSecondScreen(bool) {}
    void Stop() {}
    uint64_t State() { return 0; }
    void SetMode(int) {}
    void Submit(const uint32_t*,int=256,int=192) {}
};
#endif
