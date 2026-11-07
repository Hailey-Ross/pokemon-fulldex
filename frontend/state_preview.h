#pragma once
#include <SDL.h>
#include <algorithm>
#include <string>

class StatePreview
{
    SDL_Texture* texture=nullptr;
    std::string current;
public:
    void Clear(){if(texture)SDL_DestroyTexture(texture);texture=nullptr;current.clear();}
    void Draw(SDL_Renderer* renderer,const std::string& file,int outputW,int outputH,int menuX,int menuY)
    {
        if(current!=file)
        {
            Clear();current=file;
            if(SDL_Surface* image=SDL_LoadBMP(file.c_str()))
            {if(image->w==256&&image->h==384)texture=SDL_CreateTextureFromSurface(renderer,image);SDL_FreeSurface(image);}
        }
        if(!texture)return;
        SDL_Rect destination{};
        if(menuX>140)
        {
            destination.w=std::min(menuX-30,outputW/4);destination.h=destination.w*3/2;
            if(destination.h>outputH-30){destination.h=outputH-30;destination.w=destination.h*2/3;}
            destination.x=(menuX-destination.w)/2;destination.y=(outputH-destination.h)/2;
        }
        else if(menuY>160)
        {
            destination.w=std::min(outputW/2,(menuY-30)*2/3);destination.h=destination.w*3/2;
            destination.x=(outputW-destination.w)/2;destination.y=(menuY-destination.h)/2;
        }
        if(destination.w>0&&destination.h>0)SDL_RenderCopy(renderer,texture,nullptr,&destination);
    }
};
