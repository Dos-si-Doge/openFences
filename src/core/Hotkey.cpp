// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "core/Hotkey.h"

#include <windows.h>

#include "core/Strings.h"

namespace of {

namespace {

struct KeyEntry {
    unsigned vk;
    const wchar_t* name;
};

constexpr KeyEntry kKeys[] = {
    {VK_SPACE, L"Space"},   {VK_TAB, L"Tab"},         {VK_RETURN, L"Enter"},     {VK_ESCAPE, L"Esc"},    {VK_BACK, L"Backspace"},
    {VK_PRIOR, L"PgUp"},    {VK_NEXT, L"PgDn"},       {VK_HOME, L"Home"},        {VK_END, L"End"},       {VK_INSERT, L"Insert"},
    {VK_DELETE, L"Delete"}, {VK_LEFT, L"Left"},       {VK_RIGHT, L"Right"},      {VK_UP, L"Up"},         {VK_DOWN, L"Down"},
    {VK_OEM_3, L"`"},       {VK_OEM_MINUS, L"-"},     {VK_OEM_PLUS, L"="},       {VK_OEM_4, L"["},       {VK_OEM_6, L"]"},
    {VK_OEM_5, L"\\"},      {VK_OEM_1, L";"},         {VK_OEM_7, L"'"},          {VK_OEM_COMMA, L","},   {VK_OEM_PERIOD, L"."},
    {VK_OEM_2, L"/"},       {VK_PAUSE, L"Pause"},     {VK_SNAPSHOT, L"PrtSc"},   {VK_NUMPAD0, L"Num0"},  {VK_NUMPAD1, L"Num1"},
    {VK_NUMPAD2, L"Num2"},  {VK_NUMPAD3, L"Num3"},    {VK_NUMPAD4, L"Num4"},     {VK_NUMPAD5, L"Num5"},  {VK_NUMPAD6, L"Num6"},
    {VK_NUMPAD7, L"Num7"},  {VK_NUMPAD8, L"Num8"},    {VK_NUMPAD9, L"Num9"},
};

}  // namespace

std::wstring KeyName(unsigned vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    for (const auto& k : kKeys)
        if (k.vk == vk) return k.name;
    return {};
}

std::optional<Hotkey> ParseHotkey(const std::wstring& text) {
    Hotkey hk;
    auto parts = SplitList(text, L"+");
    if (parts.empty()) return std::nullopt;
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::wstring& p = parts[i];
        bool last = i + 1 == parts.size();
        if (EqualsI(p, L"Ctrl") || EqualsI(p, L"Control")) hk.modifiers |= MOD_CONTROL;
        else if (EqualsI(p, L"Alt")) hk.modifiers |= MOD_ALT;
        else if (EqualsI(p, L"Shift")) hk.modifiers |= MOD_SHIFT;
        else if (EqualsI(p, L"Win") || EqualsI(p, L"Windows")) hk.modifiers |= MOD_WIN;
        else if (last) {
            if (p.size() == 1 && ((towupper(p[0]) >= L'A' && towupper(p[0]) <= L'Z') || (p[0] >= L'0' && p[0] <= L'9'))) {
                hk.vk = towupper(p[0]);
            } else if ((p[0] == L'F' || p[0] == L'f') && p.size() <= 3 && p.size() >= 2 && iswdigit(p[1])) {
                int n = _wtoi(p.c_str() + 1);
                if (n < 1 || n > 24) return std::nullopt;
                hk.vk = VK_F1 + n - 1;
            } else {
                for (const auto& k : kKeys)
                    if (EqualsI(p, k.name)) hk.vk = k.vk;
                if (!hk.vk) return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    if (!hk.vk) return std::nullopt;
    return hk;
}

std::wstring FormatHotkey(const Hotkey& hk) {
    if (!hk.Valid()) return {};
    std::wstring out;
    if (hk.modifiers & MOD_CONTROL) out += L"Ctrl+";
    if (hk.modifiers & MOD_WIN) out += L"Win+";
    if (hk.modifiers & MOD_ALT) out += L"Alt+";
    if (hk.modifiers & MOD_SHIFT) out += L"Shift+";
    return out + KeyName(hk.vk);
}

}  // namespace of
