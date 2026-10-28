// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include "core/Update.h"

using namespace of::update;

namespace {
// Shape of GitHub's /releases/latest response (trimmed), as returned for v1.0.3.
const char* kRelease = R"({
  "tag_name": "v1.0.3", "draft": false, "prerelease": false, "published_at": "2026-10-26T13:27:39Z",
  "assets": [
    {"name": "openFences-1.0.3-x64-portable.zip",
     "browser_download_url": "https://github.com/Dos-si-Doge/openFences/releases/download/v1.0.3/openFences-1.0.3-x64-portable.zip",
     "digest": "sha256:0c0fbb8fea403d6a0236fe19e1b20c3a8ffe594e96f692306452947f5b563ea6"},
    {"name": "openFences-1.0.3-x64.msi",
     "browser_download_url": "https://github.com/Dos-si-Doge/openFences/releases/download/v1.0.3/openFences-1.0.3-x64.msi",
     "digest": "sha256:A98811916149C61ED1FFCD1D5B55717E403D2F1C695D8D9AF3F2BC583C182AD4"}
  ]})";
constexpr int64_t kPublished = 1793021259;  // 2026-10-26T13:27:39Z
constexpr int64_t kDay = 86400;
}  // namespace

TEST(Update, ParseAndCompareVersions) {
    EXPECT_EQ(ParseVersion(L"v1.0.3"), (Version{1, 0, 3}));
    EXPECT_EQ(ParseVersion(L"1.10.0"), (Version{1, 10, 0}));
    EXPECT_EQ(ParseVersion(L"2.0.1-beta"), (Version{2, 0, 1}));
    EXPECT_FALSE(ParseVersion(L"1.0"));
    EXPECT_FALSE(ParseVersion(L"v1.0.x"));
    EXPECT_FALSE(ParseVersion(L"1.0.3 evil"));
    EXPECT_FALSE(ParseVersion(L""));
    EXPECT_TRUE((Version{1, 10, 0}) > (Version{1, 9, 9}));
    EXPECT_TRUE((Version{1, 1, 0}) > (Version{1, 0, 3}));
    EXPECT_EQ(FormatVersion({1, 1, 0}), L"1.1.0");
}

TEST(Update, ParsesTheLatestReleaseJson) {
    auto r = ParseLatestRelease(kRelease);
    ASSERT_TRUE(r);
    EXPECT_EQ(r->version, (Version{1, 0, 3}));
    EXPECT_EQ(r->publishedUnix, kPublished);
    EXPECT_EQ(r->msi.name, L"openFences-1.0.3-x64.msi");
    EXPECT_EQ(r->msi.sha256, L"a98811916149c61ed1ffcd1d5b55717e403d2f1c695d8d9af3f2bc583c182ad4") << "digest is lower-cased";
    ASSERT_TRUE(r->zip);
    EXPECT_EQ(r->zip->sha256.size(), 64u);
}

TEST(Update, RejectsReleasesThatDontAddUp) {
    std::string s = kRelease;
    auto replaced = [&](const std::string& from, const std::string& to) {
        std::string c = s;
        c.replace(c.find(from), from.size(), to);
        return c;
    };
    EXPECT_FALSE(ParseLatestRelease(replaced("\"v1.0.3\"", "\"v1.0.4\""))) << "tag doesn't match the asset names";
    EXPECT_FALSE(ParseLatestRelease(replaced("\"prerelease\": false", "\"prerelease\": true")));
    EXPECT_FALSE(ParseLatestRelease(replaced("\"draft\": false", "\"draft\": true")));
    EXPECT_FALSE(ParseLatestRelease(replaced("2026-10-26T13:27:39Z", "yesterday")));
    EXPECT_FALSE(ParseLatestRelease("{not json"));
    EXPECT_FALSE(ParseLatestRelease("[]"));
    // A missing or malformed digest still parses, with an empty digest: the updater then refuses to install.
    auto noDigest = ParseLatestRelease(replaced("sha256:A988", "md5:A988"));
    ASSERT_TRUE(noDigest);
    EXPECT_TRUE(noDigest->msi.sha256.empty());
}

