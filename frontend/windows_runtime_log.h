#pragma once
#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>

namespace WindowsRuntimeLog
{
struct Handle
{
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

// Keep a windowed runtime's stdout/stderr in the launcher's bug-report log too.
// Drain while it runs: waiting first would deadlock once its output fills the pipe.
inline bool Run(const std::wstring& exe, const std::wstring& folder, DWORD& code, FILE* log = stderr)
{
    auto fail = [&](const char* step) {
        const DWORD error = GetLastError();
        std::fprintf(log, "[3ds] %s failed (Windows error %lu)\n", step, (unsigned long)error);
        std::fflush(log);
        return false;
    };
    SECURITY_ATTRIBUTES sa { sizeof sa, nullptr, TRUE };
    Handle rd, wr, input;
    if (!CreatePipe(&rd.value, &wr.value, &sa, 0)) return fail("runtime log pipe");
    if (!SetHandleInformation(rd.value, HANDLE_FLAG_INHERIT, 0)) return fail("runtime log reader");
    input.value = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input.value == INVALID_HANDLE_VALUE) return fail("runtime input");
    STARTUPINFOEXW si {};
    si.StartupInfo.cb = sizeof si;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdOutput = wr.value;
    si.StartupInfo.hStdError = wr.value;
    si.StartupInfo.hStdInput = input.value;
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<unsigned char> attributes(bytes);
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes)) return fail("runtime handle list");
    struct Attributes
    {
        LPPROC_THREAD_ATTRIBUTE_LIST value;
        ~Attributes() { DeleteProcThreadAttributeList(value); }
    } cleanup { si.lpAttributeList };
    // Only the output writer and NUL input cross into the game. In particular the
    // read end and unrelated launcher handles must not keep the pipe open on exit.
    HANDLE inherited[] = { wr.value, input.value };
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   inherited, sizeof inherited, nullptr, nullptr)) return fail("runtime handles");
    std::wstring cmd = L"\"" + exe + L"\"";
    PROCESS_INFORMATION pi {};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE,
                         CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr,
                         folder.c_str(), &si.StartupInfo, &pi)) return fail("runtime start");
    Handle process, thread;
    process.value = pi.hProcess;
    thread.value = pi.hThread;
    CloseHandle(wr.value);
    wr.value = nullptr;
    char buffer[4096];
    DWORD got = 0;
    while (ReadFile(rd.value, buffer, sizeof buffer, &got, nullptr) && got)
    {
        std::fwrite(buffer, 1, got, log);
        std::fflush(log);
    }
    if (WaitForSingleObject(process.value, INFINITE) != WAIT_OBJECT_0) return fail("runtime wait");
    if (!GetExitCodeProcess(process.value, &code)) return fail("runtime exit code");
    return true;
}
}
#endif
