// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <filesystem>

#include "core/Config.h"
#include "core/Templates.h"

using namespace of;

TEST(Config, ColorParse) {
    auto c = Color::Parse("#3060ff80");
    ASSERT_TRUE(c);
    EXPECT_EQ(c->r, 0x30);
    EXPECT_EQ(c->a, 0x80);
    EXPECT_EQ(c->ToString(), "#3060ff80");
    EXPECT_EQ(Color::Parse("#102030")->a, 255);
    EXPECT_FALSE(Color::Parse("blue"));
    EXPECT_FALSE(Color::Parse("#12345z"));
}

TEST(Config, RoundTripPreservesEverything) {
    Config c;
    c.appearance.iconSize = 64;
    c.appearance.iconTint = Color{10, 20, 30, 255};
    c.appearance.tabStyle = TabStyle::Rounded;
    c.peek.hotkey = L"Ctrl+Alt+Space";
    c.peek.mode = TriggerMode::Hold;
    c.hide.hidden = true;
    c.hide.excludeItems = {L"Recycle Bin"};
    c.lock.layoutLocked = true;
    c.general.lastSeenVersion = L"1.0.1";
    c.general.showWhatsNew = false;

    Layout& l = c.layouts["abc123"];
    l.monitors = {L"\\\\?\\DISPLAY#GSM1#1"};
    Page& p = EnsurePage(l);
    Group g = MakeGroup(L"Dokumente ✓", {0.1, 0.2, 0.3, 0.4}, L"mon1");
    g.tabs[0].items = {L"Report.docx", L"::{645FF040-5081-101B-9F08-00AA002F954E}"};
    g.rolledUp = true;
    g.tabs.push_back(MakePortalGroup(L"Downloads", L"%USERPROFILE%\\Downloads", {}, L"", ViewMode::List).tabs[0]);
    p.groups.push_back(g);
    l.persistent.push_back(MakeGroup(L"Always", {0.5, 0.5, 0.1, 0.1}, L""));
    l.loose[L"notes.txt"] = LoosePos{L"mon1", 2, 3};

    Rule r;
    r.id = "r1";
    r.name = L"Images";
    r.match.extensions = {L".png"};
    r.match.modifiedOlderThanDays = 5;
    r.targetTab = g.tabs[0].id;
    c.rules.push_back(r);

    std::string text = SerializeConfig(c);
    auto back = ParseConfig(text);
    ASSERT_TRUE(back);
    EXPECT_EQ(back->appearance.iconSize, 64);
    EXPECT_EQ(back->appearance.iconTint, c.appearance.iconTint);
    EXPECT_EQ(back->appearance.tabStyle, TabStyle::Rounded);
    EXPECT_EQ(back->peek.hotkey, L"Ctrl+Alt+Space");
    EXPECT_EQ(back->peek.mode, TriggerMode::Hold);
    EXPECT_TRUE(back->hide.hidden);
    EXPECT_TRUE(back->lock.layoutLocked);
    EXPECT_EQ(back->general.lastSeenVersion, L"1.0.1");
    EXPECT_FALSE(back->general.showWhatsNew);

    Layout& bl = back->layouts.at("abc123");
    ASSERT_EQ(bl.pages.size(), 1u);
    const Group& bg = bl.pages[0].groups.at(0);
    EXPECT_EQ(bg.tabs[0].title, L"Dokumente ✓");
    EXPECT_EQ(bg.tabs[0].items, g.tabs[0].items);
    EXPECT_EQ(bg.rect, g.rect);
    EXPECT_TRUE(bg.rolledUp);
    ASSERT_EQ(bg.tabs.size(), 2u);
    EXPECT_EQ(bg.tabs[1].kind, TabKind::Portal);
    EXPECT_EQ(bg.tabs[1].view, ViewMode::List);
    EXPECT_EQ(bg.tabs[1].path, L"%USERPROFILE%\\Downloads");
    EXPECT_EQ(bl.persistent.size(), 1u);
    EXPECT_EQ(bl.loose.at(L"notes.txt"), (LoosePos{L"mon1", 2, 3}));

    ASSERT_EQ(back->rules.size(), 1u);
    EXPECT_EQ(back->rules[0].match.extensions[0], L".png");
    EXPECT_EQ(*back->rules[0].match.modifiedOlderThanDays, 5);
    EXPECT_EQ(back->rules[0].targetTab, g.tabs[0].id);
}

TEST(Config, MissingFieldsGetDefaults) {
    auto c = ParseConfig(R"({"version":1,"appearance":{"iconSize":9999}})");
    ASSERT_TRUE(c);
    EXPECT_EQ(c->appearance.iconSize, 256) << "values are clamped";
    EXPECT_TRUE(c->peek.enabled);
    EXPECT_TRUE(c->layouts.empty());
}

TEST(Config, RejectsGarbage) {
    EXPECT_FALSE(ParseConfig("not json"));
    EXPECT_FALSE(ParseConfig("[1,2,3]"));
}

