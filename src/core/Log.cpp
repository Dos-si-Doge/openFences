// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "core/Log.h"

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <mutex>

#include "core/Paths.h"
#include "core/Strings.h"

namespace of::log {

namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
bool g_verbose = false;
}  // namespace

void InitFile(const std::filesystem::path& path, bool verbose) {
    std::lock_guard lock(g_mutex);
    g_verbose = verbose;
    if (g_file) return;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    // Keep one previous log; roll when the current one passes 2 MB.
    if (std::filesystem::exists(path, ec) && std::filesystem::file_size(path, ec) > 2 * 1024 * 1024) {
        auto old = path;
        old.replace_extension(L".1" + path.extension().wstring());
        std::filesystem::rename(path, old, ec);
    }
    // Shared so the watchdog and a restarted instance can append to the same file.
    g_file = _wfsopen(path.c_str(), L"ab", _SH_DENYNO);
}

void Init(bool verbose) { InitFile(paths::LogsDir() / L"openFences.log", verbose); }

bool Verbose() { return g_verbose; }

void Write(const std::wstring& line) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t stamp[64];
    swprintf_s(stamp, L"%04u-%02u-%02u %02u:%02u:%02u.%03u [%lu] ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
               st.wSecond, st.wMilliseconds, GetCurrentThreadId());
    std::string text = ToUtf8(std::wstring(stamp) + line) + "\r\n";
    std::lock_guard lock(g_mutex);
    if (g_file) {
        fwrite(text.data(), 1, text.size(), g_file);
        fflush(g_file);
    }
#ifdef _DEBUG
    OutputDebugStringW((std::wstring(stamp) + line + L"\n").c_str());
#endif
}

}  // namespace of::log
