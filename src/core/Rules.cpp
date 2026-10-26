// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "core/Rules.h"

#include <windows.h>

#include <regex>

#include "core/Strings.h"

namespace of {

namespace {

bool IsShortcutName(const std::wstring& name) {
    std::wstring ext = ExtensionOf(name);
    return ext == L".lnk" || ext == L".url" || ext == L".appref-ms" || ext == L".pif";
}

bool KindMatches(ItemKindFilter kind, const RuleItemInfo& item) {
    bool shortcut = item.isShortcut || IsShortcutName(item.name);
    switch (kind) {
        case ItemKindFilter::Any: return true;
        case ItemKindFilter::Shortcut: return item.isFileSystem && shortcut;
        case ItemKindFilter::Folder: return item.isFileSystem && item.isFolder && !shortcut;
        case ItemKindFilter::File: return item.isFileSystem && !item.isFolder && !shortcut;
    }
    return false;
}

bool RegexMatches(const std::wstring& pattern, const std::wstring& text) {
    try {
        std::wregex re(pattern, std::regex::ECMAScript | std::regex::icase);
        return std::regex_search(text, re);
    } catch (const std::regex_error&) {
        return false;
    }
}

// Age checks: "older than N days" means age >= N days.
bool AgeOlder(uint64_t stamp, uint64_t now, int days) {
    return stamp != 0 && now >= stamp && (now - stamp) >= static_cast<uint64_t>(days) * kTicksPerDay;
}
bool AgeNewer(uint64_t stamp, uint64_t now, int days) {
    return stamp != 0 && (now < stamp || (now - stamp) < static_cast<uint64_t>(days) * kTicksPerDay);
}

}  // namespace

bool RuleHasCriteria(const Rule& r) {
    const RuleCriteria& m = r.match;
    return !m.extensions.empty() || !m.nameGlob.empty() || !m.nameRegex.empty() || !m.targetPrefix.empty() ||
           m.kind != ItemKindFilter::Any || m.modifiedOlderThanDays || m.modifiedNewerThanDays || m.createdOlderThanDays ||
           m.createdNewerThanDays;
}

bool RuleMatches(const Rule& rule, const RuleItemInfo& item, uint64_t now) {
    if (!rule.enabled || !RuleHasCriteria(rule)) return false;
    const RuleCriteria& m = rule.match;
    std::vector<bool> results;

    if (!m.extensions.empty()) {
        std::wstring ext = ExtensionOf(item.name);
        bool hit = false;
        for (const auto& e : m.extensions) hit = hit || EqualsI(e, ext);
        results.push_back(hit && !item.isFolder);
    }
    if (!m.nameGlob.empty()) results.push_back(GlobMatch(m.nameGlob, item.name));
    if (!m.nameRegex.empty()) results.push_back(RegexMatches(m.nameRegex, item.name));
    if (!m.targetPrefix.empty()) {
        std::wstring prefix = ExpandEnv(m.targetPrefix);
        results.push_back(!item.linkTarget.empty() && StartsWithI(item.linkTarget, prefix));
    }
    if (m.kind != ItemKindFilter::Any) results.push_back(KindMatches(m.kind, item));
    if (m.modifiedOlderThanDays) results.push_back(AgeOlder(item.modified, now, *m.modifiedOlderThanDays));
    if (m.modifiedNewerThanDays) results.push_back(AgeNewer(item.modified, now, *m.modifiedNewerThanDays));
    if (m.createdOlderThanDays) results.push_back(AgeOlder(item.created, now, *m.createdOlderThanDays));
    if (m.createdNewerThanDays) results.push_back(AgeNewer(item.created, now, *m.createdNewerThanDays));

    if (rule.mode == MatchMode::All) {
        for (bool b : results)
            if (!b) return false;
        return true;
    }
    for (bool b : results)
        if (b) return true;
    return false;
}

const Rule* EvaluateRules(const std::vector<Rule>& rules, const RuleItemInfo& item, uint64_t now) {
    for (const Rule& r : rules)
        if (RuleMatches(r, item, now)) return &r;
    return nullptr;
}

std::wstring DescribeCriteria(const Rule& r) {
    const RuleCriteria& m = r.match;
    std::vector<std::wstring> parts;
    if (!m.extensions.empty()) parts.push_back(L"type " + JoinList(m.extensions, L" "));
    if (!m.nameGlob.empty()) parts.push_back(L"name like \"" + m.nameGlob + L"\"");
    if (!m.nameRegex.empty()) parts.push_back(L"name ~ /" + m.nameRegex + L"/");
    if (!m.targetPrefix.empty()) parts.push_back(L"target in " + m.targetPrefix);
    switch (m.kind) {
        case ItemKindFilter::File: parts.push_back(L"is a file"); break;
        case ItemKindFilter::Folder: parts.push_back(L"is a folder"); break;
        case ItemKindFilter::Shortcut: parts.push_back(L"is a shortcut"); break;
        default: break;
    }
    if (m.modifiedOlderThanDays) parts.push_back(L"modified > " + std::to_wstring(*m.modifiedOlderThanDays) + L"d ago");
    if (m.modifiedNewerThanDays) parts.push_back(L"modified < " + std::to_wstring(*m.modifiedNewerThanDays) + L"d ago");
    if (m.createdOlderThanDays) parts.push_back(L"created > " + std::to_wstring(*m.createdOlderThanDays) + L"d ago");
    if (m.createdNewerThanDays) parts.push_back(L"created < " + std::to_wstring(*m.createdNewerThanDays) + L"d ago");
    if (parts.empty()) return L"(no criteria)";
    return JoinList(parts, r.mode == MatchMode::All ? L" and " : L" or ");
}

std::vector<Rule> DefaultRules(const std::string& programsTab, const std::string& foldersTab, const std::string& filesTab) {
    std::vector<Rule> rules;
    Rule programs;
    programs.id = NewId("r");
    programs.name = L"Programs";
    programs.match.kind = ItemKindFilter::Shortcut;
    programs.targetTab = programsTab;
    rules.push_back(programs);

    Rule exes;
    exes.id = NewId("r");
    exes.name = L"Applications";
    exes.match.extensions = {L".exe", L".msi", L".bat", L".cmd"};
    exes.targetTab = programsTab;
    rules.push_back(exes);

    Rule folders;
    folders.id = NewId("r");
    folders.name = L"Folders";
    folders.match.kind = ItemKindFilter::Folder;
    folders.targetTab = foldersTab;
    rules.push_back(folders);

    Rule files;
    files.id = NewId("r");
    files.name = L"Files and documents";
    files.match.kind = ItemKindFilter::File;
    files.targetTab = filesTab;
    rules.push_back(files);
    return rules;
}

uint64_t NowTicks() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

}  // namespace of
