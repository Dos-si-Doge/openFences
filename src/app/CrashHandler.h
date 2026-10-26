// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

namespace of {

// Writes a minidump to %LOCALAPPDATA%\openFences\Crashes and restores Explorer's desktop icons
// when the process hits an unhandled exception.
void InstallCrashHandler();

}  // namespace of
