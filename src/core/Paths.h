#pragma once

#include <filesystem>

namespace of::paths {

// A file with this name next to openFences.exe turns on portable mode: all data lives in "<exe folder>\Data".
constexpr wchar_t kPortableMarker[] = L"openFences.portable";
bool IsPortable();

// %APPDATA%\openFences (or %OPENFENCES_DATA_DIR% when set, for testing; "<exe folder>\Data" in portable mode).
std::filesystem::path RoamingDir();
// %LOCALAPPDATA%\openFences (logs, crash dumps); "<data dir>\local" when overridden or portable.
std::filesystem::path LocalDir();
std::filesystem::path ConfigFile();
std::filesystem::path BackupsDir();
std::filesystem::path LogsDir();
std::filesystem::path CrashesDir();
std::filesystem::path ExePath();
std::filesystem::path TemplatesDir();

}  // namespace of::paths
