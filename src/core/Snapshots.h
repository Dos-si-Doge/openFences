// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/Model.h"

namespace of {

struct SnapshotInfo {
    std::filesystem::path path;
    std::wstring name;
    std::wstring stamp;  // sortable "YYYYMMDD-HHMMSS" (daily backups: "YYYYMMDD")
    bool automatic = false;
};

// Newest first.
std::vector<SnapshotInfo> ListSnapshots(const std::filesystem::path& dir);
std::optional<std::filesystem::path> SaveSnapshot(const std::filesystem::path& dir, const Config& config, const std::wstring& name,
                                                  const std::wstring& stamp);
// Creates today's automatic backup if missing, then prunes so only `keep` automatic backups remain.
bool EnsureDailyBackup(const std::filesystem::path& dir, const Config& config, const std::wstring& dateYmd, int keep);
std::optional<Config> LoadSnapshot(const std::filesystem::path& path);

// Pure helper: given automatic-backup file names, returns the ones to delete to keep the newest `keep`.
std::vector<std::wstring> AutoBackupsToDelete(std::vector<std::wstring> fileNames, int keep);

std::wstring CurrentStamp();    // local time "YYYYMMDD-HHMMSS"
std::wstring CurrentDateYmd();  // local date "YYYYMMDD"
// "20261024-153000" -> "2026-10-24 15:30".
std::wstring FormatStamp(const std::wstring& stamp);

}  // namespace of
