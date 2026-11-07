#pragma once
#include <ostream>
#include <string>

// Optional additions preserve the established Fulldex experience until chosen.
struct OptionalFeatures
{
    bool liveCompanion=false,quickActions=false,statePreviews=false;
    bool libraryHistory=false,secondArtwork=false,gbaMirror=false;
    struct Option { const char* key;const char* name;bool OptionalFeatures::* value;const char* help; };
    static const Option* Options(size_t& count)
    {
        static const Option options[]={
            {"extra_live_companion","LIVE COMPANION",&OptionalFeatures::liveCompanion,"UPDATE PARTY, BAG AND DEX DATA WHILE PLAYING. MANUAL COMPANION PAGES ALSO WORK WITH THIS OFF. THE HUD OR COMPANION DISPLAY REQUEST THEIR OWN LIVE DATA."},
            {"extra_quick_actions","QUICK ACTION SHORTCUTS",&OptionalFeatures::quickActions,"SHOW GUARDED GAME ACTIONS IN THE PAUSE MENU FOR VERIFIED VERSIONS. EACH ACTION STILL REQUIRES YOU TO CHOOSE IT."},
            {"extra_state_previews","STATE THUMBNAILS",&OptionalFeatures::statePreviews,"SAVE AND DISPLAY A SMALL PICTURE WITH SAVE STATES. OFF KEEPS EXISTING PICTURES WITHOUT CREATING NEW ONES. SAVE PROTECTION STAYS ACTIVE."},
            {"extra_library_history","RECENTLY PLAYED HISTORY",&OptionalFeatures::libraryHistory,"RECORD GAMES YOU LAUNCH FOR RECENTLY PLAYED SORTING. OFF STOPS NEW RECORDS AND KEEPS EXISTING HISTORY AND FAVOURITES."},
            {"extra_second_artwork","SECOND SCREEN ARTWORK",&OptionalFeatures::secondArtwork,"SHOW THE SELECTED GAME ON AN ENABLED SECOND DISPLAY WHILE THE LAUNCHER IS OPEN. REQUIRES SECOND SCREEN MODE TO BE ENABLED."},
            {"extra_gba_mirror","GBA SECOND SCREEN",&OptionalFeatures::gbaMirror,"MIRROR THE GBA GAME ON AN ENABLED SECOND DISPLAY. PRIMARY CONTROLS REMAIN AVAILABLE. APPLIES WHEN A GBA GAME STARTS."}
        };
        count=sizeof(options)/sizeof(*options);return options;
    }
    bool ApplyExtra(const std::string& key,const std::string& value)
    {
        size_t count;const auto* options=Options(count);
        for(size_t i=0;i<count;i++)if(key==options[i].key){this->*options[i].value=value=="1"||value=="1\r";return true;}
        return false;
    }
    void SaveExtras(std::ostream& out)const
    {
        size_t count;const auto* options=Options(count);
        for(size_t i=0;i<count;i++)out<<options[i].key<<'='<<(this->*options[i].value?1:0)<<'\n';
    }
};