TEST(Config, MigratesVersion0SingleLayout) {
    auto c = ParseConfig(R"({"pages":[{"id":"p1","name":"Work","groups":[{"id":"g1","rect":[0,0,0.5,0.5],"tabs":[{"id":"t1","title":"A"}]}]}]})");
    ASSERT_TRUE(c);
    ASSERT_EQ(c->layouts.count("default"), 1u);
    EXPECT_EQ(c->layouts["default"].pages[0].groups[0].tabs[0].title, L"A");
    EXPECT_EQ(c->version, kConfigVersion);
}

TEST(Config, ExtensionsNormalised) {
    auto c = ParseConfig(R"({"version":1,"rules":[{"id":"r","match":{"ext":["PNG",".Jpg"]},"target":"t"}]})");
    ASSERT_TRUE(c);
    EXPECT_EQ(c->rules[0].match.extensions, (std::vector<std::wstring>{L".png", L".jpg"}));
}

TEST(Config, AtomicFileWrite) {
    auto dir = std::filesystem::temp_directory_path() / "openfences-test-config";
    std::filesystem::remove_all(dir);
    auto file = dir / "config.json";
    Config c;
    c.appearance.iconSize = 32;
    ASSERT_TRUE(SaveConfigFile(file, c));
    c.appearance.iconSize = 96;
    ASSERT_TRUE(SaveConfigFile(file, c)) << "overwriting goes through ReplaceFileW";
    auto back = LoadConfigFile(file);
    ASSERT_TRUE(back);
    EXPECT_EQ(back->appearance.iconSize, 96);
    EXPECT_FALSE(std::filesystem::exists(dir / "config.json.tmp"));
    std::filesystem::remove_all(dir);
}

TEST(Model, ItemOwnershipHelpers) {
    Layout l;
    Page& p = EnsurePage(l);
    p.groups.push_back(MakeGroup(L"A", {}, L""));
    p.groups.push_back(MakeGroup(L"B", {}, L""));
    p.groups[0].tabs[0].items = {L"x.txt"};
    auto [g, t] = FindItemOwner(l, L"x.txt");
    ASSERT_TRUE(g);
    EXPECT_EQ(t->title, L"A");
    RemoveItemEverywhere(l, L"x.txt");
    EXPECT_FALSE(FindItemOwner(l, L"x.txt").first);
    std::string id = p.groups[1].id;
    auto taken = TakeGroup(l, id);
    ASSERT_TRUE(taken);
    EXPECT_EQ(l.pages[0].groups.size(), 1u);
    EXPECT_FALSE(FindGroup(l, id));
}

TEST(Templates, DefaultLayoutAndTemplateParse) {
    Config c;
    BuildDefaultLayout(c, "k", L"mon");
    ASSERT_EQ(c.layouts["k"].pages[0].groups.size(), 3u);
    EXPECT_FALSE(c.rules.empty());

    auto t = ParseTemplate(R"({"template":{"name":"Kiosk","description":"Locked"},
        "layouts":{"template":{"pages":[{"id":"p","name":"Main","groups":[{"id":"g","rect":[0.1,0.1,0.3,0.3],"tabs":[{"id":"t","title":"Apps"}]}]}]}},
        "rules":[{"id":"r","match":{"kind":"shortcut"},"target":"t"}],
        "lock":{"layoutLocked":true,"contentLocked":true}})");
    ASSERT_TRUE(t);
    EXPECT_EQ(t->name, L"Kiosk");
    ASSERT_TRUE(t->lock);
    EXPECT_FALSE(t->appearance);
    ApplyTemplate(c, "k", *t);
    EXPECT_TRUE(c.lock.layoutLocked);
    EXPECT_EQ(c.layouts["k"].pages[0].groups[0].tabs[0].title, L"Apps");
    EXPECT_EQ(c.rules.size(), 1u);
}

TEST(Templates, ShippedTemplatesParse) {
    auto list = ListTemplates(OF_TEMPLATES_DIR);
    ASSERT_EQ(list.size(), 3u) << "default, kiosk and work templates";
    for (const auto& t : list) {
        EXPECT_FALSE(t.name.empty());
        EXPECT_FALSE(t.layout.pages.empty()) << t.path;
        EXPECT_FALSE(t.rules.empty()) << t.path;
        // Every assign rule must point at a tab that exists in the template.
        Layout l = t.layout;
        for (const auto& r : t.rules)
            if (r.action == RuleAction::AssignToTab) EXPECT_TRUE(FindTab(l, r.targetTab).second) << r.id;
    }
}

TEST(Model, LooseItemVisibilityDuringQuickHide) {
    std::vector<std::wstring> exclude{L"Recycle Bin", L"notes.txt"};
    EXPECT_TRUE(LooseItemVisible(true, false, L"report.docx", exclude)) << "not hidden: everything visible";
    EXPECT_FALSE(LooseItemVisible(true, true, L"report.docx", exclude)) << "hidden: ordinary icons are hidden";
    EXPECT_TRUE(LooseItemVisible(true, true, L"notes.txt", exclude)) << "hidden: excluded icons stay visible and clickable";
    EXPECT_FALSE(LooseItemVisible(false, false, L"notes.txt", exclude)) << "loose icons turned off";
}
