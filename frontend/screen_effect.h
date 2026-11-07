#pragma once
#include <SDL.h>
#include <vector>

// Optional display effects are drawn beneath controls and menus. They do not modify emulation.
class ScreenEffect
{
    SDL_Texture* texture=nullptr;
    int width=0,height=0,kind=-1;
public:
    static uint32_t Pixel(int mode,int x,int y)
    {
        if(mode==1)return y%3==2?0x50000000u:0;
        if(mode==2)return x%3==2||y%3==2?0x28000000u:0;
        return 0;
    }
    void Clear(){if(texture)SDL_DestroyTexture(texture);texture=nullptr;width=height=0;kind=-1;}
    void Draw(SDL_Renderer* renderer,int w,int h,int mode)
    {
        if(!mode||w<=0||h<=0||w>8192||h>8192||(size_t)w*h>16*1024*1024)return;
        if(!texture||width!=w||height!=h||kind!=mode)
        {
            Clear();
            texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,w,h);
            if(!texture)return;
            width=w;height=h;kind=mode;
            std::vector<uint32_t> pixels((size_t)w*h);
            for(int y=0;y<h;y++)for(int x=0;x<w;x++)pixels[(size_t)y*w+x]=Pixel(mode,x,y);
            SDL_UpdateTexture(texture,nullptr,pixels.data(),w*4);
            SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
        }
        SDL_RenderCopy(renderer,texture,nullptr,nullptr);
    }
};
