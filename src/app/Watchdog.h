// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <windows.h>

namespace of::watchdog {

// Starts "openFences.exe --watchdog <pid>" for the current process.
void Spawn(int restarts, ULONGLONG firstRestartTick);

// Watchdog process body: waits for the main process; if it dies abnormally, makes Explorer's desktop
// icons visible again and (for crashes) restarts it, at most 3 times within 5 minutes.
int Run(DWORD pid, int restarts, ULONGLONG firstRestartTick);

}  // namespace of::watchdog
