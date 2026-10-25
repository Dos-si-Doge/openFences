#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/Model.h"

namespace of {

// What the rules engine needs to know about a desktop item.
struct RuleItemInfo {
    std::wstring name;        // file name including extension
    std::wstring linkTarget;  // resolved shortcut target ("" if not a shortcut)
    bool isFileSystem = true;
    bool isFolder = false;
    bool isShortcut = false;
    uint64_t created = 0;   // FILETIME ticks (100 ns since 1601), 0 = unknown
    uint64_t modified = 0;
};

constexpr uint64_t kTicksPerDay = 10'000'000ull * 60 * 60 * 24;

bool RuleHasCriteria(const Rule& rule);
bool RuleMatches(const Rule& rule, const RuleItemInfo& item, uint64_t nowTicks);
// First enabled rule (in priority order) that matches, or nullptr.
const Rule* EvaluateRules(const std::vector<Rule>& rules, const RuleItemInfo& item, uint64_t nowTicks);
// Human-readable one-line summary of a rule's criteria.
std::wstring DescribeCriteria(const Rule& rule);

// Default rules used by first-run setup ("Programs", "Folders", "Files").
std::vector<Rule> DefaultRules(const std::string& programsTab, const std::string& foldersTab, const std::string& filesTab);

uint64_t NowTicks();

}  // namespace of
