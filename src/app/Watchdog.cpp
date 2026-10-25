#include "app/Watchdog.h"

#include <string>

#include "core/Log.h"
#include "core/Paths.h"
#include "desktop/DesktopHost.h"

namespace of::watchdog {

namespace {
constexpr int kMaxRestarts = 3;
constexpr ULONGLONG kRestartWindowMs = 5 * 60 * 1000;

bool Launch(const std::wstring& args) {
    std::wstring exe = paths::ExePath().wstring();
    std::wstring cmd = L"\"" + exe + L"\" " + args;
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}
}  // namespace

void Spawn(int restarts, ULONGLONG firstRestartTick) {
    Launch(L"--watchdog " + std::to_wstring(GetCurrentProcessId()) + L" --restarts " + std::to_wstring(restarts) + L" --since " +
           std::to_wstring(firstRestartTick));
}

int Run(DWORD pid, int restarts, ULONGLONG firstRestartTick) {
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return 0;
    WaitForSingleObject(process, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process, &code);
    CloseHandle(process);
    if (code == 0) return 0;  // clean exit: the app already restored the desktop

    log::Init(false);
    log::Info(L"watchdog: openFences (pid {}) exited with 0x{:08x}; restoring desktop icons", pid, code);
    DesktopHost::ForceShowDesktopIcons();

    // Restart only after crashes (NTSTATUS error codes), not when the user killed the process.
    bool crashed = (code & 0xC0000000) == 0xC0000000;
    if (!crashed) return 0;
    ULONGLONG now = GetTickCount64();
    if (firstRestartTick == 0 || now - firstRestartTick > kRestartWindowMs) {
        restarts = 0;
        firstRestartTick = now;
    }
    if (restarts >= kMaxRestarts) {
        log::Info(L"watchdog: too many crashes, not restarting");
        return 0;
    }
    Launch(L"--restarted " + std::to_wstring(restarts + 1) + L" --since " + std::to_wstring(firstRestartTick));
    return 0;
}

}  // namespace of::watchdog
