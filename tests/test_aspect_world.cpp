#include "gpucompose.h"
#include <cassert>
#include <cstdio>

static std::vector<uint32_t> Draw(bool gpu, int w, int h, float aspect, const Scene& scene,
                                  const ScreenImage screens[2], const WideImage& world)
{
    g_mergedAspect = aspect;
    std::vector<uint32_t> pixels;
    if (!gpu) MergedCompose(scene, screens, w, h, pixels, false, &world);
    else
    {
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        assert(surface);
        SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);assert(renderer);
        GpuCompositor compositor(renderer);
        compositor.Draw(scene, screens, w, h, false, &world);
        pixels.resize((size_t)w*h);
        assert(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(), w*4)==0);
        SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);
    }
    MergedBlackBars(pixels, w, h);
    return pixels;
}

int main()
{
    assert(SDL_Init(0)==0);
    std::vector<uint32_t> ds(256*192, 0xFF000000);
    std::vector<uint8_t> flags(256*192, LAYER_3D);
    ScreenImage screens[2]={{ds.data(),flags.data()},{ds.data(),flags.data()}};
    Scene scene;scene.wide=true;scene.items={FullScreen(0)};
    // A nonuniform, opaque world image makes horizontal stretching observable.
    std::vector<uint32_t> pattern(1024*192);
    for(int y=0;y<192;y++)for(int x=0;x<1024;x++)
        pattern[y*1024+x]=0xFF000000u|((x/8)%256<<16)|((y%192)<<8)|(x%251);
    WideImage world;world.pixels=pattern.data();world.w=1024;world.h=192;
    const int h=384;
    for(bool gpu:{false,true})for(float aspect:{16.f/9,16.f/10,3.f/2})
    {
        const int selected=(int)(h*aspect+0.5f);
        const auto reference=Draw(gpu,selected,h,aspect,scene,screens,world);
        for(int w:{896,1280}) // 21:9 and 32:9, same selected view and pixel scale
        {
            const auto actual=Draw(gpu,w,h,aspect,scene,screens,world);
            const int left=(w-selected)/2;
            for(int y=0;y<h;y++)for(int x=0;x<w;x++)
                assert(actual[y*w+x]==(x<left||x>=left+selected?0xFF000000u:reference[y*selected+x-left]));
            // A transparent left half also checks backdrop/coverage coordinates.
            std::vector<uint32_t> half=pattern;
            for(int y=0;y<192;y++)std::fill(half.begin()+y*1024,half.begin()+y*1024+512,0);
            WideImage coverage=world;coverage.pixels=half.data();
            const auto v=MergedMakeView(w,h);
            const auto rv=MergedMakeView(selected,h);
            for(int sx=0;sx<256;sx++)assert(MergedWideCovers(coverage,v,w,h,sx,96)==
                MergedWideCovers(coverage,rv,selected,h,sx,96));
        }
    }
    // Auto still fills its output, and placed battle images retain their base-pixel mapping.
    for(bool gpu:{false,true})
    {
        const auto a=Draw(gpu,896,h,0,scene,screens,world);
        assert(a[h/2*896+10]!=0xFF000000u);
        world.placed=true;world.baseX=-128;
        const auto b=Draw(gpu,896,h,16.f/9,scene,screens,world);
        assert(b[h/2*896+448]!=0xFF000000u);
        world.placed=false;
    }
    SDL_Quit();puts("Fixed aspects: CPU and SDL world mapping matches unstretched view at 21:9 and 32:9");
}
