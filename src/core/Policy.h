// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <optional>
#include <string>

namespace of {

// Business / administrator controls.
//   HKLM\SOFTWARE\Policies\openFences : ConfigPath, UpdateFeedUrl (REG_SZ); LockLayout, LockContent, DisableSettings,
//                                       Disabled, DisableUpdates (REG_DWORD)
//   HKCU\Software\openFences          : Disabled (REG_DWORD) - per-user disable
struct Policy {
    bool disabled = false;
    std::wstring configPath;  // layout/template pushed at logon (may be a UNC path)
    std::optional<bool> lockLayout;
    std::optional<bool> lockContent;
    bool disableSettings = false;
    bool disableUpdates = false;  // no update checks at all; Settings shows the update options greyed out
    std::wstring updateFeedUrl;   // replaces the GitHub "latest release" API (https, or http on loopback); empty = default
};

Policy ReadPolicy();

}  // namespace of
