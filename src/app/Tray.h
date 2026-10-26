// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <windows.h>

#include <string>

namespace of {

// Notification-area icon. Clicks arrive at the owner window as WM_APP_TRAY (NOTIFYICON_VERSION_4).
class Tray {
public:
    explicit Tray(HWND owner) : m_owner(owner) {}
    ~Tray() { Remove(); }
    void Add();
    void Remove();
    void SetTip(const std::wstring& tip);
    void Balloon(const std::wstring& title, const std::wstring& text);

private:
    HWND m_owner;
    bool m_added = false;
};

}  // namespace of
