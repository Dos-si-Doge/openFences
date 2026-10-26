// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include "core/MonitorConfig.h"

using namespace of;

namespace {
MonitorDesc Mon(const wchar_t* id, int x, int y, int w, int h, bool primary = false) {
    MonitorDesc m;
    m.id = id;
    m.bounds = RectI::FromXYWH(x, y, w, h);
    m.work = RectI::FromXYWH(x, y, w, h - 40);
    m.primary = primary;
    return m;
}
}  // namespace

TEST(MonitorConfig, HashIsOrderIndependentAndCaseInsensitive) {
    auto a = Mon(L"MON-A", 0, 0, 1920, 1080, true);
    auto b = Mon(L"mon-b", 1920, 0, 2560, 1440);
    auto b2 = b;
    b2.id = L"MON-B";
    EXPECT_EQ(ComputeMonitorConfigHash({a, b}), ComputeMonitorConfigHash({b2, a}));
}

TEST(MonitorConfig, HashChangesWithResolutionOrArrangement) {
    auto a = Mon(L"A", 0, 0, 1920, 1080, true);
    auto b = Mon(L"B", 1920, 0, 1920, 1080);
    std::string base = ComputeMonitorConfigHash({a, b});
    auto moved = b;
    moved.bounds = RectI::FromXYWH(-1920, 0, 1920, 1080);
    EXPECT_NE(base, ComputeMonitorConfigHash({a, moved}));
    auto res = a;
    res.bounds = RectI::FromXYWH(0, 0, 2560, 1440);
    EXPECT_NE(base, ComputeMonitorConfigHash({res, b}));
    EXPECT_NE(base, ComputeMonitorConfigHash({a})) << "undocking changes the key";
}

TEST(MonitorConfig, ClosestLayoutPrefersSharedMonitors) {
    std::map<std::string, Layout> layouts;
    layouts["laptop"].monitors = {L"LAPTOP"};
    layouts["laptop"].pages.push_back(Page{"p", L"P", {MakeGroup(L"x", {}, L"")}});
    layouts["docked"].monitors = {L"LAPTOP", L"DELL"};
    layouts["docked"].pages.push_back(Page{"p", L"P", {MakeGroup(L"x", {}, L"")}});
    auto best = FindClosestLayout(layouts, {Mon(L"LAPTOP", 0, 0, 1920, 1080, true), Mon(L"DELL", 1920, 0, 2560, 1440)});
    EXPECT_EQ(best, "docked");
    best = FindClosestLayout(layouts, {Mon(L"laptop", 0, 0, 1600, 900, true)});
    EXPECT_EQ(best, "laptop");
    EXPECT_EQ(FindClosestLayout({}, {Mon(L"X", 0, 0, 1, 1)}), "");
}

TEST(MonitorConfig, MonitorLookup) {
    std::vector<MonitorDesc> mons{Mon(L"A", 0, 0, 1920, 1080, true), Mon(L"B", 1920, 0, 1920, 1080)};
    EXPECT_EQ(PrimaryMonitor(mons)->id, L"A");
    EXPECT_EQ(FindMonitor(mons, L"b")->id, L"B");
    EXPECT_EQ(FindMonitor(mons, L"zzz"), nullptr);
    EXPECT_EQ(MonitorForRect(mons, RectI::FromXYWH(1800, 10, 400, 100))->id, L"B");
    EXPECT_EQ(MonitorForRect(mons, RectI::FromXYWH(-9000, -9000, 10, 10))->id, L"A") << "offscreen falls back to primary";
}
