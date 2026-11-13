from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class RomImportConflictTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_rom_conflicts_preserve_files_and_read_errors_are_not_identical(self):
        source=Path(os.environ.get('SSR_IMPORT_SOURCE',ROOT/'frontend/pc_launcher.cpp')).read_text()
        helpers=source[source.index('bool EndsWithCi('):source.index('// iOS puts a ROM opened')]
        helpers=helpers.replace('fread(', 'ReadBlock(').replace('ferror(', 'ReadError(')
        code=r'''
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <cassert>
static bool injectError=false;static FILE* failedFile=nullptr;
static size_t ReadBlock(void* p,size_t s,size_t n,FILE* f){if(injectError&&!failedFile){failedFile=f;return 0;}return fread(p,s,n,f);}
static int ReadError(FILE* f){return f==failedFile?1:ferror(f);}
struct Folders {std::string roms,saves;};
static void MakeDir(const std::string& p){std::filesystem::create_directories(p);}
static bool EndsWithCi(const std::string&,const char*);
namespace Launcher {static bool EndsWithNds(const std::string& s){return EndsWithCi(s,".nds");}}
'''.replace('static bool EndsWithCi','bool EndsWithCi')+helpers+r'''
static void Write(const std::filesystem::path& p,const std::string& s){std::ofstream(p,std::ios::binary)<<s;}
static std::string Read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
 assert(argc==2);namespace fs=std::filesystem;fs::path root=argv[1];
 Folders fo{(root/"roms").string(),(root/"saves").string()};MakeDir(fo.roms);MakeDir(fo.saves);MakeDir((root/"incoming").string());
 Write(root/"roms/Game.gba","OLD-ROM");Write(root/"incoming/Game.gba","NEW-ROM");Write(root/"saves/game.sav","SAVE-SENTINEL");
 assert(!ImportFile(fo,(root/"incoming/Game.gba").string()));assert(Read(root/"roms/Game.gba")=="OLD-ROM");
 assert(Read(root/"incoming/Game.gba")=="NEW-ROM"&&Read(root/"saves/game.sav")=="SAVE-SENTINEL");
 Write(root/"incoming/Game.gba","OLD-ROM");assert(ImportFile(fo,(root/"incoming/Game.gba").string()));
 injectError=true;failedFile=nullptr;assert(!SameContents(root/"incoming/Game.gba",root/"roms/Game.gba"));injectError=false;failedFile=nullptr;
 Write(root/"incoming/Other.nds","OTHER-ROM");assert(ImportFile(fo,(root/"incoming/Other.nds").string()));assert(Read(root/"roms/Other.nds")=="OTHER-ROM");
 Write(root/"incoming/game.sav","NEW-SAVE");assert(ImportFile(fo,(root/"incoming/game.sav").string()));assert(Read(root/"saves/game.sav")=="NEW-SAVE"&&Read(root/"saves/game.sav.before-import.bak")=="SAVE-SENTINEL");
}
'''
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder);cpp=path/'import.cpp';cpp.write_text(code);exe=path/'import'
            subprocess.run(['c++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
            subprocess.run([str(exe),str(path/'data')],check=True)
