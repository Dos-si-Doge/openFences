// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "core/Update.h"

#include <algorithm>
#include <cwctype>
#include <ctime>
#include <nlohmann/json.hpp>

#include "core/Strings.h"

namespace of::update {

namespace {

bool ParseInt(std::wstring_view s, size_t& pos, int& out) {
    size_t start = pos;
    long long v = 0;
    while (pos < s.size() && iswdigit(s[pos]) && pos - start < 6) v = v * 10 + (s[pos++] - L'0');
    if (pos == start) return false;
    out = static_cast<int>(v);
    return true;
}

struct UrlParts {
    std::wstring scheme;  // lower-case
    std::wstring host;    // lower-case, without port
    std::wstring port;
    std::wstring path;    // from the first '/', may be empty
};

std::optional<UrlParts> SplitUrl(std::wstring_view url) {
    size_t sep = url.find(L"://");
    if (sep == std::wstring_view::npos) return std::nullopt;
    UrlParts u;
    u.scheme = ToLower(url.substr(0, sep));
    std::wstring_view rest = url.substr(sep + 3);
    size_t slash = rest.find(L'/');
    std::wstring_view authority = rest.substr(0, slash);
    u.path = slash == std::wstring_view::npos ? L"" : std::wstring(rest.substr(slash));
    // No credentials ("user@host") and nothing that could smuggle a different host.
    if (authority.empty() || authority.find_first_of(L"@\\ ") != std::wstring_view::npos) return std::nullopt;
    size_t colon = authority.rfind(L':');
    if (colon != std::wstring_view::npos) {
        u.port = authority.substr(colon + 1);
        authority = authority.substr(0, colon);
        if (u.port.empty() || !std::all_of(u.port.begin(), u.port.end(), [](wchar_t c) { return iswdigit(c); })) return std::nullopt;
    }
    u.host = ToLower(authority);
    if (u.host.empty()) return std::nullopt;
    return u;
}

bool IsLoopback(const std::wstring& host) { return host == L"localhost" || host == L"127.0.0.1"; }

bool IsDefaultFeed(std::wstring_view feedUrl) { return EqualsI(feedUrl, kDefaultFeedUrl); }

std::wstring AssetName(const Version& v, std::wstring_view suffix) { return L"openFences-" + FormatVersion(v) + std::wstring(suffix); }

std::wstring DigestHex(const nlohmann::json& asset) {
    if (!asset.contains("digest") || !asset["digest"].is_string()) return {};
    std::string d = asset["digest"].get<std::string>();
    if (d.rfind("sha256:", 0) != 0) return {};
    std::wstring hex = ToLower(FromUtf8(d.substr(7)));
    if (hex.size() != 64 || !std::all_of(hex.begin(), hex.end(), [](wchar_t c) { return iswxdigit(c) != 0; })) return {};
    return hex;
}

}  // namespace

std::optional<Version> ParseVersion(std::wstring_view text) {
    std::wstring t = Trim(text);
    std::wstring_view s = t;
    if (!s.empty() && (s[0] == L'v' || s[0] == L'V')) s.remove_prefix(1);
    Version v;
    size_t pos = 0;
    if (!ParseInt(s, pos, v.major) || pos >= s.size() || s[pos++] != L'.') return std::nullopt;
    if (!ParseInt(s, pos, v.minor) || pos >= s.size() || s[pos++] != L'.') return std::nullopt;
    if (!ParseInt(s, pos, v.patch)) return std::nullopt;
    if (pos < s.size() && s[pos] != L'-' && s[pos] != L'+') return std::nullopt;
    return v;
}

std::wstring FormatVersion(const Version& v) {
    return std::to_wstring(v.major) + L"." + std::to_wstring(v.minor) + L"." + std::to_wstring(v.patch);
}

std::optional<int64_t> ParseIsoUtc(std::string_view text) {
    // YYYY-MM-DDTHH:MM:SSZ (GitHub's format); fractional seconds are tolerated.
    int y, mo, d, h, mi, s;
    char tail = 0;
    std::string str(text);
    if (sscanf_s(str.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%c", &y, &mo, &d, &h, &mi, &s, &tail, 1) < 6) return std::nullopt;
    if (y < 2000 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 60) return std::nullopt;
    if (str.back() != 'Z') return std::nullopt;
    std::tm tm{};
    tm.tm_year = y - 1900;
    tm.tm_mon = mo - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = s;
    __time64_t t = _mkgmtime64(&tm);
    if (t == -1) return std::nullopt;
    return static_cast<int64_t>(t);
}

std::optional<Release> ParseLatestRelease(std::string_view json) {
    nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    if (j.value("draft", false) || j.value("prerelease", false)) return std::nullopt;
    if (!j.contains("tag_name") || !j["tag_name"].is_string() || !j.contains("published_at") || !j["published_at"].is_string()) return std::nullopt;
    Release r;
    r.tag = FromUtf8(j["tag_name"].get<std::string>());
    auto version = ParseVersion(r.tag);
    auto published = ParseIsoUtc(j["published_at"].get<std::string>());
    if (!version || !published) return std::nullopt;
    r.version = *version;
    r.publishedUnix = *published;
    if (!j.contains("assets") || !j["assets"].is_array()) return std::nullopt;
    const std::wstring msiName = AssetName(r.version, L"-x64.msi"), zipName = AssetName(r.version, L"-x64-portable.zip");
    bool haveMsi = false;
    for (const auto& a : j["assets"]) {
        if (!a.is_object() || !a.contains("name") || !a["name"].is_string() || !a.contains("browser_download_url") ||
            !a["browser_download_url"].is_string())
            continue;
        Asset asset{FromUtf8(a["name"].get<std::string>()), FromUtf8(a["browser_download_url"].get<std::string>()), DigestHex(a)};
        if (EqualsI(asset.name, msiName)) {
            r.msi = asset;
            haveMsi = true;
        } else if (EqualsI(asset.name, zipName)) {
            r.zip = asset;
        }
    }
    if (!haveMsi) return std::nullopt;
    return r;
}

DecisionResult Decide(const Version& current, const Release& latest, int64_t nowUnix, bool installImmediately, int minAgeDays) {
    if (!(latest.version > current)) return {Decision::UpToDate, 0};
    if (installImmediately) return {Decision::Install, 0};
    const int64_t needed = static_cast<int64_t>(minAgeDays) * 86400;
    const int64_t age = nowUnix - latest.publishedUnix;
    if (age >= needed) return {Decision::Install, 0};
    const int64_t left = needed - std::max<int64_t>(age, 0);
    return {Decision::TooNew, static_cast<int>(std::max<int64_t>(1, (left + 86399) / 86400))};
}

bool CheckDue(int64_t lastCheckUnix, int intervalDays, int64_t nowUnix) {
    if (lastCheckUnix <= 0 || nowUnix < lastCheckUnix) return true;
    return nowUnix - lastCheckUnix >= static_cast<int64_t>(NormalizeInterval(intervalDays)) * 86400;
}

int NormalizeInterval(int days) {
    int best = kDefaultCheckIntervalDays;
    for (int c : kCheckIntervalChoices)
        if (std::abs(c - days) < std::abs(best - days)) best = c;
    return best;
}

bool IsAcceptableFeedUrl(std::wstring_view url) {
    auto u = SplitUrl(url);
    if (!u) return false;
    return u->scheme == L"https" || (u->scheme == L"http" && IsLoopback(u->host));
}

bool IsAllowedUrl(std::wstring_view url, std::wstring_view feedUrl) {
    auto u = SplitUrl(url);
    if (!u) return false;
    if (IsDefaultFeed(feedUrl)) {
        if (u->scheme != L"https" || !u->port.empty()) return false;
        if (EqualsI(url, kDefaultFeedUrl)) return true;
        if (StartsWithI(url, kReleaseDownloadPrefix)) return true;
        // Where github.com redirects release downloads.
        return u->host == L"objects.githubusercontent.com" || u->host == L"release-assets.githubusercontent.com";
    }
    auto feed = SplitUrl(feedUrl);
    if (!feed || !IsAcceptableFeedUrl(feedUrl)) return false;
    if (u->scheme != feed->scheme || u->host != feed->host || u->port != feed->port) return false;
    return u->scheme == L"https" || IsLoopback(u->host);
}

}  // namespace of::update
