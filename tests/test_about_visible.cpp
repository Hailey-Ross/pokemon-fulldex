#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../frontend/launcher.h"
#include <cassert>

// Stub services from pages not visited by this rendering test.
const LauncherFontInfo kLauncherFonts[] = {{"noto", "Noto", "Sans", nullptr, nullptr, 1}};
const int kLauncherFontCount = 1;
namespace CloudSaves { Settings LoadSettings(const std::string&) { return {}; } }
namespace HubLink { Status Snapshot() { return {}; } }

namespace ThemePack {
bool Jpeg(const uint32_t*, int, int, int, std::vector<unsigned char>&) { return false; }
bool ZipRead(const std::string&, std::map<std::string, std::vector<unsigned char>>&, size_t, const std::string&) { return false; }
}
namespace Achievements {
bool HttpRequest(const HttpCall&, int&, std::string&, std::string&) { return false; }
}

int main()
{
    Launcher::PhoneUi() = true;
    for (const auto size : {std::pair<int,int>{720,1280}, {1024,1366}, {540,720}})
    {
      for (int section=0; section<5; ++section) {
        Canvas canvas; canvas.Resize(size.first,size.second);
        auto& f=Launcher::Focus(); f=Launcher::GlassFocus{};
        f.page=Launcher::SIDE_ABOUT; f.side=-1; f.prow=section;
        Launcher::Orient(canvas); const auto sp=Launcher::Spots(canvas);
        // Panel shadows may darken the navigation area; text and QR codes must not brighten it.
        std::fill(canvas.px.begin(),canvas.px.end(),0xFF123456);
        Launcher::DrawPage(canvas,sp,true,Launcher::GU(canvas));
        for(int y=sp.navY;y<canvas.h;y++)
            for(int x=canvas.w/4;x<canvas.w*3/4;x++) if((canvas.px[y*canvas.w+x]&0xFFFFFF)>0x123456) { fprintf(stderr,"size %d,%d pixel %d,%d nav %d pdY %d\n",canvas.w,canvas.h,x,y,sp.navY,f.pdY); abort(); }
        assert(f.pdY>0);
        const int maxScroll=f.dMax;
        if(maxScroll>0) {
            assert(Launcher::PageMove(2)); assert(f.pact==0);
            assert(Launcher::PageMove(4)); assert(f.prow==section && f.dScroll>0);
            assert(Launcher::PageMove(1)); assert(f.pact==-1);
        }
        f.dScroll=maxScroll;
        Launcher::DrawPage(canvas,sp,true,Launcher::GU(canvas));
        assert(f.dMax==maxScroll); assert(f.dScroll==maxScroll);
        for(const auto& link:Launcher::AboutLinks()) assert(link.y>=f.pdY && link.y+link.h<=sp.navY);
      }
    }
}
