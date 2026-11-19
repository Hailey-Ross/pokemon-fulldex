// The names the community WFC redirect answers (melonDS/src/net/WfcNames.h).
#include "net/WfcNames.h"

#include <cstdio>

int main()
{
    struct Case { const char* name; bool wfc; };
    const Case cases[] = {
        { "nintendowifi.net", true },
        { "nintendowifi.com", true },
        { "nas.nintendowifi.net", true },
        { "conntest.nintendowifi.net", true },
        { "dwc.nintendowifi.net", true },
        { "gamestats.gs.nintendowifi.net", true },
        { "NAS.NINTENDOWIFI.NET", true },
        { "nintendowifi.net.example.com", false }, // (a different domain that merely starts alike)
        { "evilnintendowifi.net", false },         // (not a label boundary)
        { "nintendo.net", false },
        { "example.com", false },
        { "", false },
    };
    int wrong = 0;
    for (const Case& c : cases)
    {
        if (melonDS::Wfc::IsName(c.name) != c.wfc)
        {
            std::printf("wrong: %s\n", c.name);
            wrong++;
        }
    }
    return wrong ? 1 : 0;
}
