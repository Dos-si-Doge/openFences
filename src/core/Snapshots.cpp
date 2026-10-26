// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "core/Snapshots.h"

#include <windows.h>

#include <algorithm>
#include <nlohmann/json.hpp>

#include "core/Config.h"
#include "core/Strings.h"

namespace of {

namespace fs = std::filesystem;

namespace {
constexpr wchar_t kAutoPrefix[] = L"auto-";
constexpr wchar_t kSnapPrefix[] = L"snap-";

bool IsAutoName(const std::wstring& n) { return n.rfind(kAutoPrefix, 0) == 0 && ExtensionOf(n) == L".json"; }
bool IsSnapName(const std::wstring& n) { return n.rfind(kSnapPrefix, 0) == 0 && ExtensionOf(n) == L".json"; }
}  // namespace

std::vector<std::wstring> AutoBackupsToDelete(std::vector<std::wstring> names, int keep) {
    std::erase_if(names, [](const std::wstring& n) { return !IsAutoName(n); });
    // Names embed YYYYMMDD so lexical order is chronological; newest last.
    std::sort(names.begin(), names.end());
    std::vector<std::wstring> out;
    if (static_cast<int>(names.size()) > keep) out.assign(names.begin(), names.end() - std::max(0, keep));
    return out;
}

std::vector<SnapshotInfo> ListSnapshots(const fs::path& dir) {
    std::vector<SnapshotInfo> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        std::wstring file = entry.path().filename().wstring();
        SnapshotInfo info;
        info.path = entry.path();
        std::wstring stem = entry.path().stem().wstring();
        if (IsAutoName(file)) {
            info.automatic = true;
            info.stamp = stem.substr(wcslen(kAutoPrefix));
            info.name = L"Daily backup";
        } else if (IsSnapName(file)) {
            std::wstring rest = stem.substr(wcslen(kSnapPrefix));
            info.stamp = rest.substr(0, 15);
            info.name = rest.size() > 16 ? rest.substr(16) : L"Snapshot";
            // Prefer the display name stored inside the file.
            if (auto text = ReadFileText(entry.path())) {
                auto j = nlohmann::json::parse(*text, nullptr, false);
                if (j.is_object() && j.contains("snapshot") && j["snapshot"].contains("name"))
                    info.name = FromUtf8(j["snapshot"]["name"].get<std::string>());
            }
        } else {
            continue;
        }
        out.push_back(std::move(info));
    }
    std::sort(out.begin(), out.end(), [](const SnapshotInfo& a, const SnapshotInfo& b) { return a.stamp > b.stamp; });
    return out;
}

std::optional<fs::path> SaveSnapshot(const fs::path& dir, const Config& config, const std::wstring& name, const std::wstring& stamp) {
    auto j = nlohmann::json::parse(SerializeConfig(config));
    j["snapshot"] = {{"name", ToUtf8(name)}, {"created", ToUtf8(stamp)}};
    std::wstring safe = SanitizeFileName(name);
    fs::path path = dir / (std::wstring(kSnapPrefix) + stamp + (safe.empty() ? L"" : L"-" + safe) + L".json");
    if (!WriteFileAtomic(path, j.dump(2))) return std::nullopt;
    return path;
}

bool EnsureDailyBackup(const fs::path& dir, const Config& config, const std::wstring& dateYmd, int keep) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    fs::path today = dir / (std::wstring(kAutoPrefix) + dateYmd + L".json");
    bool created = false;
    if (!fs::exists(today, ec)) created = WriteFileAtomic(today, SerializeConfig(config));
    std::vector<std::wstring> names;
    for (const auto& entry : fs::directory_iterator(dir, ec)) names.push_back(entry.path().filename().wstring());
    for (const auto& n : AutoBackupsToDelete(names, keep)) fs::remove(dir / n, ec);
    return created;
}

std::optional<Config> LoadSnapshot(const fs::path& path) {
    return LoadConfigFile(path);
}

std::wstring CurrentStamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf_s(buf, L"%04u%02u%02u-%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::wstring CurrentDateYmd() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[16];
    swprintf_s(buf, L"%04u%02u%02u", st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::wstring FormatStamp(const std::wstring& s) {
    if (s.size() < 8) return s;
    std::wstring out = s.substr(0, 4) + L"-" + s.substr(4, 2) + L"-" + s.substr(6, 2);
    if (s.size() >= 13 && s[8] == L'-') out += L" " + s.substr(9, 2) + L":" + s.substr(11, 2);
    return out;
}

}  // namespace of
