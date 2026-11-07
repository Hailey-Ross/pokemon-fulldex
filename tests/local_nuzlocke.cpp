#include "nuzlocke_tracker.h"
#include <cassert>
#include <filesystem>
#include <random>
#include <iostream>
int main(){
 namespace fs=std::filesystem;
 auto root=fs::temp_directory_path()/("fulldex-run-"+std::to_string(std::random_device{}()));fs::create_directory(root);
 auto path=(root/"run.txt").string();NuzlockeTracker t;t.Load(path);
 Companion::Snapshot s;s.ready=true;s.map=12;
 Companion::Mon mon;mon.personality=123;mon.species=25;mon.hp=0;s.party.push_back(mon);
 assert(t.Observe(s)&&t.Deaths().empty());assert(t.SetEnabled(true));
 assert(t.Observe(s)&&t.Dead(123)&&t.Deaths().size()==1);assert(t.Observe(s)&&t.Deaths().size()==1);
 mon.personality=124;mon.egg=true;s.party.push_back(mon);assert(t.Observe(s)&&!t.Dead(124));
 s.battlers.resize(2);s.battlers[1].species=16;s.trainerBattle=true;assert(t.Observe(s)&&t.Encounters().empty());
 s.battlers.clear();t.Observe(s);s.battlers.resize(2);s.battlers[1].species=16;s.trainerBattle=false;
 assert(t.Observe(s)&&t.Encounters().at(12)==16);
 s.battlers.clear();t.Observe(s);s.battlers.resize(2);s.battlers[1].species=19;assert(t.Observe(s)&&t.Encounters().at(12)==16);
 NuzlockeTracker loaded;loaded.Load(path);assert(loaded.Enabled()&&loaded.Dead(123)&&loaded.Encounters().at(12)==16);
 assert(loaded.Reset()&&loaded.Deaths().empty()&&loaded.Encounters().empty());
 fs::remove_all(root);std::cout<<"PASS: opt-in, faint deduplication, egg exclusion, trainer exclusion, first-area encounter and persistence\n";
}
