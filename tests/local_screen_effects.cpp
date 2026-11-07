#include "screen_effect.h"
#include "state_preview.h"
#include <cassert>
#include <cstdio>
#include <iostream>
int main(int argc,char** argv)
{
    assert(ScreenEffect::Pixel(0,2,2)==0);
    assert(ScreenEffect::Pixel(1,0,2)==0x50000000 && ScreenEffect::Pixel(1,0,1)==0);
    assert(ScreenEffect::Pixel(2,2,0)==0x28000000 && ScreenEffect::Pixel(2,1,1)==0);
    SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO)==0);
    auto* surface=SDL_CreateRGBSurfaceWithFormat(0,800,600,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
    auto* renderer=SDL_CreateSoftwareRenderer(surface);assert(renderer);
    SDL_SetRenderDrawColor(renderer,255,255,255,255);SDL_RenderClear(renderer);
    ScreenEffect filter;filter.Draw(renderer,800,600,1);SDL_RenderPresent(renderer);
    auto* pixels=(uint32_t*)surface->pixels;
    assert((pixels[2*800]&255)<(pixels[1*800]&255));filter.Clear();
    StatePreview preview;preview.Draw(renderer,"/missing-file.bmp",800,600,200,200);preview.Clear();
    if(argc>1)SDL_SaveBMP(surface,argv[1]);
    SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    std::cout<<"PASS: scanline/LCD patterns, software-rendered blending and absent-preview fallback\n";
}
