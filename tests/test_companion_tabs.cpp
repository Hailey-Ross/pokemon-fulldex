#include "companion_tabs.h"
#include "test_pad.h"
#include <cassert>
#include <cstdio>
int main(){
    int tab=2,restore=-1;bool in=false;
    CompanionTabs::Battle(true,in,tab,restore);assert(tab==4&&restore==2);
    CompanionTabs::Next(tab,restore);assert(tab==5&&restore==-1);
    CompanionTabs::Battle(true,in,tab,restore);assert(tab==5);
    CompanionTabs::Battle(false,in,tab,restore);assert(tab==5);
    CompanionTabs::Next(tab,restore);assert(tab==0);
    CompanionTabs::Battle(true,in,tab,restore);assert(tab==4&&restore==0);
    CompanionTabs::Battle(false,in,tab,restore);assert(tab==0&&restore==-1);
    // The real remapper reserves L3 for a user binding instead of changing companion tabs.
    PadState pad;pad.btn[PAD_LS]=true;
    Controls::Bindings b;
    assert(RemapPad(pad,b,true).btn[PAD_LS]);
    b.SetPad(Controls::A,Controls::IN_LS,true);
    const auto remapped=RemapPad(pad,b,true);
    assert(!remapped.btn[PAD_LS]&&remapped.btn[PAD_A]);
    b.Reset();b.SetPad(Controls::SAVE,Controls::IN_LS,true);
    assert(!RemapPad(pad,b,true).btn[PAD_LS]);
    puts("Companion: manual battle tabs retained, auto restore works, custom L3 bindings take priority");
}
