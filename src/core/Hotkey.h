// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <optional>
#include <string>

namespace of {

struct Hotkey {
    unsigned modifiers = 0;  // MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN
    unsigned vk = 0;
    bool operator==(const Hotkey&) const = default;
    bool Valid() const { return vk != 0; }
};

// "Ctrl+Alt+Space", "Win+Shift+F" (case-insensitive). Returns nullopt for unknown keys.
std::optional<Hotkey> ParseHotkey(const std::wstring& text);
std::wstring FormatHotkey(const Hotkey& hk);
std::wstring KeyName(unsigned vk);

}  // namespace of
