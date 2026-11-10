#pragma once
namespace CompanionTabs
{
inline void Next(int& tab, int& restore)
{
    tab = (tab + 1) % 6;
    restore = -1; // A deliberate choice survives the automatic battle view.
}
inline void Battle(bool inBattle, bool& wasInBattle, int& tab, int& restore)
{
    if (inBattle && !wasInBattle && tab != 4) { restore = tab; tab = 4; }
    if (!inBattle && wasInBattle && restore >= 0) tab = restore;
    if (!inBattle) restore = -1;
    wasInBattle = inBattle;
}
}
