#include "library_history.h"
#include <cassert>
#include <random>
#include <iostream>
struct Game{std::string rom;int platform=0;bool favourite=false;long long lastPlayed=0;};
int main()
{
    namespace fs=std::filesystem;
    auto root=fs::temp_directory_path()/("fulldex-library-"+std::to_string(std::random_device{}()));fs::create_directory(root);
    std::string path=(root/"history.txt").string();LibraryHistory history;history.Load(path);
    const std::string odd="folder with spaces/game \"special\".nds";
    assert(history.Toggle(odd));LibraryHistory again;again.Load(path);assert(again.Favourite(odd));
    assert(again.Played(odd)&&again.PlayedAt(odd)>0);
    std::vector<Game> games={{"other.nds",0},{odd,0},{"gba.gba",1}};
    again.Apply(games,"original");assert(games[0].rom=="other.nds");
    again.Apply(games,"favourites");assert(games[0].rom==odd&&games[2].platform==1);
    again.Apply(games,"recent");assert(games[0].rom==odd);
    assert(again.Toggle(odd));LibraryHistory final;final.Load(path);assert(!final.Favourite(odd)&&final.PlayedAt(odd)>0);
    fs::remove_all(root);
    std::cout<<"PASS: favourite persistence, quoted paths, recent history, original ordering and console grouping\n";
}
