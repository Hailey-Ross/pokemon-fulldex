#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
static int calls, step;
int SDL_WaitEventTimeout(SDL_Event* event, int timeout)
{
    int (*real_wait)(SDL_Event*, int) = dlsym(RTLD_NEXT, "SDL_WaitEventTimeout");
    const int paging = getenv("SSR_PICTURE_PAGE_TEST") != NULL;
    if (calls++ < 2 || step >= (paging ? 12 : 3)) return real_wait(event, timeout);
    SDL_zero(*event);
    if (paging && step == 7)
    {
        // Change page capacity after showing five files, before choosing More Files.
        SDL_Window* window = SDL_GetWindowFromID(1);
        if (!window) abort();
        SDL_SetWindowSize(window, 1280, 480);
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        if (h != 480) abort();
        event->type = SDL_USEREVENT;
    }
    else
    {
        event->type = SDL_CONTROLLERBUTTONDOWN;
        event->cbutton.state = SDL_PRESSED;
        event->cbutton.button = paging ? ((step == 8 || step == 11) ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_DPAD_DOWN)
                                      : (step < 2 ? SDL_CONTROLLER_BUTTON_DPAD_DOWN : SDL_CONTROLLER_BUTTON_A);
    }
    fprintf(stderr, "[picture-input] step %d\n", step);
    ++step;
    return 1;
}
