// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace of::autostart {

constexpr wchar_t kValueName[] = L"openFences";

// Task Manager -> Startup writes a 12-byte value under Explorer\StartupApproved\Run:
// first byte even (0x02/0x06) = enabled, odd (0x03/0x07) = disabled. Missing = enabled.
bool IsApprovedDataEnabled(const std::vector<uint8_t>& data);

struct State {
    bool runValuePresent = false;
    bool disabledByUser = false;  // disabled in Task Manager / Settings > Apps > Startup
    std::wstring command;
    bool Enabled() const { return runValuePresent && !disabledByUser; }
};

State Query();
// Writes or removes HKCU\...\Run\openFences. Enabling also clears a Task Manager "disabled" mark.
bool Set(bool enable, const std::wstring& exePath);
std::wstring CommandLineFor(const std::wstring& exePath);

// Applies the machine default written by the installer (HKLM\SOFTWARE\openFences\DefaultAutostart)
// the first time each user runs the app. Returns true if a value was applied.
bool ApplyInstallerDefaultOnce(const std::wstring& exePath);

}  // namespace of::autostart
