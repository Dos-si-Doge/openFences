#pragma once

namespace of {

// Writes a minidump to %LOCALAPPDATA%\openFences\Crashes and restores Explorer's desktop icons
// when the process hits an unhandled exception.
void InstallCrashHandler();

}  // namespace of
