// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include "core/Autostart.h"

using namespace of;

TEST(Autostart, StartupApprovedParsing) {
    EXPECT_TRUE(autostart::IsApprovedDataEnabled({}));
    EXPECT_TRUE(autostart::IsApprovedDataEnabled({0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
    EXPECT_TRUE(autostart::IsApprovedDataEnabled({0x06, 0, 0, 0}));
    EXPECT_FALSE(autostart::IsApprovedDataEnabled({0x03, 0, 0, 0, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x01}));
    EXPECT_FALSE(autostart::IsApprovedDataEnabled({0x07}));
}

TEST(Autostart, StateEnabledLogic) {
    autostart::State s;
    EXPECT_FALSE(s.Enabled());
    s.runValuePresent = true;
    EXPECT_TRUE(s.Enabled());
    s.disabledByUser = true;
    EXPECT_FALSE(s.Enabled());
}

TEST(Autostart, CommandLineQuotesPath) {
    EXPECT_EQ(autostart::CommandLineFor(L"C:\\Program Files\\openFences\\openFences.exe"),
              L"\"C:\\Program Files\\openFences\\openFences.exe\" --autostart");
}
