// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "core/Paths.h"

#include <windows.h>
#include <shlobj.h>

namespace of::paths {

namespace {

std::filesystem::path KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    std::filesystem::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &raw))) out = raw;
    CoTaskMemFree(raw);
    return out;
}

// %OPENFENCES_DATA_DIR% wins; otherwise portable mode keeps data in a "Data" folder next to the exe.
std::filesystem::path Override() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"OPENFENCES_DATA_DIR", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return std::filesystem::path(buf);
    if (IsPortable()) return ExePath().parent_path() / L"Data";
    return {};
}

}  // namespace

bool IsPortable() {
    std::error_code ec;
    return std::filesystem::exists(ExePath().parent_path() / kPortableMarker, ec);
}

std::filesystem::path RoamingDir() {
    if (auto o = Override(); !o.empty()) return o;
    return KnownFolder(FOLDERID_RoamingAppData) / L"openFences";
}

std::filesystem::path LocalDir() {
    if (auto o = Override(); !o.empty()) return o / L"local";
    return KnownFolder(FOLDERID_LocalAppData) / L"openFences";
}

std::filesystem::path ConfigFile() { return RoamingDir() / L"config.json"; }
std::filesystem::path BackupsDir() { return RoamingDir() / L"Backups"; }
std::filesystem::path LogsDir() { return LocalDir() / L"logs"; }
std::filesystem::path CrashesDir() { return LocalDir() / L"Crashes"; }

std::filesystem::path ExePath() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    return std::filesystem::path(std::wstring(buf, n));
}

std::filesystem::path TemplatesDir() { return ExePath().parent_path() / L"templates"; }

}  // namespace of::paths
