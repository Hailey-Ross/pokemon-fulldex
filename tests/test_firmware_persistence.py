from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class FirmwarePersistenceTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_failed_replacement_keeps_existing_firmware(self):
        src=Path(os.environ.get('SSR_FIRMWARE_SOURCE',ROOT/'frontend/platform.cpp')).read_text()
        body=src[src.index('void WriteFirmware('):src.index('void WriteDateTime(')]
        body=body.replace('std::filesystem::rename(', 'FsRename(').replace('rename(', 'Crename(')
        harness=r'''
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
using u8=unsigned char;using u32=unsigned;
namespace Host {std::string FirmwarePath;}
struct Firmware { std::vector<u8> bytes; const u8* Buffer()const{return bytes.data();}u32 Length()const{return bytes.size();}};
static bool failInstall=false,failBackup=false,failRestore=false;
static void FsRename(const std::filesystem::path& a,const std::filesystem::path& b,std::error_code& e){
 if(failInstall)e=std::make_error_code(std::errc::io_error);else std::filesystem::rename(a,b,e);
}
static int Crename(const char* a,const char* b){
 const std::string from=a,to=b;
 if((failInstall&&from==Host::FirmwarePath+".tmp")||(failBackup&&to==Host::FirmwarePath+".old")||(failRestore&&from==Host::FirmwarePath+".old"))return -1;
 return std::rename(a,b);
}
static std::string Read(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
'''+body+r'''
int main(int argc,char**argv){
 assert(argc==2);Host::FirmwarePath=std::string(argv[1])+"/firmware.bin";
 std::filesystem::create_directories(argv[1]);Firmware fw{{'N','E','W'}};
 {std::ofstream f(Host::FirmwarePath);f<<"ORIGINAL";}
 failInstall=true;WriteFirmware(fw,0,0,nullptr);
 assert(Read(Host::FirmwarePath)=="ORIGINAL");
 failInstall=false;WriteFirmware(fw,0,0,nullptr);assert(Read(Host::FirmwarePath)=="NEW");
#ifdef __SWITCH__
 failBackup=true;WriteFirmware(fw,0,0,nullptr);assert(Read(Host::FirmwarePath)=="NEW");failBackup=false;
 failInstall=true;failRestore=true;WriteFirmware(fw,0,0,nullptr);
 assert(Read(Host::FirmwarePath+".old")=="NEW");
 failInstall=false;failRestore=false;WriteFirmware(fw,0,0,nullptr);
 assert(Read(Host::FirmwarePath+".old")=="NEW");
#endif
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'firmware.cpp';cpp.write_text(harness)
            for name,defines in [('desktop',[]),('switch',['-D__SWITCH__'])]:
                exe=Path(folder)/name
                subprocess.run(['c++','-std=c++17',*defines,str(cpp),'-o',str(exe)],check=True)
                subprocess.run([str(exe),str(Path(folder)/(name+'-data'))],check=True)

    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_corrupt_firmware_is_read_with_a_fixed_bound(self):
        source=(ROOT/'frontend/main.cpp').read_text()
        body=source[source.index('static std::vector<uint8_t> ReadFirmwareFile('):source.index('static std::unique_ptr<NDS> MakeConsole(')]
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder); cpp=path/'reader.cpp'
            cpp.write_text(r'''#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <cassert>
'''+body+r'''
int main(int argc,char**argv){
    std::string path=std::string(argv[1])+"/firmware.bin";
    assert(ReadFirmwareFile(path,1024).empty());
    {std::ofstream f(path,std::ios::binary);f<<std::string(1024,'X');}
    assert(ReadFirmwareFile(path,1024)==std::vector<uint8_t>(1024,'X'));
    {std::ofstream f(path,std::ios::binary);f<<std::string(1023,'X');}
    assert(ReadFirmwareFile(path,1024).empty());
    {std::ofstream f(path,std::ios::binary);f.seekp(1024*1024*1024);f.put('X');}
    assert(ReadFirmwareFile(path,1024).empty());
}
''')
            exe=path/'reader'
            subprocess.run(['c++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
            subprocess.run([str(exe),folder],check=True,timeout=5)
