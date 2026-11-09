#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
static int phase, waits, calls;
static SDL_Joystick* virtual_pad;
int SDL_WaitEventTimeout(SDL_Event* event, int timeout)
{
    int (*real_wait)(SDL_Event*, int) = dlsym(RTLD_NEXT, "SDL_WaitEventTimeout");
    if (calls++ < 2) return real_wait(event, timeout);
    const char* sequence = getenv("SSR_TEST_INPUT_SEQUENCE");
    const int separate = sequence && !strcmp(sequence, "separate");
    const int modal = sequence && !strcmp(sequence, "modal");
    const int custom = sequence && !strcmp(sequence, "custom-direction");
    const int underlay = sequence && !strcmp(sequence, "modal-underlay");
    if ((custom && waits < 3) || (underlay && waits < 14))
    {
        phase = 2;
        SDL_zero(*event);
        event->type = SDL_USEREVENT;
        if (waits == 0)
        {
            const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
            if (index < 0 || !SDL_IsGameController(index)) abort();
            virtual_pad = SDL_JoystickOpen(index);
            if (!virtual_pad) abort();
        }
        else if (waits == 1)
        {
            SDL_JoystickSetVirtualButton(virtual_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1);
            SDL_JoystickUpdate();
        }
        else if ((!underlay && waits == 2) || (underlay && waits == 12))
        {
            SDL_JoystickSetVirtualButton(virtual_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0);
            SDL_JoystickUpdate();
            if (underlay)
            {
                event->type = SDL_CONTROLLERBUTTONDOWN;
                event->cbutton.button = SDL_CONTROLLER_BUTTON_B;
                event->cbutton.state = SDL_PRESSED;
            }
        }
        if ((!underlay && waits == 2) || (underlay && waits == 13))
        {
            event->type = SDL_KEYDOWN;
            event->key.keysym.sym = SDLK_f;
            event->key.keysym.scancode = SDL_SCANCODE_F;
            event->key.state = SDL_PRESSED;
        }
        ++waits;
        return 1;
    }
    if (!custom && !underlay && (!waits || ((separate || modal) && waits == 1)))
    {
        const int accept = modal && waits == 1;
        ++waits;
        phase = (separate || accept) ? 2 : 1;
        SDL_zero(*event);
        if (accept)
        {
            event->type = SDL_CONTROLLERBUTTONDOWN;
            event->cbutton.button = SDL_CONTROLLER_BUTTON_A;
            event->cbutton.state = SDL_PRESSED;
        }
        else
        {
            event->type = SDL_KEYDOWN;
            event->key.keysym.sym = modal ? SDLK_DOWN : SDLK_f;
            event->key.keysym.scancode = modal ? SDL_SCANCODE_DOWN : SDL_SCANCODE_F;
            event->key.state = SDL_PRESSED;
        }
        return 1;
    }
    return real_wait(event, timeout);
}
int SDL_PollEvent(SDL_Event* event)
{
    if (phase == 1)
    {
        phase = 2;
        const char* sequence = getenv("SSR_TEST_INPUT_SEQUENCE");
        SDL_zero(*event);
        event->type = SDL_CONTROLLERBUTTONDOWN;
        event->cbutton.button = sequence && !strcmp(sequence, "modal") ? SDL_CONTROLLER_BUTTON_DPAD_DOWN : SDL_CONTROLLER_BUTTON_X;
        event->cbutton.state = SDL_PRESSED;
        return 1;
    }
    int (*real_poll)(SDL_Event*) = dlsym(RTLD_NEXT, "SDL_PollEvent");
    return real_poll(event);
}
