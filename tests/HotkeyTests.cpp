// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <windows.h>

#include "core/Hotkey.h"

using namespace of;

TEST(Hotkey, ParseAndFormat) {
    auto hk = ParseHotkey(L"win+shift+space");
    ASSERT_TRUE(hk);
    EXPECT_EQ(hk->modifiers, static_cast<unsigned>(MOD_WIN | MOD_SHIFT));
    EXPECT_EQ(hk->vk, static_cast<unsigned>(VK_SPACE));
    EXPECT_EQ(FormatHotkey(*hk), L"Win+Shift+Space");

    auto f = ParseHotkey(L"Ctrl+Alt+F12");
    ASSERT_TRUE(f);
    EXPECT_EQ(f->vk, static_cast<unsigned>(VK_F12));
    EXPECT_EQ(FormatHotkey(*f), L"Ctrl+Alt+F12");

    auto d = ParseHotkey(L"Win+D");
    ASSERT_TRUE(d);
    EXPECT_EQ(d->vk, static_cast<unsigned>('D'));
}

TEST(Hotkey, RejectsInvalid) {
    EXPECT_FALSE(ParseHotkey(L""));
    EXPECT_FALSE(ParseHotkey(L"Ctrl+Alt"));
    EXPECT_FALSE(ParseHotkey(L"Ctrl+Banana"));
    EXPECT_FALSE(ParseHotkey(L"Space+Ctrl"));
    EXPECT_FALSE(ParseHotkey(L"F25"));
}
