// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <ole2.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "app/Controller.h"
#include "app/Ipc.h"
#include "app/Watchdog.h"
#include "core/Autostart.h"

using namespace of;

namespace {

// Verbs forwarded to an already-running instance.
const wchar_t* const kRemoteVerbs[] = {L"settings", L"welcome", L"toggle-hide", L"hide", L"show", L"toggle-lock", L"peek", L"desktop-display",
                                       L"next-page", L"prev-page", L"page", L"new-fence", L"apply-rules", L"snapshot", L"restore",
                                       L"export", L"import", L"apply-template", L"automatic-layout", L"refresh", L"shutdown", L"exit"};

bool IsRemoteVerb(const std::wstring& v) {
    for (const wchar_t* r : kRemoteVerbs)
        if (v == r) return true;
    return false;
}

void PrintUsage() {
    MessageBoxW(nullptr,
                L"openFences [option]\n\n"
                L"  --settings [page]       open Configure openFences\n"
                L"  --welcome [page]        open Welcome to openFences (built-in guide)\n"
                L"  --toggle-hide | --hide | --show\n"
                L"  --toggle-lock\n"
                L"  --peek | --desktop-display\n"
                L"  --next-page | --prev-page | --page N\n"
                L"  --new-fence\n"
                L"  --apply-rules\n"
                L"  --snapshot [name] | --restore <file>\n"
                L"  --export <file> | --import <file> | --apply-template <file>\n"
                L"  --automatic-layout\n"
                L"  --refresh\n"
                L"  --shutdown\n"
                L"  --no-welcome            don't open Welcome to openFences automatically (scripted deployments)\n"
                L"  --verbose               detailed logging\n",
                L"openFences", MB_ICONINFORMATION);
}

}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    // Never load DLLs from the current directory (DLL planting). The exe's own imports are restricted to System32 by the
    // /DEPENDENTLOADFLAG linker option (CMakeLists.txt).
    SetDllDirectoryW(L"");
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args(argv + 1, argv + argc);
    LocalFree(argv);

    CommandLine cmd;
    DWORD watchdogPid = 0;
    bool unregisterAutostart = false;
    for (size_t i = 0; i < args.size(); ++i) {
        std::wstring a = args[i];
        auto next = [&]() -> std::wstring { return i + 1 < args.size() && args[i + 1].rfind(L"--", 0) != 0 ? args[++i] : L""; };
        if (a == L"--watchdog") watchdogPid = static_cast<DWORD>(_wtoi(next().c_str()));
        else if (a == L"--restarts" || a == L"--restarted") cmd.restarts = _wtoi(next().c_str());
        else if (a == L"--since") cmd.firstRestartTick = _wcstoui64(next().c_str(), nullptr, 10);
        else if (a == L"--autostart") cmd.autostart = true;
        else if (a == L"--unregister-autostart") unregisterAutostart = true;
        else if (a == L"--verbose") cmd.verbose = true;
        else if (a == L"--no-welcome") cmd.noWelcome = true;
        else if (a == L"--help" || a == L"/?") {
            PrintUsage();
            return 0;
        } else if (a.rfind(L"--", 0) == 0 && IsRemoteVerb(a.substr(2))) {
            cmd.verb = a.substr(2);
            cmd.arg = next();
        }
    }

    if (watchdogPid) return watchdog::Run(watchdogPid, cmd.restarts, cmd.firstRestartTick);
    if (unregisterAutostart) {
        // Used by the uninstaller: drop this user's "run at sign-in" entry.
        autostart::Set(false, {});
        return 0;
    }

    HANDLE mutex = CreateMutexW(nullptr, TRUE, ipc::kMutexName);
    bool alreadyRunning = GetLastError() == ERROR_ALREADY_EXISTS;
    if (alreadyRunning) {
        // Forward to the running instance; a plain second launch opens Settings.
        std::wstring verb = cmd.verb.empty() ? (cmd.autostart ? L"" : L"activate") : cmd.verb;
        if (!verb.empty()) ipc::SendCommand(verb, cmd.arg);
        if (mutex) CloseHandle(mutex);
        return 0;
    }
    if (cmd.verb == L"shutdown" || cmd.verb == L"exit") {
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    if (FAILED(OleInitialize(nullptr))) return 1;
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_LINK_CLASS};
    InitCommonControlsEx(&icc);
    SetCurrentProcessExplicitAppUserModelID(L"openFences.openFences");

    int code = Controller::Get().Run(cmd);

    OleUninitialize();
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return code;
}
