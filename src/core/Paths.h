#pragma once

#include <filesystem>

namespace of::paths {

// %APPDATA%\openFences (or %OPENFENCES_DATA_DIR% when set, for testing).
std::filesystem::path RoamingDir();
// %LOCALAPPDATA%\openFences (logs, crash dumps).
std::filesystem::path LocalDir();
std::filesystem::path ConfigFile();
std::filesystem::path BackupsDir();
std::filesystem::path LogsDir();
std::filesystem::path CrashesDir();
std::filesystem::path ExePath();
std::filesystem::path TemplatesDir();

}  // namespace of::paths