TEST(Update, DecidesByVersionAndReleaseAge) {
    Release r = *ParseLatestRelease(kRelease);
    // Not newer: nothing to do, whatever the age.
    EXPECT_EQ(Decide({1, 0, 3}, r, kPublished + 10 * kDay, true).decision, Decision::UpToDate);
    EXPECT_EQ(Decide({1, 1, 0}, r, kPublished + 10 * kDay, true).decision, Decision::UpToDate);
    // Newer, default: only after 3 days.
    auto young = Decide({1, 0, 2}, r, kPublished + kDay, false);
    EXPECT_EQ(young.decision, Decision::TooNew);
    EXPECT_EQ(young.daysUntilEligible, 2);
    EXPECT_EQ(Decide({1, 0, 2}, r, kPublished + 3 * kDay - 1, false).decision, Decision::TooNew);
    EXPECT_EQ(Decide({1, 0, 2}, r, kPublished + 3 * kDay - 1, false).daysUntilEligible, 1);
    EXPECT_EQ(Decide({1, 0, 2}, r, kPublished + 3 * kDay, false).decision, Decision::Install);
    // "Install new updates immediately".
    EXPECT_EQ(Decide({1, 0, 2}, r, kPublished + 60, true).decision, Decision::Install);
    // Clock behind the publish time: still waits the full period.
    EXPECT_EQ(Decide({1, 0, 2}, r, kPublished - kDay, false).daysUntilEligible, 3);
}

TEST(Update, SchedulesPeriodicChecks) {
    EXPECT_TRUE(CheckDue(0, 7, kPublished)) << "never checked";
    EXPECT_FALSE(CheckDue(kPublished, 7, kPublished + 6 * kDay));
    EXPECT_TRUE(CheckDue(kPublished, 7, kPublished + 7 * kDay));
    EXPECT_TRUE(CheckDue(kPublished, 7, kPublished - 60)) << "clock went backwards";
    EXPECT_EQ(NormalizeInterval(7), 7);
    EXPECT_EQ(NormalizeInterval(0), 1);
    EXPECT_EQ(NormalizeInterval(10), 7);
    EXPECT_EQ(NormalizeInterval(365), 30);
}

TEST(Update, OnlyFetchesAllowedUrls) {
    const std::wstring feed = kDefaultFeedUrl;
    EXPECT_TRUE(IsAllowedUrl(feed, feed));
    EXPECT_TRUE(IsAllowedUrl(L"https://github.com/Dos-si-Doge/openFences/releases/download/v1.0.3/openFences-1.0.3-x64.msi", feed));
    EXPECT_TRUE(IsAllowedUrl(L"https://release-assets.githubusercontent.com/github-production-release-asset/1/2?sig=x", feed));
    EXPECT_TRUE(IsAllowedUrl(L"https://objects.githubusercontent.com/x", feed));
    EXPECT_FALSE(IsAllowedUrl(L"http://github.com/Dos-si-Doge/openFences/releases/download/v1.0.3/x.msi", feed)) << "no plain http";
    EXPECT_FALSE(IsAllowedUrl(L"https://github.com/someone-else/openFences/releases/download/v1/x.msi", feed));
    EXPECT_FALSE(IsAllowedUrl(L"https://evil.example/openFences-1.0.3-x64.msi", feed));
    EXPECT_FALSE(IsAllowedUrl(L"https://github.com@evil.example/x", feed)) << "credentials trick";
    EXPECT_FALSE(IsAllowedUrl(L"https://objects.githubusercontent.com:8443/x", feed));
    // Administrator feed (policy): same host only; http only on loopback.
    EXPECT_TRUE(IsAcceptableFeedUrl(L"https://mirror.corp.example/openfences/latest.json"));
    EXPECT_TRUE(IsAcceptableFeedUrl(L"http://127.0.0.1:8899/latest.json"));
    EXPECT_FALSE(IsAcceptableFeedUrl(L"http://mirror.corp.example/latest.json"));
    EXPECT_FALSE(IsAcceptableFeedUrl(L"file:///C:/latest.json"));
    EXPECT_TRUE(IsAllowedUrl(L"http://127.0.0.1:8899/files/openFences-1.1.1-x64.msi", L"http://127.0.0.1:8899/latest.json"));
    EXPECT_FALSE(IsAllowedUrl(L"http://127.0.0.1:9999/x.msi", L"http://127.0.0.1:8899/latest.json")) << "different port";
    EXPECT_FALSE(IsAllowedUrl(L"https://evil.example/x.msi", L"https://mirror.corp.example/latest.json"));
}
