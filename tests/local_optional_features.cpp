#include "optional_features.h"
#include <cassert>
#include <sstream>
#include <iostream>
int main(){OptionalFeatures defaults;size_t n;const auto* choices=OptionalFeatures::Options(n);assert(n==6);
 for(size_t i=0;i<n;i++)assert(!(defaults.*choices[i].value));
 assert(defaults.ApplyExtra("extra_library_history","1"));assert(defaults.libraryHistory&&!defaults.liveCompanion);
 assert(!defaults.ApplyExtra("wide","0"));std::ostringstream saved;defaults.SaveExtras(saved);
 OptionalFeatures loaded;std::istringstream input(saved.str());std::string line;
 while(std::getline(input,line)){auto eq=line.find('=');assert(loaded.ApplyExtra(line.substr(0,eq),line.substr(eq+1)));}
 assert(loaded.libraryHistory&&!loaded.quickActions&&!loaded.statePreviews&&!loaded.secondArtwork&&!loaded.gbaMirror);
 assert(loaded.ApplyExtra("extra_library_history","0")&&!loaded.libraryHistory);
 assert(loaded.ApplyExtra("extra_gba_mirror","1\r")&&loaded.gbaMirror);
 std::cout<<"PASS: conservative defaults, independent switches, persistence and unrelated-setting isolation\n";}
