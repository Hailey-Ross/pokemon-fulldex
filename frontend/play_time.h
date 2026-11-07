#pragma once
// PLAY TIME, for the one Support message after 20 hours: seconds played, all games together, in
// fulldex-playtime.txt beside the settings; asked=1 once the message has been shown (never again).
#include <cstdio>
#include <string>

namespace PlayTime
{
struct Data { long long seconds = 0; bool asked = false; };
inline Data Read(const std::string& dir)
{
    Data d;
    if (FILE* f = fopen((dir + "/fulldex-playtime.txt").c_str(), "rb"))
    {
        char line[128];
        while (fgets(line, sizeof line, f))
        {
            long long v = 0;
            int a = 0;
            if (sscanf(line, "seconds=%lld", &v) == 1) d.seconds = v;
            else if (sscanf(line, "asked=%d", &a) == 1) d.asked = a != 0;
        }
        fclose(f);
    }
    return d;
}
inline void Write(const std::string& dir, const Data& d)
{
    if (FILE* f = fopen((dir + "/fulldex-playtime.txt").c_str(), "wb"))
    {
        fprintf(f, "seconds=%lld\nasked=%d\n", d.seconds, d.asked ? 1 : 0);
        fclose(f);
    }
}
inline void Add(const std::string& dir, long long seconds)
{
    if (seconds <= 0 || dir.empty()) return;
    Data d = Read(dir);
    d.seconds += seconds;
    Write(dir, d);
}
// True once: the first time 20 hours have been played on this device.
inline bool AskNow(const std::string& dir)
{
    Data d = Read(dir);
    if (d.asked || d.seconds < 20LL * 3600) return false;
    d.asked = true;
    Write(dir, d);
    return true;
}
}
