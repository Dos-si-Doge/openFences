#pragma once

#include <vector>

#include "shell/ShellItem.h"

namespace of {

struct EnumResult {
    HRESULT hr = S_OK;
    std::vector<ShellItemPtr> items;
};

// Explorer's "show hidden files" settings.
struct HiddenSettings {
    bool showHidden = false;
    bool showSuperHidden = false;
};
HiddenSettings ReadHiddenSettings();

// Enumerates the merged desktop (user + public + visible virtual items). Worker thread.
EnumResult EnumerateDesktop(const HiddenSettings& hidden);
// Enumerates a folder for a portal. Keys are absolute parsing names. Worker thread.
EnumResult EnumerateFolder(PCIDLIST_ABSOLUTE folder, const HiddenSettings& hidden);

// Builds one item (used for single-item refreshes). parentAbs + child must form the item's absolute PIDL.
ShellItemPtr MakeItem(IShellFolder* parent, PCIDLIST_ABSOLUTE parentAbs, PCUITEMID_CHILD child, bool desktopKeys);

}  // namespace of
