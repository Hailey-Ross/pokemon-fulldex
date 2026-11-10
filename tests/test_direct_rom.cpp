#define STB_IMAGE_IMPLEMENTATION
#include "direct_rom.h"
#include <cassert>
#include <fstream>
int main(int argc,char** argv) {
    assert(argc==2);namespace fs=std::filesystem;fs::path root=argv[1];
    fs::create_directories(root/"other");fs::create_directories(root/"saves");
    auto make=[&](const fs::path& path,const char* code){std::vector<char>b(4096);std::copy(code,code+4,b.begin()+12);std::ofstream f(path,std::ios::binary);f.write(b.data(),b.size());};
    make(root/"Game.nds","CPUE");make(root/"other/Game.nds","CPUE");
    {std::ofstream f(root/"saves/platinum.sav");f<<"existing save";}
    std::vector<Launcher::Game> games;
    assert(DirectRom::Ds((root/"Game.nds").string(),(root/"saves").string(),root.string(),games)==0);
    assert(DirectRom::Ds((root/"other/../Game.nds").string(),(root/"saves").string(),root.string(),games)==0);
    assert(DirectRom::Ds((root/"other/Game.nds").string(),(root/"saves").string(),root.string(),games)==1);
    assert(games.size()==2&&games[0].rom!=games[1].rom&&games[1].folder==(root/"saves").string());
    assert(DirectRom::Ds((root/"missing.nds").string(),root.string(),root.string(),games)==-1);
    make(root/"Unknown.nds","XXXX");assert(DirectRom::Ds((root/"Unknown.nds").string(),root.string(),root.string(),games)==-1);
    assert(games.size()==2&&fs::file_size(root/"Game.nds")==4096);
    std::ifstream f(root/"saves/platinum.sav");std::string s;std::getline(f,s);assert(s=="existing save");
}
