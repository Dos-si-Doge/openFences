// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "ui/InlineEditor.h"

#include <commctrl.h>

namespace of::inline_editor {

namespace {
HWND g_edit = nullptr;
HFONT g_font = nullptr;
std::function<void(const std::wstring&)> g_commit;
bool g_closing = false;

void Close(bool commit) {
    if (!g_edit || g_closing) return;
    g_closing = true;
    std::wstring text;
    if (commit) {
        int len = GetWindowTextLengthW(g_edit);
        text.resize(static_cast<size_t>(len) + 1);
        GetWindowTextW(g_edit, text.data(), len + 1);
        text.resize(static_cast<size_t>(len));
    }
    auto cb = std::move(g_commit);
    g_commit = nullptr;
    HWND edit = g_edit;
    g_edit = nullptr;
    DestroyWindow(edit);
    if (g_font) DeleteObject(g_font);
    g_font = nullptr;
    g_closing = false;
    if (commit && cb) cb(text);
}

LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS | DefSubclassProc(hwnd, msg, wp, lp);
        case WM_KEYDOWN:
            if (wp == VK_RETURN) {
                Close(true);
                return 0;
            }
            if (wp == VK_ESCAPE) {
                Close(false);
                return 0;
            }
            break;
        case WM_CHAR:
            if (wp == VK_RETURN || wp == VK_ESCAPE) return 0;
            break;
        case WM_KILLFOCUS:
            PostMessageW(hwnd, WM_APP + 50, 0, 0);
            break;
        case WM_APP + 50:
            if (GetFocus() != hwnd) Close(true);
            return 0;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, EditProc, 1);
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}
}  // namespace

void Show(HWND owner, const RectI& r, const std::wstring& text, float fontPx, bool selectStem, std::function<void(const std::wstring&)> onCommit) {
    Close(false);
    g_commit = std::move(onCommit);
    g_font = CreateFontW(-static_cast<int>(fontPx), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_edit = CreateWindowExW(WS_EX_TOOLWINDOW, L"EDIT", text.c_str(), WS_POPUP | WS_BORDER | ES_AUTOHSCROLL | ES_CENTER, r.left, r.top, r.Width(),
                             r.Height(), owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_edit) return;
    SendMessageW(g_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    SetWindowSubclass(g_edit, EditProc, 1, 0);
    int selEnd = static_cast<int>(text.size());
    if (selectStem) {
        size_t dot = text.find_last_of(L'.');
        if (dot != std::wstring::npos && dot > 0) selEnd = static_cast<int>(dot);
    }
    SendMessageW(g_edit, EM_SETSEL, 0, selEnd);
    ShowWindow(g_edit, SW_SHOW);
    SetForegroundWindow(g_edit);
    SetFocus(g_edit);
}

void Cancel() { Close(false); }
bool Active() { return g_edit != nullptr; }

}  // namespace of::inline_editor
