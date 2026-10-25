#include "shell/ShellItem.h"

#include <shellapi.h>
#include <shlwapi.h>

#include "core/Strings.h"

namespace of {

SharedPidl MakeShared(PIDLIST_ABSOLUTE owned) {
    if (!owned) return nullptr;
    // PIDLIST_ABSOLUTE is __unaligned; shell allocations are aligned in practice on x64.
    return SharedPidl(const_cast<ITEMIDLIST*>(reinterpret_cast<const ITEMIDLIST*>(owned)), PidlDeleter{});
}

SharedPidl ClonePidl(PCIDLIST_ABSOLUTE pidl) {
    if (!pidl) return nullptr;
    return MakeShared(ILCloneFull(pidl));
}

std::wstring ParsingName(PCIDLIST_ABSOLUTE pidl) {
    PWSTR name = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetNameFromIDList(pidl, SIGDN_DESKTOPABSOLUTEPARSING, &name))) out = name;
    CoTaskMemFree(name);
    return out;
}

std::wstring FileSystemPath(PCIDLIST_ABSOLUTE pidl) {
    PWSTR name = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetNameFromIDList(pidl, SIGDN_FILESYSPATH, &name))) out = name;
    CoTaskMemFree(name);
    return out;
}

SharedPidl ParsePath(const std::wstring& path) {
    std::wstring expanded = ExpandEnv(path);
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (FAILED(SHParseDisplayName(expanded.c_str(), nullptr, &pidl, 0, nullptr))) return nullptr;
    return MakeShared(pidl);
}

SharedPidl KnownFolderPidl(REFKNOWNFOLDERID id) {
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (FAILED(SHGetKnownFolderIDList(id, 0, nullptr, &pidl))) return nullptr;
    return MakeShared(pidl);
}

RuleItemInfo ToRuleInfo(const ShellItem& item) {
    RuleItemInfo info;
    info.name = item.isFileSystem ? std::wstring(PathFindFileNameW(item.parsingName.c_str())) : item.key;
    info.linkTarget = item.linkTarget;
    info.isFileSystem = item.isFileSystem;
    info.isFolder = item.isFolder;
    info.isShortcut = item.isLink;
    info.created = item.created;
    info.modified = item.modified;
    return info;
}

bool GetParentAndChildren(const std::vector<ShellItemPtr>& items, ComPtr<IShellFolder>& parent, std::vector<PCUITEMID_CHILD>& children) {
    children.clear();
    parent.Reset();
    PIDLIST_ABSOLUTE firstParent = nullptr;
    bool ok = true;
    for (const auto& item : items) {
        if (!item || !item->pidl) continue;
        PCUITEMID_CHILD child = nullptr;
        if (!parent) {
            if (FAILED(SHBindToParent(item->pidl.get(), IID_PPV_ARGS(&parent), &child))) return false;
            firstParent = ILCloneFull(item->pidl.get());
            ILRemoveLastID(firstParent);
        } else {
            PIDLIST_ABSOLUTE p = ILCloneFull(item->pidl.get());
            ILRemoveLastID(p);
            bool same = ILIsEqual(p, firstParent) != FALSE;
            CoTaskMemFree(p);
            if (!same) {
                ok = false;
                break;
            }
            child = ILFindLastID(item->pidl.get());
        }
        children.push_back(child);
    }
    CoTaskMemFree(firstParent);
    return ok && parent && !children.empty();
}

bool OpenItem(HWND owner, const ShellItem& item, const wchar_t* verb) {
    SHELLEXECUTEINFOW sei{sizeof(sei)};
    sei.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_ASYNCOK | SEE_MASK_FLAG_LOG_USAGE;
    sei.hwnd = owner;
    sei.lpVerb = verb;
    sei.lpIDList = item.pidl.get();
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

void OpenInExplorer(PCIDLIST_ABSOLUTE folder) {
    SHELLEXECUTEINFOW sei{sizeof(sei)};
    sei.fMask = SEE_MASK_IDLIST | SEE_MASK_ASYNCOK;
    sei.lpVerb = L"explore";
    sei.lpIDList = const_cast<ITEMIDLIST*>(folder);
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        sei.lpVerb = L"open";
        ShellExecuteExW(&sei);
    }
}

void RevealInExplorer(const ShellItem& item) {
    SHOpenFolderAndSelectItems(item.pidl.get(), 0, nullptr, 0);
}

}  // namespace of
