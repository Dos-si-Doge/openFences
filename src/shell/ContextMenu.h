#pragma once

#include <vector>

#include "shell/ShellItem.h"

namespace of {

// Command ids at or above this value belong to openFences; below it they belong to the shell menu.
constexpr UINT kFirstAppMenuId = 0x8000;

struct ShellMenuResult {
    UINT appId = 0;               // openFences command chosen (>= kFirstAppMenuId), or 0
    bool renameRequested = false; // the shell "Rename" verb was chosen (handled inline by the caller)
    bool invoked = false;         // a shell command ran
};

ComPtr<IContextMenu> ItemContextMenu(HWND owner, const std::vector<ShellItemPtr>& items);
// Background menu of a folder (New, Display settings, Personalize, ... for the desktop).
ComPtr<IContextMenu> BackgroundContextMenu(HWND owner, PCIDLIST_ABSOLUTE folder);

// Merges the shell menu into `menu` at insertPos, tracks it at pt (screen) and executes the choice.
ShellMenuResult TrackShellMenu(HWND owner, IContextMenu* cm, HMENU menu, UINT insertPos, POINT pt, UINT cmfFlags);
// Tracks a plain popup with only openFences commands.
UINT TrackAppMenu(HWND owner, HMENU menu, POINT pt);

// Runs a canonical verb ("delete", "properties", "copy", "cut", "open") on the items.
bool InvokeVerb(HWND owner, const std::vector<ShellItemPtr>& items, const char* verb);

// Routes WM_INITMENUPOPUP / WM_DRAWITEM / WM_MEASUREITEM / WM_MENUCHAR to the active shell menu.
// Call first in every window procedure that can own a context menu.
bool HandleShellMenuMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT* result);

}  // namespace of
