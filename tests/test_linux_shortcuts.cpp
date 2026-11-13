#define STB_IMAGE_IMPLEMENTATION
#include "shortcuts.h"
#include <cassert>
#include <fstream>
int main(int argc,char** argv) {
    if(argc>1) {
        std::ofstream out(getenv("FULLDEX_SHORTCUT_ARGS"));
        for(int i=1;i<argc;i++) out<<argv[i]<<'\n';
        return 0;
    }
    Launcher::Game g;g.code="IRBO";g.known=true;g.hasIcon=true;
    std::fill(std::begin(g.icon),std::end(g.icon),0xff4080c0);
    const std::string root=getenv("HOME");
    g.rom=root+"/ROMs/Pokemon %f $cash `tick` \\\"quoted\\\".nds";
    Launcher::Game hack=g;hack.hack=true;hack.name="Blaze Black";
    hack.rom=root+"/Other/Pokemon %f $cash `tick` \\\"quoted\\\".nds";
    Launcher::Game other=g;other.rom=root+"/GBA/Emerald.gba";other.code="BPEE";other.platform=Launcher::GBA;
    std::string where;
    assert(Shortcuts::Make(root,{g,hack,g,other},where)==3);
}
