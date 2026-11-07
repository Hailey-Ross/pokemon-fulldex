#include "state_slots.h"
#include "rewind_history.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>
int main()
{
    namespace fs=std::filesystem;
    auto root=fs::temp_directory_path()/("fulldex-slots-"+std::to_string(std::random_device{}()));
    fs::create_directory(root);
    auto path=(root/"quick.state").string();
    auto write=[](const std::string& p,const std::string& value){std::ofstream f(p,std::ios::binary);f<<value;return bool(f);};
    assert(StateSlots::Save(path,[&](const std::string& p){return write(p,"FIRST STATE");}));
    assert(StateSlots::Save(path,[&](const std::string& p){return write(p,"SECOND STATE");}));
    auto read=[](const std::string& p){std::ifstream f(p,std::ios::binary);return std::string((std::istreambuf_iterator<char>(f)),{});};
    assert(read(path)=="SECOND STATE"&&read(path+".previous")=="FIRST STATE");
    assert(!StateSlots::Save(path,[&](const std::string& p){write(p,"FAILED");return false;}));
    assert(read(path)=="SECOND STATE"&&read(path+".previous")=="FIRST STATE");
    assert(!StateSlots::Save(path,[&](const std::string& p){return write(p,"THIRD STATE");})); // Interrupted pending file retained.
    assert(StateSlots::Date(path)!="EMPTY" && StateSlots::Date(path+".missing")=="EMPTY");
    assert(!StateSlots::PreviewCurrent(path));
    assert(write(path+".bmp","SYNTHETIC PREVIEW"));
    auto stateTime=fs::last_write_time(path);
    fs::last_write_time(path+".bmp",stateTime-std::chrono::seconds(2));
    assert(!StateSlots::PreviewCurrent(path));
    fs::last_write_time(path+".bmp",stateTime+std::chrono::seconds(2));
    assert(StateSlots::PreviewCurrent(path));
    RewindHistory history;assert(!history.Push({1,2,3}));history.Limit(6);
    assert(history.Push({1,2,3})&&history.Push({4,5,6})&&history.Bytes()==6);
    assert(history.Push({7,8,9})&&history.Size()==2&&history.Bytes()==6);
    assert(history.Pop()==std::vector<unsigned char>({7,8,9}));
    assert(history.Pop()==std::vector<unsigned char>({4,5,6})&&history.Bytes()==0);
    assert(history.Pop().empty()&&!history.Push(std::vector<unsigned char>(7)));
    history.Push({1,2,3});history.Limit(0);assert(history.Size()==0&&history.Bytes()==0);
    fs::remove_all(root); // Only this test's randomly named synthetic directory.
    std::cout<<"PASS: slot replacement, previous copy, interrupted write refusal, timestamps and bounded rewind eviction\n";
}
