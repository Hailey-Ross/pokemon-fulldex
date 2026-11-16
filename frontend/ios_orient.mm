// Lets the game list turn with an iPhone or iPad (upright or sideways, as it is held), and
// turns the screen sideways for a game.
// Info.plist allows both (tools/ios/build_all.sh); this asks iOS to use one of them.
// SDL's hint alone does not turn a screen that is already up, which is how the list
// and a game share one app.
#import <UIKit/UIKit.h>
#import <GameController/GameController.h>
#include <SDL.h>
#include <cstdio>
#include "ios_orient.h"

static bool g_landscape = false;
static bool g_free = true; // (the list: any way the device is held)

static UIInterfaceOrientationMask Wanted()
{
    if (g_free)
        return UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad ? UIInterfaceOrientationMaskAll
                                                                                     : UIInterfaceOrientationMaskAllButUpsideDown;
    return g_landscape ? (UIInterfaceOrientationMaskLandscapeLeft | UIInterfaceOrientationMaskLandscapeRight)
                       : UIInterfaceOrientationMaskPortrait;
}

static bool AlreadyThere(UIInterfaceOrientation o)
{
    if (g_free) return true; // (the list: whichever way it is held already will do)
    if (g_landscape) return UIInterfaceOrientationIsLandscape(o);
    return o == UIInterfaceOrientationPortrait;
}

// lock: the screen should stay in this orientation, so tell iOS again even when it
// already faces that way. A later resize does not, or a turn would ask forever.
static void Apply(bool lock)
{
    if (@available(iOS 13.0, *))
    {
        for (UIScene* s in UIApplication.sharedApplication.connectedScenes)
        {
            if (![s isKindOfClass:[UIWindowScene class]]) continue;
            UIWindowScene* scene = (UIWindowScene*)s;
            UIWindow* window = nil;
            for (UIWindow* w in scene.windows)
                if (w.isKeyWindow) { window = w; break; }
            if (!window && scene.windows.count) window = scene.windows.firstObject;
            if (!window) continue;
            const bool there = AlreadyThere(scene.interfaceOrientation);
            if (there && !lock) return;
            UIViewController* vc = window.rootViewController;
            if (@available(iOS 16.0, *))
            {
                // The public way to turn the screen to a chosen orientation.
                if (vc) [vc setNeedsUpdateOfSupportedInterfaceOrientations];
                if (there) return;
                UIWindowSceneGeometryPreferencesIOS* prefs =
                    [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:Wanted()];
                [scene requestGeometryUpdateWithPreferences:prefs errorHandler:^(NSError* err) {
                    fprintf(stderr, "[ios] could not turn the screen: %s\n", err.localizedDescription.UTF8String);
                }];
            }
            else
            {
                // iOS 14 and 15 have no public call that turns the screen against how the
                // device is held. This is the call those versions answer. The app is
                // sideloaded, so it is not going through App Store review.
                if (!there)
                {
                    const UIInterfaceOrientation o = g_landscape ? UIInterfaceOrientationLandscapeRight
                                                                : UIInterfaceOrientationPortrait;
                    [[UIDevice currentDevice] setValue:@(o) forKey:@"orientation"];
                }
                [UIViewController attemptRotationToDeviceOrientation];
            }
            return;
        }
    }
}

static int OnWindow(void*, SDL_Event* e)
{
    // A game's window (the GBA build makes its own) is shown after the hint is set.
    // Deferred: this runs inside SDL, and asking iOS to turn can call back in.
    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SHOWN)
        dispatch_async(dispatch_get_main_queue(), ^{ Apply(false); });
    return 1;
}

static void Set(bool landscape, bool free)
{
    g_landscape = landscape;
    g_free = free;
    SDL_SetHint(SDL_HINT_ORIENTATIONS, free ? "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown"
                                            : landscape ? "LandscapeLeft LandscapeRight" : "Portrait");
    if (SDL_WasInit(SDL_INIT_VIDEO))
    {
        SDL_DelEventWatch(OnWindow, nullptr);
        SDL_AddEventWatch(OnWindow, nullptr);
        Apply(true);
    }
}

void IosOrientLauncher() { Set(false, true); }
void IosOrientGame() { Set(true, false); }

void IosSafeInsets(int* top, int* bottom, int* left, int* right)
{
    *top = *bottom = *left = *right = 0;
    UIWindow* window = nil;
    if (@available(iOS 13.0, *))
    {
        for (UIScene* s in UIApplication.sharedApplication.connectedScenes)
        {
            if (![s isKindOfClass:[UIWindowScene class]]) continue;
            for (UIWindow* w in ((UIWindowScene*)s).windows)
                if (w.isKeyWindow) { window = w; break; }
            if (!window && ((UIWindowScene*)s).windows.count) window = ((UIWindowScene*)s).windows.firstObject;
            if (window) break;
        }
    }
    if (!window) return;
    const UIEdgeInsets in = window.safeAreaInsets;
    *top = (int)ceil(in.top);
    *bottom = (int)ceil(in.bottom * 0.5); // (the home bar: half of it, a line under the buttons is enough)
    *left = (int)ceil(in.left);
    *right = (int)ceil(in.right);
}

// (IosIgnoreControllerPresses.)
static SDL_EventFilter g_nextFilter = nullptr;
static void* g_nextFilterData = nullptr;
static int SDLCALL DropControllerPresses(void*, SDL_Event* e)
{
    if ((e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) && !SDL_IsTextInputActive())
    {
        bool keyboard = false;
        if (@available(iOS 14.0, *)) keyboard = GCKeyboard.coalescedKeyboard != nil;
        if (!keyboard)
            switch (e->key.keysym.scancode)
            {
            case SDL_SCANCODE_UP: case SDL_SCANCODE_DOWN: case SDL_SCANCODE_LEFT: case SDL_SCANCODE_RIGHT:
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_PAUSE:
                return 0;
            default:
                break;
            }
    }
    return g_nextFilter ? g_nextFilter(g_nextFilterData, e) : 1;
}

void IosIgnoreControllerPresses()
{
    SDL_EventFilter current = nullptr;
    void* data = nullptr;
    if (SDL_GetEventFilter(&current, &data) && current == DropControllerPresses) return;
    g_nextFilter = current;
    g_nextFilterData = data;
    SDL_SetEventFilter(DropControllerPresses, nullptr);
}
