#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "core/Model.h"

namespace of {

std::string SerializeConfig(const Config& config);
// Parses (and migrates) a configuration document. Unknown fields are ignored, missing ones get defaults.
std::optional<Config> ParseConfig(std::string_view json, std::string* error = nullptr);

// Atomic write: temp file + ReplaceFileW, so a crash never leaves a truncated config.
bool WriteFileAtomic(const std::filesystem::path& path, std::string_view contents);
std::optional<std::string> ReadFileText(const std::filesystem::path& path);

bool SaveConfigFile(const std::filesystem::path& path, const Config& config);
std::optional<Config> LoadConfigFile(const std::filesystem::path& path, std::string* error = nullptr);

// Serializes a single layout (used by export and templates).
std::string SerializeLayout(const Layout& layout);
std::optional<Layout> ParseLayout(std::string_view json);

}  // namespace of
