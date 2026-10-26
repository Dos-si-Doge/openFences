// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <optional>
#include <string>

namespace of {

// Business / administrator controls.
//   HKLM\SOFTWARE\Policies\openFences : ConfigPath (REG_SZ), LockLayout, LockContent, DisableSettings, Disabled (REG_DWORD)
//   HKCU\Software\openFences          : Disabled (REG_DWORD) - per-user disable
struct Policy {
    bool disabled = false;
    std::wstring configPath;  // layout/template pushed at logon (may be a UNC path)
    std::optional<bool> lockLayout;
    std::optional<bool> lockContent;
    bool disableSettings = false;
};

Policy ReadPolicy();

}  // namespace of
