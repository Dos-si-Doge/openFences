// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace of::update {

// Automatic updates: the pure decision logic (no network, registry or windows), shared by the app and the updater
// service. Releases come from GitHub's "latest release" API; see app/Updater.cpp for the transport and the checks made
// before anything is installed.

constexpr wchar_t kDefaultFeedUrl[] = L"https://api.github.com/repos/Dos-si-Doge/openFences/releases/latest";
constexpr wchar_t kReleasesPage[] = L"https://github.com/Dos-si-Doge/openFences/releases/latest";
// Assets of the default feed must be served from here (GitHub then redirects to its download hosts).
constexpr wchar_t kReleaseDownloadPrefix[] = L"https://github.com/Dos-si-Doge/openFences/releases/download/";

constexpr int kMinReleaseAgeDays = 3;         // unless "install new updates immediately" is on
constexpr int kDefaultCheckIntervalDays = 7;  // while openFences keeps running; every launch checks too
constexpr std::array<int, 5> kCheckIntervalChoices = {1, 3, 7, 14, 30};

struct Version {
    int major = 0, minor = 0, patch = 0;
    auto operator<=>(const Version&) const = default;
};

// "1.2.3" or "v1.2.3" (a "-suffix" or "+build" after the patch number is ignored). nullopt when malformed.
std::optional<Version> ParseVersion(std::wstring_view text);
std::wstring FormatVersion(const Version& v);

struct Asset {
    std::wstring name;
    std::wstring url;
    std::wstring sha256;  // lower-case hex; empty when the feed gave no digest
};

struct Release {
    Version version;
    std::wstring tag;
    int64_t publishedUnix = 0;  // seconds since 1970-01-01 UTC
    Asset msi;                  // openFences-<version>-x64.msi
    std::optional<Asset> zip;   // openFences-<version>-x64-portable.zip
};

// Parses GitHub's "latest release" JSON. nullopt for drafts, prereleases, a malformed tag or date, or when no MSI asset
// named after the tag's version exists (so the asset and the tag can't disagree).
std::optional<Release> ParseLatestRelease(std::string_view json);

// "2026-10-26T13:27:39Z" -> Unix seconds (UTC). nullopt when malformed.
std::optional<int64_t> ParseIsoUtc(std::string_view text);

enum class Decision { UpToDate, TooNew, Install };
struct DecisionResult {
    Decision decision = Decision::UpToDate;
    int daysUntilEligible = 0;  // TooNew: whole days left until the release is old enough (at least 1)
};

// Install only releases newer than the running version; unless installImmediately, only once they have been published
// for at least minAgeDays.
DecisionResult Decide(const Version& current, const Release& latest, int64_t nowUnix, bool installImmediately,
                      int minAgeDays = kMinReleaseAgeDays);

// True when a periodic check is due: never checked, the clock went backwards, or intervalDays have passed.
bool CheckDue(int64_t lastCheckUnix, int intervalDays, int64_t nowUnix);

// Nearest supported interval (1, 3, 7, 14 or 30 days).
int NormalizeInterval(int days);

// URLs the updater may fetch. With the default feed: the API itself, release downloads under kReleaseDownloadPrefix and
// GitHub's download hosts they redirect to. With an administrator's feed (policy UpdateFeedUrl): https URLs on the
// feed's own host, and http only on the loopback host (local mirrors and tests).
bool IsAllowedUrl(std::wstring_view url, std::wstring_view feedUrl);
// A feed URL an administrator may configure: https, or http to localhost / 127.0.0.1.
bool IsAcceptableFeedUrl(std::wstring_view url);

}  // namespace of::update
