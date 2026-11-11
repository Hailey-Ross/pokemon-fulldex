#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "launcher.h"
#include <cassert>
#include <fstream>
const LauncherFontInfo kLauncherFonts[] = {{"noto", "Noto", "Sans", nullptr, nullptr, 1}};
const int kLauncherFontCount = 1;
namespace CloudSaves { Settings LoadSettings(const std::string&) { return {}; } }
namespace HubLink { Status Snapshot() { return {}; } int Version() { return 0; } }
namespace ThemePack {
bool Jpeg(const uint32_t*, int, int, int, std::vector<unsigned char>&) { return false; }
bool ZipRead(const std::string&, std::map<std::string, std::vector<unsigned char>>&, size_t, const std::string&) { return false; }
bool ZipWrite(const std::string&, const std::vector<std::pair<std::string, std::vector<unsigned char>>>&) { return false; }
}
namespace Achievements { bool HttpRequest(const HttpCall&, int&, std::string&, std::string&) { return false; } }
int main(int argc,char** argv){
    using namespace Launcher;assert(argc==2);HubDir()=argv[1];LookLoad();LauncherMode()=0;
    for(bool phone:{false,true})for(auto size:{std::pair<int,int>{1280,720},{1280,800},{720,1280},{640,360}}){
        PhoneUi()=phone;GlassOn()=true;Focus()=GlassFocus{};Focus().page=SIDE_LAUNCHER;Focus().side=-1;
        Canvas c;c.Resize(size.first,size.second);Look().layout="sidebar";
        auto draw=[&](){c.Clear();DrawPage(c,Spots(c),false,GU(c));};
        draw();auto choices=LayoutPreviews();assert(choices.size()==(PhoneLook()?2:3));
        for(const auto& hit:choices)assert(hit.layout!="panel"); // (kept as a file value only)
        const auto original=c.px;
        for(const auto& hit:choices){
            assert(hit.w>0&&hit.h>0&&hit.x>=0&&hit.x+hit.w<=c.w&&hit.y>=0&&hit.y+hit.h<=c.h);
            Focus().side=SIDE_LAUNCHER;
            assert(LookTap(hit.x+hit.w/2,hit.y+hit.h/2)==1);assert(Look().layout==hit.layout&&Focus().side==-1);
            Look().loaded=false;LookLoad();assert(Look().layout==hit.layout);draw();
            if(hit.layout=="grid")assert(c.px!=original); // Selection outline/caption updates.
        }
        Look().layout="grid";draw();
        if(const char* dir=getenv("SSR_LAYOUT_PREVIEW_CAPTURE")){
            std::ofstream f(std::string(dir)+"/layout-"+(phone?"phone-":"desktop-")+std::to_string(c.w)+"x"+std::to_string(c.h)+".ppm",std::ios::binary);
            f<<"P6\n"<<c.w<<" "<<c.h<<"\n255\n";
            for(auto p:c.px){char rgb[3]={(char)(p>>16),(char)(p>>8),(char)p};f.write(rgb,3);}
        }
        if(PhoneLook()&&c.h>c.w){
            Focus().dScroll=60;draw();
            for(const auto& hit:LayoutPreviews())assert(hit.y>=Focus().pdY);
            assert(!LookTap(choices[0].x+2,Focus().pdY-1));
        }
        Focus().prow=1;draw();assert(LayoutPreviews().empty());
        assert(!LookTap(choices[0].x+2,choices[0].y+2));
    }
    puts("Layout previews: all supported choices, selection, persisted taps and clipped phone hit areas passed");
}
