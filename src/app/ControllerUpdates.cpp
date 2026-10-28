// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

// Automatic updates, the openFences side: when to check, and the notify-only path for copies that can't install
// updates. The service side (download, verification, msiexec) is in app/Updater.cpp.

#include <cstdlib>

#include "Version.h"
#include "app/Controller.h"
#include "app/Tray.h"
#include "core/Log.h"
#include "core/Paths.h"

namespace of {

namespace {
update::Version CurrentVersion() { return update::ParseVersion(OF_VERSION_WSTRING).value_or(update::Version{}); }

// Test runs point openFences at a scratch data folder; they must not reach the internet or start the service.
bool TestIsolated() {
    wchar_t buf[8];
    return GetEnvironmentVariableW(L"OPENFENCES_DATA_DIR", buf, 8) > 0;
}
}  // namespace

void Controller::InitUpdates() {
    // Let Windows Installer's Restart Manager restart us after an update closes us (not after crashes or hangs: the
    // watchdog handles those).
    RegisterApplicationRestart(L"--autostart", RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_REBOOT);

    if (m_policy.disableUpdates) m_updateMode = UpdateMode::Disabled;
    else if (!paths::IsPortable() && updater::ServiceUsable()) m_updateMode = UpdateMode::Service;
    else m_updateMode = UpdateMode::NotifyOnly;
    log::Info(L"Updates: {}", m_updateMode == UpdateMode::Service ? L"installed by the update service"
                              : m_updateMode == UpdateMode::NotifyOnly ? L"notify only" : L"disabled by policy");
    updater::CleanupRelauncherCopies();
    if (m_updateMode == UpdateMode::Service) {
        ReconcileUpdateStatus();
        // The service announces an installation before starting it; we then make way for it (UpdateWatchTick). Also
        // when updates are off here: another user may have started one.
        SetTimer(m_msgWnd, kTimerUpdateWatch, 3000, nullptr);
    }
    if (m_updateMode == UpdateMode::Disabled || TestIsolated()) return;
    SetTimer(m_msgWnd, kTimerUpdateLaunch, 60 * 1000, nullptr);  // every launch checks, a minute after start
    SetTimer(m_msgWnd, kTimerUpdate, 60 * 60 * 1000, nullptr);   // then: is a periodic check due?
}

// The service starts msiexec and exits, so it can't report the result itself: the new version (restarted by Restart
// Manager) does, or a later start notices that the installation didn't finish.
void Controller::ReconcileUpdateStatus() {
    updater::Status st = updater::ReadStatus();
    if (st.code != updater::ResultCode::Installing) return;
    auto target = update::ParseVersion(st.latestVersion);
    if (target && !(*target > CurrentVersion())) {
        st.code = updater::ResultCode::Installed;
        st.message = L"openFences was updated to " + st.latestVersion + L".";
    } else if (updater::NowUnix() - st.lastCheckUnix > 30 * 60) {
        st.code = updater::ResultCode::Failed;
        st.message = L"Installing openFences " + st.latestVersion + L" didn't finish. Details: " +
                     (paths::ExePath().parent_path() / L"Updates" / L"install.log").wstring();
    } else {
        return;  // still installing
    }
    log::Info(L"Updates: {}", st.message);
    updater::WriteStatus(st);
}

// The update service is about to replace openFences (ResultCode=Installing, a newer version, announced moments ago):
// close cleanly so the files aren't in use, and leave a helper that starts the new version once it's installed.
void Controller::UpdateWatchTick() {
    if (m_closingForUpdate) return;
    updater::Status st = updater::ReadStatus();
    if (st.code != updater::ResultCode::Installing) return;
    auto target = update::ParseVersion(st.latestVersion);
    if (!target || !(*target > CurrentVersion()) || updater::NowUnix() - st.lastCheckUnix > 15 * 60) return;
    log::Info(L"Updates: openFences {} is being installed; closing and restarting afterwards", st.latestVersion);
    if (!updater::StartRelauncher(st.latestVersion)) return;  // stay running rather than disappear
    m_closingForUpdate = true;
    KillTimer(m_msgWnd, kTimerUpdateWatch);
    Exit();
}

void Controller::AutomaticUpdateCheck() {
    UpdatePrefs prefs = GetUpdatePrefs();
    if (!prefs.automatic) return;
    if (m_updateMode == UpdateMode::Service) updater::StartService(L"check");
    else if (m_updateMode == UpdateMode::NotifyOnly) StartNotifyCheck(false);
}

void Controller::UpdateTimerTick() {
    UpdatePrefs prefs = GetUpdatePrefs();
    if (update::CheckDue(UpdateStatus().lastCheckUnix, prefs.intervalDays, updater::NowUnix())) AutomaticUpdateCheck();
}

Controller::UpdatePrefs Controller::GetUpdatePrefs() const {
    UpdatePrefs p;
    if (m_updateMode == UpdateMode::Service) {
        updater::Settings s = updater::ReadSettings();
        p.automatic = s.enabled;
        p.installImmediately = s.installImmediately;
        p.intervalDays = s.intervalDays;
    } else {
        p.automatic = m_updateMode != UpdateMode::Disabled && m_config.general.updateCheck;
        p.intervalDays = m_config.general.updateIntervalDays;
    }
    return p;
}

bool Controller::SetUpdatePrefs(const UpdatePrefs& prefs) {
    if (m_updateMode == UpdateMode::Disabled) return false;
    if (m_updateMode == UpdateMode::Service) return updater::WriteSettings({prefs.automatic, prefs.installImmediately, prefs.intervalDays});
    m_config.general.updateCheck = prefs.automatic;
    m_config.general.updateIntervalDays = update::NormalizeInterval(prefs.intervalDays);
    SaveSoon();
    return true;
}

void Controller::CheckForUpdatesNow() {
    if (m_updateMode == UpdateMode::Service) updater::StartService(L"check-now");
    else if (m_updateMode == UpdateMode::NotifyOnly) StartNotifyCheck(true);
}

void Controller::InstallUpdateNow() {
    if (m_updateMode == UpdateMode::Service) updater::StartService(L"install-now");
}

bool Controller::UpdateCheckRunning() const {
    if (m_updateMode == UpdateMode::Service) return updater::ServiceRunning();
    return m_updateFetchRunning;
}

updater::Status Controller::UpdateStatus() const {
    if (m_updateMode == UpdateMode::Service) return updater::ReadStatus();
    updater::Status st = m_notifyStatus;
    st.lastCheckUnix = m_config.general.updateLastCheck;
    return st;
}

void Controller::StartNotifyCheck(bool userInitiated) {
    if (m_updateFetchRunning) return;
    m_updateFetchRunning = true;
    if (!m_updateWorker) m_updateWorker = std::make_unique<Worker>(L"openFences update check");
    std::wstring feed = updater::FeedUrl(m_policy);
    m_updateWorker->Post([feed, userInitiated] {
        std::wstring error;
        auto release = updater::FetchLatest(feed, error);
        PostToUi([release, error, userInitiated] { Controller::Get().OnNotifyCheckDone(release, error, userInitiated); });
    });
}

void Controller::OnNotifyCheckDone(std::optional<update::Release> release, const std::wstring& error, bool userInitiated) {
    m_updateFetchRunning = false;
    m_config.general.updateLastCheck = updater::NowUnix();
    SaveSoon();
    updater::Status& st = m_notifyStatus;
    if (!release) {
        st.code = updater::ResultCode::Failed;
        st.message = L"Couldn't check for updates: " + error + L".";
        log::Info(L"Updates: {}", st.message);
        return;
    }
    st.latestVersion = update::FormatVersion(release->version);
    if (release->version > CurrentVersion()) {
        st.code = updater::ResultCode::Available;
        st.message = L"openFences " + st.latestVersion + L" is available. Download it from the releases page.";
        if (m_notifiedVersion != st.latestVersion && m_tray && !userInitiated) {
            m_tray->Balloon(L"openFences " + st.latestVersion + L" is available",
                            L"Open Configure openFences → Updates to download it.");
        }
        m_notifiedVersion = st.latestVersion;
    } else {
        st.code = updater::ResultCode::UpToDate;
        st.message = L"openFences is up to date (latest release: " + st.latestVersion + L").";
    }
    log::Info(L"Updates: {}", st.message);
}

}  // namespace of
