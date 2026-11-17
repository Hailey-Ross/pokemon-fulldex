#pragma once
#if defined(__linux__) && !defined(__ANDROID__)
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

namespace LinuxFolderPicker
{
struct Result { bool available = false; std::string path; };

inline std::string Quote(const std::string& text)
{
    std::string out = "'";
    for (char c : text) out += c == '\'' ? "'\\''" : std::string(1, c);
    return out + "'";
}

inline bool Installed(const char* program)
{
    const char* value = getenv("PATH");
    std::string paths = value ? value : "";
    size_t at = 0;
    do
    {
        size_t end = paths.find(':', at);
        std::string dir = paths.substr(at, end == std::string::npos ? end : end - at);
        if (access(((dir.empty() ? "." : dir) + "/" + program).c_str(), X_OK) == 0) return true;
        if (end == std::string::npos) break;
        at = end + 1;
    } while (at <= paths.size());
    return false;
}

inline Result Choose(const std::string& start, const std::string& title)
{
    std::string desktop = getenv("XDG_CURRENT_DESKTOP") ? getenv("XDG_CURRENT_DESKTOP") : "";
    for (char& c : desktop) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    const bool kde = desktop.find("KDE") != std::string::npos || getenv("KDE_FULL_SESSION");
    const char* first = kde ? "kdialog" : "zenity";
    const char* second = kde ? "zenity" : "kdialog";
    const char* program = Installed(first) ? first : Installed(second) ? second : nullptr;
    if (!program) return {};
    Result result; result.available = true;
    const std::string initial = start.empty() ? "." : start;
    std::string command = std::string(program) == "kdialog"
        ? "kdialog --getexistingdirectory " + Quote(initial) + " --title " + Quote(title)
        : "zenity --file-selection --directory --filename=" + Quote(initial.back() == '/' ? initial : initial + "/") + " --title=" + Quote(title);
    if (FILE* pipe = popen((command + " 2>/dev/null").c_str(), "r"))
    {
        char buf[4096];
        while (fgets(buf, sizeof buf, pipe)) result.path += buf;
        const int status = pclose(pipe);
        if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) result.path.clear();
    }
    // Cancellation is final: never open a second dialog after the user presses Cancel.
    if (!result.path.empty() && result.path.back() == '\n') result.path.pop_back();
    return result;
}
}
#endif
