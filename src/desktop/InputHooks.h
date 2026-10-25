#pragma once

#include <windows.h>

#include <vector>

#include "core/Hotkey.h"
#include "core/MonitorConfig.h"

namespace of {

constexpr UINT WM_APP_WIN_D = WM_APP + 2;       // Win+D was intercepted
constexpr UINT WM_APP_FOREGROUND = WM_APP + 3;  // wParam = new foreground window

// Low-level keyboard hook that swallows Win+D (opt-in "Desktop Display" replacement).
namespace windhook {
bool Install(HWND notify);
void Uninstall();
bool Installed();
}  // namespace windhook

// Out-of-context WinEvent hook reporting foreground changes (auto-hide, Peek dismissal).
namespace foreground {
bool Install(HWND notify);
void Uninstall();
}  // namespace foreground

// Screen rectangle of the Peek hot corner on the primary monitor.
RectI HotCornerRect(HotCorner corner, const std::vector<MonitorDesc>& monitors, int sizePx);

// Default Peek hotkey: Win+Space unless several keyboard layouts are installed (Windows uses Win+Space
// to switch input language), in which case Ctrl+Alt+Space.
Hotkey DefaultPeekHotkey();

// True while the given key (and the modifiers of `hk`) are held down.
bool IsHotkeyHeld(const Hotkey& hk);

}  // namespace of
