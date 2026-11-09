#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
static int phase;
static int waits;
static int calls;
int SDL_WaitEventTimeout(SDL_Event* event, int timeout) {
    int (*real_wait)(SDL_Event*,int)=dlsym(RTLD_NEXT,"SDL_WaitEventTimeout");
    // Let the initial layout select the first visible synthetic game.
    if (calls++ < 2) return real_wait(event, timeout);
    const char* sequence = getenv("SSR_TEST_INPUT_SEQUENCE");
    const int separate = sequence && !strcmp(sequence, "separate");
    if (!waits || (separate && waits == 1)) {
        waits++;
        phase = separate ? 2 : 1;
        SDL_zero(*event);
        event->type=SDL_KEYDOWN;
        event->key.keysym.sym=SDLK_f;
        event->key.keysym.scancode=SDL_SCANCODE_F;
        event->key.state=SDL_PRESSED;
        return 1;
    }
    return real_wait(event,timeout);
}
int SDL_PollEvent(SDL_Event* event) {
    if(phase==1) {
        phase=2;
        SDL_zero(*event);
        event->type=SDL_CONTROLLERBUTTONDOWN;
        event->cbutton.button=SDL_CONTROLLER_BUTTON_X;
        event->cbutton.state=SDL_PRESSED;
        return 1;
    }
    int (*real_poll)(SDL_Event*)=dlsym(RTLD_NEXT,"SDL_PollEvent");
    return real_poll(event);
}
