#include <cassert>
#include <cstdio>
using EGLContext = int;
using EGLDisplay = int;
using EGLSurface = int;
using EGLenum = int;
constexpr int EGL_NO_CONTEXT=0, EGL_NO_DISPLAY=0, EGL_NO_SURFACE=0;
static EGLContext OurContext=0, PrevContext=0, current=0;
static EGLDisplay OurDisplay=0, PrevDisplay=0;
static EGLSurface PrevDraw=0, PrevRead=0;
static EGLenum PrevApi=1, boundApi=0;
static void* SdlWindow=nullptr;
static void* SdlContext=nullptr;
static int flushes=0, changes=0, sdlChanges=0;
static void Flush(){flushes++;}
static void (*glad_glFlush)()=nullptr;
#define glFlush glad_glFlush
static int eglGetCurrentContext(){return current;}
static void eglMakeCurrent(int,int,int,int context){current=context;changes++;}
static void eglBindAPI(int api){boundApi=api;}
static void SDL_GL_MakeCurrent(void*,void* context){current=context?3:0;sdlChanges++;}
#include "context_leave.h"
int main(){
    // Failed CreateContext: GL has never been loaded, and the caller still leaves to restore API state.
    SsrLeaveGLContext();
    SsrReleaseGLContext();
    assert(flushes==0 && changes==0 && boundApi==PrevApi);
    // Partially initialized context with missing GL entry point: detach without dereferencing it.
    OurContext=current=7;OurDisplay=5;PrevContext=9;PrevDisplay=6;
    SsrLeaveGLContext();assert(current==9 && flushes==0);
    // Valid owner context flushes and returns to the previous context.
    glad_glFlush=Flush;current=7;
    SsrLeaveGLContext();assert(flushes==1 && current==9);
    current=7;SsrReleaseGLContext();assert(flushes==2 && current==0);
    // A failed enter must not flush/detach another renderer's context.
    current=11;const int before=changes;SsrReleaseGLContext();assert(current==11 && changes==before && flushes==2);
    // SDL owns restoration when its window/context exists.
    SdlWindow=reinterpret_cast<void*>(1);SdlContext=reinterpret_cast<void*>(3);current=7;
    SsrLeaveGLContext();assert(flushes==3 && sdlChanges==2 && current==3);
    std::puts("Rejected/partial GL contexts fall back safely; valid context flush and EGL/SDL restoration preserved.");
}
