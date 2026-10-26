// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "Version.h"
#include "core/Strings.h"
#include "core/Welcome.h"

using namespace of;

namespace {
std::string ReadAll(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
}  // namespace

// The built-in Welcome window and the external HTML guide must stay in step: every page links to an existing guide
// section, has its hero image, and opens a real Settings page.
TEST(Welcome, PagesReferenceExistingGuideSectionsImagesAndSettings) {
    std::string guide = ReadAll(std::filesystem::path(OF_SOURCE_DIR) / L"docs/user-guide/guide.src.html");
    ASSERT_FALSE(guide.empty());
    const std::set<std::wstring> settingsPages = {L"welcome", L"chameleon", L"personalization", L"portals", L"peek",
                                                  L"hide",    L"rollup",    L"tabs",            L"pages",   L"rules",
                                                  L"layout",  L"business",  L"about"};
    std::set<std::wstring> ids;
    for (const auto& page : WelcomePages()) {
        EXPECT_TRUE(ids.insert(page.id).second) << "duplicate id " << ToUtf8(page.id);
        EXPECT_FALSE(page.title.empty());
        EXPECT_FALSE(page.description.empty());
        if (!page.guideAnchor.empty())
            EXPECT_NE(guide.find("id=\"" + ToUtf8(page.guideAnchor) + "\""), std::string::npos)
                << "guide section #" << ToUtf8(page.guideAnchor) << " is missing";
        if (!page.settingsPage.empty()) EXPECT_TRUE(settingsPages.count(page.settingsPage)) << ToUtf8(page.settingsPage);
        EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(OF_SOURCE_DIR) / L"res/welcome" / (page.hero + L".jpg")))
            << "hero image " << ToUtf8(page.hero) << ".jpg is missing";
    }
    EXPECT_EQ(WelcomePages().front().id, L"welcome");
    EXPECT_EQ(WelcomePages()[static_cast<size_t>(WelcomePageIndex(L"peek"))].id, L"peek");
    EXPECT_EQ(WelcomePageIndex(L"no-such-page"), 0);
}

// "What's new" is shown after updates, so each release must describe itself there and in the changelog.
// Hero images show whole and proportional, and stop growing at their native size instead of zooming in.
TEST(Welcome, FitHeroScalesProportionallyUpToNativeSize) {
    HeroSize s = FitHero(1200, 600, 780);
    EXPECT_EQ(s.width, 780);
    EXPECT_EQ(s.height, 390);
    s = FitHero(1200, 600, 2400);  // wider than the image: native size, centered by the caller
    EXPECT_EQ(s.width, 1200);
    EXPECT_EQ(s.height, 600);
    s = FitHero(1200, 600, 1200);
    EXPECT_EQ(s.width, 1200);
    EXPECT_EQ(s.height, 600);
    s = FitHero(1200, 900, 300);
    EXPECT_EQ(s.width, 300);
    EXPECT_EQ(s.height, 225);
    s = FitHero(0, 600, 780);
    EXPECT_EQ(s.width, 0);
    EXPECT_EQ(s.height, 0);
}

// The author compiled into the exe (Version.h) must be the copyright holder named in LICENSE.
TEST(Welcome, CreditsMatchTheLicense) {
    std::string license = ReadAll(std::filesystem::path(OF_SOURCE_DIR) / L"LICENSE");
    ASSERT_FALSE(license.empty());
    EXPECT_NE(license.find("MIT License"), std::string::npos);
    EXPECT_NE(license.find(std::string("Copyright (c) 2026 ") + OF_AUTHOR), std::string::npos);
    EXPECT_EQ(std::count_if(WelcomePages().begin(), WelcomePages().end(), [](const WelcomePage& p) { return p.credits; }), 1);
}

TEST(Welcome, WhatsNewMatchesTheCurrentRelease) {
    const auto& page = WelcomePages()[static_cast<size_t>(WelcomePageIndex(kWhatsNewPageId))];
    EXPECT_EQ(page.id, kWhatsNewPageId);
    EXPECT_NE(page.title.find(OF_VERSION_WSTRING), std::wstring::npos);
    EXPECT_FALSE(page.steps.empty());
    std::string changelog = ReadAll(std::filesystem::path(OF_SOURCE_DIR) / L"CHANGELOG.md");
    EXPECT_NE(changelog.find(std::string("## [") + OF_VERSION_STRING + "]"), std::string::npos)
        << "CHANGELOG.md has no entry for " << OF_VERSION_STRING;
}
