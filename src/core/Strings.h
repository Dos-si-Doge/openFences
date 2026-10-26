// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace of {

std::string ToUtf8(std::wstring_view text);
std::wstring FromUtf8(std::string_view text);

std::wstring ToLower(std::wstring_view text);
bool EqualsI(std::wstring_view a, std::wstring_view b);
bool StartsWithI(std::wstring_view text, std::wstring_view prefix);
bool ContainsI(std::wstring_view text, std::wstring_view needle);

// Case-insensitive wildcard match supporting '*' and '?'.
bool GlobMatch(std::wstring_view pattern, std::wstring_view text);

// Splits on any of the separator characters, trims whitespace and drops empty parts.
std::vector<std::wstring> SplitList(std::wstring_view text, std::wstring_view separators = L",; ");
std::wstring JoinList(const std::vector<std::wstring>& parts, std::wstring_view separator = L", ");
std::wstring Trim(std::wstring_view text);

// Expands %VAR% references using the current process environment.
std::wstring ExpandEnv(std::wstring_view text);

// Returns the extension including the dot, lower-cased ("" when none).
std::wstring ExtensionOf(std::wstring_view fileName);

// Random identifier like "g-3fa9c2d1".
std::string NewId(std::string_view prefix);

// Replaces characters that are invalid in file names.
std::wstring SanitizeFileName(std::wstring_view text);

}  // namespace of
