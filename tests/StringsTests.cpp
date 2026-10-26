// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include "core/Strings.h"

using namespace of;

TEST(Strings, Utf8RoundTrip) {
    std::wstring w = L"Ünïcödé 文档 ✓";
    EXPECT_EQ(FromUtf8(ToUtf8(w)), w);
    EXPECT_EQ(ToUtf8(L""), "");
}

TEST(Strings, GlobMatch) {
    EXPECT_TRUE(GlobMatch(L"*invoice*", L"March Invoice 2026.pdf"));
    EXPECT_TRUE(GlobMatch(L"PRJ-???-*", L"prj-042-spec.docx"));
    EXPECT_FALSE(GlobMatch(L"PRJ-???-*", L"prj-42-spec.docx"));
    EXPECT_TRUE(GlobMatch(L"*", L""));
    EXPECT_FALSE(GlobMatch(L"a*b", L"acd"));
    EXPECT_TRUE(GlobMatch(L"*.png", L"shot.PNG"));
}

TEST(Strings, ExtensionOf) {
    EXPECT_EQ(ExtensionOf(L"Report.DOCX"), L".docx");
    EXPECT_EQ(ExtensionOf(L"archive.tar.gz"), L".gz");
    EXPECT_EQ(ExtensionOf(L"README"), L"");
    EXPECT_EQ(ExtensionOf(L"C:\\dir.v2\\file"), L"");
    EXPECT_EQ(ExtensionOf(L".gitignore"), L"");
}

TEST(Strings, SplitAndJoin) {
    auto parts = SplitList(L" .png, .jpg;.gif  ");
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[1], L".jpg");
    EXPECT_EQ(JoinList(parts, L"|"), L".png|.jpg|.gif");
}

TEST(Strings, SanitizeFileName) {
    EXPECT_EQ(SanitizeFileName(L"Before: move?"), L"Before_ move_");
}

TEST(Strings, CaseInsensitive) {
    EXPECT_TRUE(StartsWithI(L"C:\\Games\\Steam\\x.exe", L"c:\\games"));
    EXPECT_TRUE(EqualsI(L"ABC", L"abc"));
    EXPECT_TRUE(ContainsI(L"Hello World", L"WORLD"));
}
