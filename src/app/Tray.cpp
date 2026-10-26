// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "app/Tray.h"

#include <commctrl.h>
#include <shellapi.h>

#include "app/Controller.h"
#include "resource.h"

namespace of {

namespace {
// Fixed GUID so Windows remembers the icon's notification-area placement across runs.
constexpr GUID kTrayGuid = {0x6f1a4c52, 0x8a3e, 0x4d0b, {0x9b, 0x61, 0x2f, 0x4e, 0x0c, 0x7a, 0x15, 0xd3}};

NOTIFYICONDATAW BaseData(HWND owner) {
    NOTIFYICONDATAW nid{sizeof(nid)};
    nid.hWnd = owner;
    nid.uFlags = NIF_GUID;
    nid.guidItem = kTrayGuid;
    return nid;
}
}  // namespace

void Tray::Add() {
    NOTIFYICONDATAW nid = BaseData(m_owner);
    nid.uFlags |= NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_APP_TRAY;
    LoadIconMetric(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP), LIM_SMALL, &nid.hIcon);
    wcscpy_s(nid.szTip, L"openFences");
    // A stale entry with our GUID (e.g. after a crash) makes NIM_ADD fail; remove it first.
    Shell_NotifyIconW(NIM_DELETE, &nid);
    m_added = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
    if (nid.hIcon) DestroyIcon(nid.hIcon);
}

void Tray::Remove() {
    if (!m_added) return;
    NOTIFYICONDATAW nid = BaseData(m_owner);
    Shell_NotifyIconW(NIM_DELETE, &nid);
    m_added = false;
}

void Tray::SetTip(const std::wstring& tip) {
    NOTIFYICONDATAW nid = BaseData(m_owner);
    nid.uFlags |= NIF_TIP | NIF_SHOWTIP;
    wcsncpy_s(nid.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void Tray::Balloon(const std::wstring& title, const std::wstring& text) {
    NOTIFYICONDATAW nid = BaseData(m_owner);
    nid.uFlags |= NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

}  // namespace of
