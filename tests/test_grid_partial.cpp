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
static void Capture(const Canvas& c,const std::string& name){
    if(const char* dir=getenv("SSR_GRID_CAPTURE")){
        std::ofstream f(std::string(dir)+"/"+name+".ppm",std::ios::binary);
        f<<"P6\n"<<c.w<<" "<<c.h<<"\n255\n";
        for(auto p:c.px){char rgb[3]={(char)(p>>16),(char)(p>>8),(char)p};f.write(rgb,3);}
    }
}
int main(){
    using namespace Launcher;
    LookLoad();Look().backdrop="plain";Look().music="off";
    std::vector<Game> games(32);
    for(int i=0;i<(int)games.size();i++){
        auto& g=games[i];g.name="Test Game "+std::to_string(i+1);g.rom="/synthetic/"+std::to_string(i)+".nds";
        g.code="CPUE";g.known=true;g.platform=0;g.art.assign(Art*Art,0xFF306090u+i*0x030201u);
    }
    ContinueRom()=games[0].rom;BackdropRom()=games[0].rom;
    for(int mode=0;mode<4;mode++)for(auto size:{std::pair<int,int>{1280,720},{1280,800},{720,1280},{640,360}}){
        PhoneUi()=mode==2;GlassOn()=mode!=3;Look().layout=mode==1?"panel":"sidebar";
        Focus()=GlassFocus{};
        Canvas c;c.Resize(size.first,size.second);
        auto v=MakeView(c,games,TAB_DS);const auto& g=v.g;
        int initialScroll=0;Draw(c,games,TAB_DS,0,initialScroll,"","",Theme(),false);
        Capture(c,"grid-"+std::to_string(mode)+"-"+std::to_string(c.w)+"x"+std::to_string(c.h));
        // Put a later row halfway through the lower edge, then through the upper edge.
        const int row=std::max(3,(g.bottom-g.top)/std::max(1,g.rowStep)+1);
        const auto spot=v.spots[std::min((int)v.spots.size()-1,g.cols*row)];
        for(bool upper:{false,true})for(bool selected:{false,true}){
            int scroll=upper?spot.y+g.ch/2:spot.y+g.ch/2-(g.bottom-g.top);
            assert(scroll>=0);
            const int chosen=selected?spot.game:0;
            auto draw=[&](uint32_t colour){
                games[spot.game].art.assign(Art*Art,colour);
                Draw(c,games,TAB_DS,chosen,scroll,"","",Theme(),false);
                return c.px;
            };
            auto a=draw(0xFFEE2222),b=draw(0xFF22EE22);
            int changed=0;
            for(int y=0;y<c.h;y++)for(int x=0;x<c.w;x++)if(a[y*c.w+x]!=b[y*c.w+x]){
                assert(y>=g.top&&y<g.bottom);changed++;
            }
            assert(changed>100); // The partly visible cover must actually be rendered.
            const int y=upper?g.top+2:g.bottom-2;
            assert(TileAt(v,scroll,spot.x+g.cw/2,y)==spot.game);
            assert(TileAt(v,scroll,spot.x+g.cw/2,g.top-1)==-1);
            assert(TileAt(v,scroll,spot.x+g.cw/2,g.bottom)==-1);
        }
        int scroll=0;Draw(c,games,TAB_DS,0,scroll,"","",Theme(),false);
        // Controller focus still brings the final card fully into the viewport.
        FollowSelection(v,31,scroll);
        const auto& last=v.spots[v.SpotOf(31)];
        assert(last.y>=scroll&&last.y+g.ch<=scroll+g.bottom-g.top);
    }
    puts("Partial grid cards: visible above/below, clipped, clickable, controller-follow intact across layouts");
}
