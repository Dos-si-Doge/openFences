#include "core/Strings.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <random>

namespace of {

std::string ToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring FromUtf8(std::string_view text) {
    if (text.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), len);
    return out;
}

std::wstring ToLower(std::wstring_view text) {
    std::wstring out(text);
    if (!out.empty()) {
        CharLowerBuffW(out.data(), static_cast<DWORD>(out.size()));
    }
    return out;
}

bool EqualsI(std::wstring_view a, std::wstring_view b) {
    return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

bool StartsWithI(std::wstring_view text, std::wstring_view prefix) {
    return text.size() >= prefix.size() && EqualsI(text.substr(0, prefix.size()), prefix);
}

bool ContainsI(std::wstring_view text, std::wstring_view needle) {
    if (needle.empty()) return true;
    return ToLower(text).find(ToLower(needle)) != std::wstring::npos;
}

bool GlobMatch(std::wstring_view pattern, std::wstring_view text) {
    std::wstring p = ToLower(pattern);
    std::wstring t = ToLower(text);
    size_t pi = 0, ti = 0, star = std::wstring::npos, mark = 0;
    while (ti < t.size()) {
        if (pi < p.size() && (p[pi] == L'?' || p[pi] == t[ti])) {
            ++pi;
            ++ti;
        } else if (pi < p.size() && p[pi] == L'*') {
            star = pi++;
            mark = ti;
        } else if (star != std::wstring::npos) {
            pi = star + 1;
            ti = ++mark;
        } else {
            return false;
        }
    }
    while (pi < p.size() && p[pi] == L'*') ++pi;
    return pi == p.size();
}

std::wstring Trim(std::wstring_view text) {
    size_t b = 0, e = text.size();
    while (b < e && std::iswspace(text[b])) ++b;
    while (e > b && std::iswspace(text[e - 1])) --e;
    return std::wstring(text.substr(b, e - b));
}

std::vector<std::wstring> SplitList(std::wstring_view text, std::wstring_view separators) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || separators.find(text[i]) != std::wstring_view::npos) {
            std::wstring part = Trim(text.substr(start, i - start));
            if (!part.empty()) parts.push_back(std::move(part));
            start = i + 1;
        }
    }
    return parts;
}

std::wstring JoinList(const std::vector<std::wstring>& parts, std::wstring_view separator) {
    std::wstring out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += separator;
        out += parts[i];
    }
    return out;
}

std::wstring ExpandEnv(std::wstring_view text) {
    if (text.find(L'%') == std::wstring_view::npos) return std::wstring(text);
    std::wstring in(text);
    DWORD len = ExpandEnvironmentStringsW(in.c_str(), nullptr, 0);
    if (len == 0) return in;
    std::wstring out(len, L'\0');
    ExpandEnvironmentStringsW(in.c_str(), out.data(), len);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

std::wstring ExtensionOf(std::wstring_view fileName) {
    size_t slash = fileName.find_last_of(L"\\/");
    size_t dot = fileName.find_last_of(L'.');
    if (dot == std::wstring_view::npos || (slash != std::wstring_view::npos && dot < slash) || dot == 0) return {};
    return ToLower(fileName.substr(dot));
}

std::string NewId(std::string_view prefix) {
    static std::mt19937_64 rng{std::random_device{}()};
    static const char* hex = "0123456789abcdef";
    uint64_t v = rng();
    std::string out(prefix);
    out += '-';
    for (int i = 0; i < 10; ++i) {
        out += hex[v & 0xF];
        v >>= 4;
    }
    return out;
}

std::wstring SanitizeFileName(std::wstring_view text) {
    std::wstring out;
    for (wchar_t c : text) {
        if (c < 32 || wcschr(L"<>:\"/\\|?*", c)) {
            out += L'_';
        } else {
            out += c;
        }
    }
    out = Trim(out);
    if (out.size() > 60) out.resize(60);
    return out;
}

}  // namespace of
