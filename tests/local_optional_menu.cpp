#include "launcher.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <random>
#include <iostream>
int main(){auto file=std::filesystem::temp_directory_path()/("fulldex-extra-menu-"+std::to_string(std::random_device{}())+".cfg");
 {std::ofstream out(file);out<<"wide=1\ncustom_future_setting=keep\n";}
 Launcher::SettingsPage page;page.Open(file.string());assert(page.items.size()>=8);page.Enter(8);assert(page.items.size()==6);
 for(const auto& item:page.items){assert(Launcher::GroupOf(item)==8);assert(page.Shown(item)=="OFF");}
 page.selected=3;page.Change(1);assert(page.Stored("extra_library_history")=="1");
 Launcher::SettingsPage reopened;reopened.Open(file.string());reopened.Enter(8);assert(reopened.Shown(reopened.items[3])=="ON");assert(reopened.Shown(reopened.items[0])=="OFF");
 assert(reopened.Stored("wide")=="1"&&reopened.Stored("custom_future_setting")=="keep");
 reopened.selected=3;reopened.Change(-1);assert(reopened.Stored("extra_library_history")=="0");
 std::filesystem::remove(file);std::cout<<"PASS: launcher extra settings group, independent toggles, reload and existing setting preservation\n";}
