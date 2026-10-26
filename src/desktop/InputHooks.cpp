// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "desktop/InputHooks.h"

#include <shellapi.h>

#include <algorithm>

namespace of {

namespace windhook {
namespace {
HHOOK g_hook = nullptr;
HWND g_notify = nullptr;
bool g_swallowUp = false;

void InjectDummyKey() {
    // An unassigned key between Win down and Win up stops the Start menu from opening.
    INPUT in[2]{};
    in[0].type = in[1].type = INPUT_KEYBOARD;
    in[0].ki.wVk = in[1].ki.wVk = 0xE8;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

LRESULT CALLBACK Proc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION) {
        auto* k = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
        bool injected = (k->flags & LLKHF_INJECTED) != 0;
        if (!injected && k->vkCode == 'D') {
            bool winDown = (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);
            bool otherMods = (GetAsyncKeyState(VK_CONTROL) & 0x8000) || (GetAsyncKeyState(VK_MENU) & 0x8000) ||
                             (GetAsyncKeyState(VK_SHIFT) & 0x8000);
            if ((wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN) && winDown && !otherMods) {
                if (!g_swallowUp) PostMessageW(g_notify, WM_APP_WIN_D, 0, 0);
                g_swallowUp = true;
                InjectDummyKey();
                return 1;
            }
            if ((wp == WM_KEYUP || wp == WM_SYSKEYUP) && g_swallowUp) {
                g_swallowUp = false;
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hook, code, wp, lp);
}
}  // namespace

bool Install(HWND notify) {
    if (g_hook) return true;
    g_notify = notify;
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, Proc, GetModuleHandleW(nullptr), 0);
    return g_hook != nullptr;
}

void Uninstall() {
    if (g_hook) UnhookWindowsHookEx(g_hook);
    g_hook = nullptr;
}

bool Installed() { return g_hook != nullptr; }
}  // namespace windhook

namespace foreground {
namespace {
HWINEVENTHOOK g_hook = nullptr;
HWND g_notify = nullptr;

void CALLBACK Proc(HWINEVENTHOOK, DWORD, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    PostMessageW(g_notify, WM_APP_FOREGROUND, reinterpret_cast<WPARAM>(hwnd), 0);
}
}  // namespace

bool Install(HWND notify) {
    if (g_hook) return true;
    g_notify = notify;
    g_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, Proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    return g_hook != nullptr;
}

void Uninstall() {
    if (g_hook) UnhookWinEvent(g_hook);
    g_hook = nullptr;
}
}  // namespace foreground

RectI HotCornerRect(HotCorner corner, const std::vector<MonitorDesc>& monitors, int s) {
    const MonitorDesc* m = PrimaryMonitor(monitors);
    if (!m) return {};
    const RectI& b = m->bounds;
    if (corner == HotCorner::TaskbarEnd) {
        APPBARDATA abd{sizeof(abd)};
        corner = HotCorner::BottomRight;
        if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
            switch (abd.uEdge) {
                case ABE_TOP: corner = HotCorner::TopRight; break;
                case ABE_LEFT: corner = HotCorner::BottomLeft; break;
                default: corner = HotCorner::BottomRight; break;
            }
        }
    }
    switch (corner) {
        case HotCorner::TopLeft: return RectI::FromXYWH(b.left, b.top, s, s);
        case HotCorner::TopRight: return RectI::FromXYWH(b.right - s, b.top, s, s);
        case HotCorner::BottomLeft: return RectI::FromXYWH(b.left, b.bottom - s, s, s);
        default: return RectI::FromXYWH(b.right - s, b.bottom - s, s, s);
    }
}

Hotkey DefaultPeekHotkey() {
    int layouts = GetKeyboardLayoutList(0, nullptr);
    if (layouts <= 1) return Hotkey{MOD_WIN, VK_SPACE};
    return Hotkey{MOD_CONTROL | MOD_ALT, VK_SPACE};
}

std::vector<Hotkey> PeekHotkeyCandidates() {
    std::vector<Hotkey> out{DefaultPeekHotkey()};
    for (Hotkey hk : {Hotkey{MOD_CONTROL | MOD_ALT, VK_SPACE}, Hotkey{MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_SPACE}})
        if (std::find(out.begin(), out.end(), hk) == out.end()) out.push_back(hk);
    return out;
}

bool IsHotkeyHeld(const Hotkey& hk) {
    if (!(GetAsyncKeyState(static_cast<int>(hk.vk)) & 0x8000)) return false;
    if ((hk.modifiers & MOD_WIN) && !((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000)) return false;
    if ((hk.modifiers & MOD_CONTROL) && !(GetAsyncKeyState(VK_CONTROL) & 0x8000)) return false;
    if ((hk.modifiers & MOD_ALT) && !(GetAsyncKeyState(VK_MENU) & 0x8000)) return false;
    if ((hk.modifiers & MOD_SHIFT) && !(GetAsyncKeyState(VK_SHIFT) & 0x8000)) return false;
    return true;
}

}  // namespace of
