#pragma once
// CLOUD SAVES WHILE PLAYING (cloud_saves.h does the work): before the game reads its save, a newer
// one in the cloud is brought down; a little after each time the game writes its save, it goes up
// on a thread of its own (play never waits); at the end, once more. Nothing at all when cloud saves
// are off (cloud.ini beside the settings).
#include <string>

namespace CloudAuto
{
// configDir: where cloud.ini is. What to tell the player ("" for nothing), in capitals.
std::string Start(const std::string& configDir, const std::string& savePath);
void Saved();  // the save file was just written
void Tick();   // once a frame
void Finish(); // at the end: the last send (waits a few seconds at most)
}
