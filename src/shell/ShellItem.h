#pragma once

#include <windows.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>

#include "core/Rules.h"

namespace of {

using Microsoft::WRL::ComPtr;

struct PidlDeleter {
    void operator()(ITEMIDLIST* p) const { CoTaskMemFree(p); }
};
using PidlPtr = std::unique_ptr<ITEMIDLIST, PidlDeleter>;
using SharedPidl = std::shared_ptr<ITEMIDLIST>;

SharedPidl MakeShared(PIDLIST_ABSOLUTE owned);
SharedPidl ClonePidl(PCIDLIST_ABSOLUTE pidl);
std::wstring ParsingName(PCIDLIST_ABSOLUTE pidl);
std::wstring FileSystemPath(PCIDLIST_ABSOLUTE pidl);
// Absolute PIDL for a path or shell parsing name ("::{GUID}", "shell:Downloads"); nullptr on failure.
SharedPidl ParsePath(const std::wstring& path);
SharedPidl KnownFolderPidl(REFKNOWNFOLDERID id);

// Immutable snapshot of one shell item, produced on a worker thread and shared with the UI thread.
struct ShellItem {
    std::wstring key;          // identity inside its folder: desktop items use the in-folder parsing name
    std::wstring parsingName;  // absolute parsing name
    std::wstring displayName;
    std::wstring editName;     // name shown in the rename box
    std::wstring typeName;
    SharedPidl pidl;           // absolute
    bool isFolder = false;
    bool isFileSystem = false;
    bool isLink = false;
    bool isBrowsable = false;  // a folder we can navigate into (portal navigation)
    uint64_t created = 0;
    uint64_t modified = 0;
    uint64_t size = 0;
    std::wstring linkTarget;
};
using ShellItemPtr = std::shared_ptr<const ShellItem>;

RuleItemInfo ToRuleInfo(const ShellItem& item);

// Splits items that share one parent into (parent folder, child IDs). Fails if parents differ.
bool GetParentAndChildren(const std::vector<ShellItemPtr>& items, ComPtr<IShellFolder>& parent, std::vector<PCUITEMID_CHILD>& children);

bool OpenItem(HWND owner, const ShellItem& item, const wchar_t* verb = nullptr);
// Opens a folder in a new Explorer window.
void OpenInExplorer(PCIDLIST_ABSOLUTE folder);
// Shows the item selected in Explorer.
void RevealInExplorer(const ShellItem& item);

}  // namespace of
