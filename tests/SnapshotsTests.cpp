// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "core/Snapshots.h"

using namespace of;
namespace fs = std::filesystem;

TEST(Snapshots, AutoBackupRotationKeepsNewest) {
    std::vector<std::wstring> names{L"auto-20261020.json", L"auto-20261024.json", L"snap-20261001-101010-Mine.json",
                                    L"auto-20261018.json", L"auto-20261023.json", L"auto-20261019.json",
                                    L"auto-20261021.json", L"auto-20261022.json", L"auto-20261017.json"};
    auto del = AutoBackupsToDelete(names, 7);
    ASSERT_EQ(del.size(), 1u);
    EXPECT_EQ(del[0], L"auto-20261017.json");
    EXPECT_TRUE(AutoBackupsToDelete({L"auto-20261024.json"}, 7).empty());
}

TEST(Snapshots, SaveListLoadAndDailyBackups) {
    auto dir = fs::temp_directory_path() / "openfences-test-snapshots";
    fs::remove_all(dir);
    Config c;
    c.appearance.iconSize = 72;
    auto saved = SaveSnapshot(dir, c, L"Before: reorg", L"20261024-101500");
    ASSERT_TRUE(saved);

    for (int d = 10; d <= 20; ++d) EnsureDailyBackup(dir, c, L"202610" + std::to_wstring(d), 7);
    EXPECT_FALSE(EnsureDailyBackup(dir, c, L"20261020", 7)) << "already exists today";

    auto list = ListSnapshots(dir);
    int autos = 0;
    for (const auto& s : list) autos += s.automatic ? 1 : 0;
    EXPECT_EQ(autos, 7);
    auto manual = std::find_if(list.begin(), list.end(), [](const SnapshotInfo& s) { return !s.automatic; });
    ASSERT_NE(manual, list.end());
    EXPECT_EQ(manual->name, L"Before: reorg") << "display name comes from file contents";
    EXPECT_EQ(manual->stamp, L"20261024-101500");
    EXPECT_EQ(list.front().stamp, L"20261024-101500") << "newest first";

    auto loaded = LoadSnapshot(manual->path);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->appearance.iconSize, 72);
    fs::remove_all(dir);
}

TEST(Snapshots, FormatStamp) {
    EXPECT_EQ(FormatStamp(L"20261024-153012"), L"2026-10-24 15:30");
    EXPECT_EQ(FormatStamp(L"20261024"), L"2026-10-24");
}
