// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "app/CrashHandler.h"

#include <windows.h>
#include <dbghelp.h>

#include <filesystem>

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Snapshots.h"
#include "desktop/DesktopHost.h"

namespace of {

namespace {
LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info) {
    DesktopHost::ForceShowDesktopIcons();
    std::error_code ec;
    auto dir = paths::CrashesDir();
    std::filesystem::create_directories(dir, ec);
    auto path = dir / (L"openFences-" + CurrentStamp() + L".dmp");
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(), info, FALSE};
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                          static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo), &mei, nullptr, nullptr);
        CloseHandle(file);
    }
    log::Write(L"Unhandled exception 0x" + std::to_wstring(info->ExceptionRecord->ExceptionCode) + L"; dump written to " + path.wstring());
    return EXCEPTION_EXECUTE_HANDLER;
}
}  // namespace

void InstallCrashHandler() {
    SetUnhandledExceptionFilter(OnUnhandledException);
}

}  // namespace of
