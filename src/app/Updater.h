// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>

#include "core/Update.h"

namespace of {
struct Policy;
}

namespace of::updater {

// Automatic updates for MSI installations.
//
// The MSI installs a demand-start Windows service, "openFencesUpdate" (this exe with --update-service, LocalSystem).
// Standard users may start it. openFences decides *when* (at launch and every N days) and starts it; the service
// fetches the latest release, decides with core/Update, downloads the MSI into "<install folder>\Updates" (writable by
// administrators only), verifies it, announces the installation (ResultCode=Installing), starts msiexec detached and
// exits, so the upgrade can replace its exe. Running copies see the announcement, close themselves and come back on
// the new version through StartRelauncher / RunRelauncher below.
//
// Settings and status live in HKLM\SOFTWARE\openFences\Update, which the MSI makes writable for Users:
//   Enabled, InstallImmediately, CheckIntervalDays (DWORD)       user settings (Enabled falls back to DefaultAutoUpdate)
//   LastCheck (QWORD, Unix seconds), ResultCode (DWORD), Result, LatestVersion (REG_SZ), DaysLeft (DWORD)   status

constexpr wchar_t kServiceName[] = L"openFencesUpdate";
constexpr wchar_t kUpdateKey[] = L"SOFTWARE\\openFences\\Update";
constexpr wchar_t kUpgradeCode[] = L"{7B3E9A41-2C5D-4F86-A1E3-9D04C6B8F512}";  // installer/Package.wxs

struct Settings {
    bool enabled = false;
    bool installImmediately = false;
    int intervalDays = update::kDefaultCheckIntervalDays;
};
Settings ReadSettings();
// False when the key can't be written (for example an installation made by an older MSI without the permission).
bool WriteSettings(const Settings& s);

enum class ResultCode : DWORD {
    None = 0,
    UpToDate = 1,
    TooNew = 2,      // a newer release exists but is younger than the minimum age
    Installing = 3,  // msiexec was started; the restarted app confirms
    Installed = 4,
    Failed = 5,
    Available = 6,   // notify-only copies: a newer release exists
};
struct Status {
    int64_t lastCheckUnix = 0;
    ResultCode code = ResultCode::None;
    std::wstring message;        // one sentence, shown in Settings
    std::wstring latestVersion;  // "1.1.1"
    int daysLeft = 0;            // TooNew
};
Status ReadStatus();
bool WriteStatus(const Status& s);

int64_t NowUnix();
std::wstring FeedUrl(const Policy& policy);  // the policy's UpdateFeedUrl, or GitHub's latest-release API

// The service is installed and runs this very exe (a development build next to an installed copy doesn't use it).
bool ServiceUsable();
bool ServiceRunning();
// Starts the service with one of the modes "check" (respects the settings), "check-now" (checks even when automatic
// updates are off) or "install-now" (ignores the minimum release age). Returns false if it couldn't be started.
bool StartService(const wchar_t* mode);

// Downloads and parses the latest release (any thread). On failure returns nullopt and sets error.
std::optional<update::Release> FetchLatest(const std::wstring& feedUrl, std::wstring& error);

// --update-service: the service process entry point.
int RunService();

// Running copies close themselves for an update (the service's msiexec runs in session 0, where Restart Manager can't
// reach user sessions). StartRelauncher copies this exe to %TEMP% (so it doesn't hold the installed file) and runs it
// with --relaunch-after-update; that helper (RunRelauncher) waits until Windows Installer is done and the installed exe
// reports targetVersion, or gives up after a timeout, and then starts the installed exe again in this session.
bool StartRelauncher(const std::wstring& targetVersion);
int RunRelauncher(const std::wstring& installedExe, const std::wstring& targetVersion);
// Deletes leftover helper copies from %TEMP% (best effort; called at startup).
void CleanupRelauncherCopies();

}  // namespace of::updater
