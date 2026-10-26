// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "shell/ShellItem.h"

namespace of {

// Renames an item the way Explorer does (IShellFolder::SetNameOf, so hidden extensions are kept).
// Returns the item's new key (in-folder parsing name when desktopKey, else absolute parsing name).
std::optional<std::wstring> RenameItem(HWND owner, const ShellItem& item, const std::wstring& newName, bool desktopKey);
// Moves items into a folder (used by "keep off desktop" rules).
bool MoveItemsToFolder(HWND owner, const std::vector<ShellItemPtr>& items, const std::wstring& folderPath);
// Shows the folder picker. Returns "" when cancelled.
std::wstring PickFolder(HWND owner, const std::wstring& title, const std::wstring& initial = {});
// Shows an open/save dialog for JSON files. Returns "" when cancelled.
std::wstring PickJsonFile(HWND owner, bool save, const std::wstring& title, const std::wstring& defaultName = {});

}  // namespace of
