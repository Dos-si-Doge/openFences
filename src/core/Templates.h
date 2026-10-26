// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/Model.h"

namespace of {

// A template is a config document plus a "template" header:
//   { "template": {"name": "...", "description": "..."}, "layouts": {"template": {...}}, "rules": [...],
//     optional "lock" / "appearance" sections that are applied too }
struct Template {
    std::wstring name;
    std::wstring description;
    std::filesystem::path path;
    Layout layout;
    std::vector<Rule> rules;
    std::optional<LockSettings> lock;
    std::optional<AppearanceSettings> appearance;
};

std::optional<Template> ParseTemplate(std::string_view json);
std::optional<Template> LoadTemplate(const std::filesystem::path& path);
std::vector<Template> ListTemplates(const std::filesystem::path& dir);

// Distinct destinations of the enabled "move file to folder" rules, in rule order. Importing a layout from an untrusted
// file would otherwise silently move desktop files there (possibly to a network share), so the import asks first.
std::vector<std::wstring> MoveRuleDestinations(const std::vector<Rule>& rules);

// Replaces the given layout and the rule list with the template's content.
void ApplyTemplate(Config& config, const std::string& layoutKey, const Template& tpl);

// Builds the first-run layout: Programs / Folders / Files fences plus the default rules.
void BuildDefaultLayout(Config& config, const std::string& layoutKey, const std::wstring& primaryMonitor);

}  // namespace of
