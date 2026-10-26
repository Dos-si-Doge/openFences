// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include "core/Rules.h"

using namespace of;

namespace {

Rule MakeRule(const char* id) {
    Rule r;
    r.id = id;
    r.targetTab = std::string("t-") + id;
    return r;
}

RuleItemInfo File(const wchar_t* name) {
    RuleItemInfo i;
    i.name = name;
    return i;
}

constexpr uint64_t kNow = 133'000'000'000'000'000ull;

}  // namespace

TEST(Rules, ExtensionMatch) {
    Rule r = MakeRule("img");
    r.match.extensions = {L".png", L".jpg"};
    EXPECT_TRUE(RuleMatches(r, File(L"Screenshot 1.PNG"), kNow));
    EXPECT_FALSE(RuleMatches(r, File(L"notes.txt"), kNow));
    RuleItemInfo folder = File(L"photos.png");
    folder.isFolder = true;
    EXPECT_FALSE(RuleMatches(r, folder, kNow)) << "extension rules never match folders";
}

TEST(Rules, NameGlobAndRegex) {
    Rule glob = MakeRule("g");
    glob.match.nameGlob = L"*invoice*";
    EXPECT_TRUE(RuleMatches(glob, File(L"Invoice-0042.pdf"), kNow));
    Rule re = MakeRule("r");
    re.match.nameRegex = L"^PRJ-\\d{3}";
    EXPECT_TRUE(RuleMatches(re, File(L"prj-123 plan.docx"), kNow));
    EXPECT_FALSE(RuleMatches(re, File(L"x prj-123.docx"), kNow));
    Rule bad = MakeRule("b");
    bad.match.nameRegex = L"([";
    EXPECT_FALSE(RuleMatches(bad, File(L"anything"), kNow)) << "invalid regex must not throw";
}

TEST(Rules, ShortcutTarget) {
    Rule r = MakeRule("games");
    r.match.targetPrefix = L"C:\\Games";
    RuleItemInfo lnk = File(L"Doom.lnk");
    lnk.isShortcut = true;
    lnk.linkTarget = L"c:\\games\\doom\\doom.exe";
    EXPECT_TRUE(RuleMatches(r, lnk, kNow));
    lnk.linkTarget = L"C:\\Program Files\\x.exe";
    EXPECT_FALSE(RuleMatches(r, lnk, kNow));
}

TEST(Rules, KindFilters) {
    Rule shortcuts = MakeRule("s");
    shortcuts.match.kind = ItemKindFilter::Shortcut;
    Rule folders = MakeRule("f");
    folders.match.kind = ItemKindFilter::Folder;
    Rule files = MakeRule("d");
    files.match.kind = ItemKindFilter::File;

    RuleItemInfo url = File(L"Site.url");
    EXPECT_TRUE(RuleMatches(shortcuts, url, kNow));
    EXPECT_FALSE(RuleMatches(files, url, kNow));

    RuleItemInfo dir = File(L"Projects");
    dir.isFolder = true;
    EXPECT_TRUE(RuleMatches(folders, dir, kNow));
    EXPECT_FALSE(RuleMatches(files, dir, kNow));

    RuleItemInfo recycle = File(L"::{645FF040-5081-101B-9F08-00AA002F954E}");
    recycle.isFileSystem = false;
    recycle.isFolder = true;
    EXPECT_FALSE(RuleMatches(folders, recycle, kNow)) << "virtual items never match kind filters";
}

TEST(Rules, DateCriteria) {
    Rule old = MakeRule("old");
    old.match.modifiedOlderThanDays = 30;
    RuleItemInfo i = File(L"a.txt");
    i.modified = kNow - 31 * kTicksPerDay;
    EXPECT_TRUE(RuleMatches(old, i, kNow));
    i.modified = kNow - 2 * kTicksPerDay;
    EXPECT_FALSE(RuleMatches(old, i, kNow));

    Rule recent = MakeRule("recent");
    recent.match.createdNewerThanDays = 7;
    i.created = kNow - 1 * kTicksPerDay;
    EXPECT_TRUE(RuleMatches(recent, i, kNow));
    i.created = 0;
    EXPECT_FALSE(RuleMatches(recent, i, kNow)) << "unknown dates never match";
}

TEST(Rules, AllVersusAny) {
    Rule r = MakeRule("combo");
    r.match.extensions = {L".pdf"};
    r.match.nameGlob = L"*invoice*";
    EXPECT_FALSE(RuleMatches(r, File(L"manual.pdf"), kNow));
    r.mode = MatchMode::Any;
    EXPECT_TRUE(RuleMatches(r, File(L"manual.pdf"), kNow));
}

TEST(Rules, PriorityFirstMatchWins) {
    Rule a = MakeRule("a");
    a.match.extensions = {L".png"};
    Rule b = MakeRule("b");
    b.match.nameGlob = L"*";
    Rule disabled = MakeRule("z");
    disabled.match.nameGlob = L"*";
    disabled.enabled = false;
    std::vector<Rule> rules{disabled, a, b};
    EXPECT_EQ(EvaluateRules(rules, File(L"x.png"), kNow)->id, "a");
    EXPECT_EQ(EvaluateRules(rules, File(L"x.txt"), kNow)->id, "b");
    EXPECT_EQ(EvaluateRules({}, File(L"x.txt"), kNow), nullptr);
}

TEST(Rules, EmptyRuleNeverMatches) {
    Rule r = MakeRule("empty");
    EXPECT_FALSE(RuleHasCriteria(r));
    EXPECT_FALSE(RuleMatches(r, File(L"x"), kNow));
}

TEST(Rules, DefaultRulesSortDesktop) {
    auto rules = DefaultRules("prog", "fold", "file");
    RuleItemInfo lnk = File(L"Chrome.lnk");
    lnk.isShortcut = true;
    RuleItemInfo dir = File(L"Work");
    dir.isFolder = true;
    EXPECT_EQ(EvaluateRules(rules, lnk, kNow)->targetTab, "prog");
    EXPECT_EQ(EvaluateRules(rules, File(L"setup.exe"), kNow)->targetTab, "prog");
    EXPECT_EQ(EvaluateRules(rules, dir, kNow)->targetTab, "fold");
    EXPECT_EQ(EvaluateRules(rules, File(L"report.docx"), kNow)->targetTab, "file");
}
