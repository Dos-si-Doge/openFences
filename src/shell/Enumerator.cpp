#include "shell/Enumerator.h"

#include <shlwapi.h>

#include "core/Strings.h"

namespace of {

namespace {

std::wstring DisplayName(IShellFolder* folder, PCUITEMID_CHILD child, SHGDNF flags) {
    STRRET sr{};
    if (FAILED(folder->GetDisplayNameOf(child, flags, &sr))) return {};
    wchar_t buf[MAX_PATH * 2];
    if (FAILED(StrRetToBufW(&sr, child, buf, static_cast<UINT>(std::size(buf))))) return {};
    return buf;
}

uint64_t ToTicks(const FILETIME& ft) { return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime; }

std::wstring ResolveLinkTarget(const std::wstring& path) {
    ComPtr<IShellLinkW> link;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) return {};
    ComPtr<IPersistFile> file;
    if (FAILED(link.As(&file)) || FAILED(file->Load(path.c_str(), STGM_READ))) return {};
    wchar_t buf[MAX_PATH * 2] = {};
    if (SUCCEEDED(link->GetPath(buf, static_cast<int>(std::size(buf)), nullptr, SLGP_RAWPATH)) && buf[0]) return ExpandEnv(buf);
    PIDLIST_ABSOLUTE target = nullptr;
    std::wstring out;
    if (SUCCEEDED(link->GetIDList(&target)) && target) out = ParsingName(target);
    CoTaskMemFree(target);
    return out;
}

// Desktop virtual items (This PC, Recycle Bin, ...) follow Settings > Personalization > Themes > Desktop icon settings.
bool VirtualDesktopItemVisible(const std::wstring& key) {
    if (key.size() < 4 || key.rfind(L"::{", 0) != 0) return true;
    std::wstring guid = key.substr(2);
    if (size_t end = guid.find(L'}'); end != std::wstring::npos) guid.resize(end + 1);
    DWORD hidden = 1, size = sizeof(hidden);
    LSTATUS st = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel",
                              guid.c_str(), RRF_RT_REG_DWORD, nullptr, &hidden, &size);
    if (st == ERROR_SUCCESS) return hidden == 0;
    // Without an explicit setting Windows shows only the Recycle Bin.
    return EqualsI(guid, L"{645FF040-5081-101B-9F08-00AA002F954E}");
}

EnumResult Enumerate(IShellFolder* folder, PCIDLIST_ABSOLUTE folderAbs, const HiddenSettings& hidden, bool desktop) {
    EnumResult result;
    SHCONTF flags = SHCONTF_FOLDERS | SHCONTF_NONFOLDERS;
    if (hidden.showHidden) flags |= SHCONTF_INCLUDEHIDDEN;
    if (hidden.showSuperHidden) flags |= SHCONTF_INCLUDESUPERHIDDEN;
    ComPtr<IEnumIDList> en;
    result.hr = folder->EnumObjects(nullptr, flags, &en);
    if (FAILED(result.hr) || !en) {
        if (SUCCEEDED(result.hr)) result.hr = E_FAIL;
        return result;
    }
    if (result.hr == S_FALSE) return result;  // empty folder
    PITEMID_CHILD child = nullptr;
    while (en->Next(1, &child, nullptr) == S_OK) {
        ShellItemPtr item = MakeItem(folder, folderAbs, child, desktop);
        CoTaskMemFree(child);
        if (!item) continue;
        if (desktop && !VirtualDesktopItemVisible(item->key)) continue;
        result.items.push_back(std::move(item));
    }
    result.hr = S_OK;
    return result;
}

}  // namespace

HiddenSettings ReadHiddenSettings() {
    SHELLSTATEW ss{};
    SHGetSetSettings(&ss, SSF_SHOWALLOBJECTS | SSF_SHOWSUPERHIDDEN, FALSE);
    return {ss.fShowAllObjects != 0, ss.fShowSuperHidden != 0};
}

ShellItemPtr MakeItem(IShellFolder* parent, PCIDLIST_ABSOLUTE parentAbs, PCUITEMID_CHILD child, bool desktopKeys) {
    auto item = std::make_shared<ShellItem>();
    SFGAOF attrs = SFGAO_FOLDER | SFGAO_FILESYSTEM | SFGAO_LINK | SFGAO_STREAM | SFGAO_BROWSABLE | SFGAO_HIDDEN;
    if (FAILED(parent->GetAttributesOf(1, &child, &attrs))) attrs = 0;
    item->pidl = MakeShared(ILCombine(parentAbs, child));
    if (!item->pidl) return nullptr;
    item->parsingName = DisplayName(parent, child, SHGDN_FORPARSING);
    item->displayName = DisplayName(parent, child, SHGDN_INFOLDER | SHGDN_NORMAL);
    item->editName = DisplayName(parent, child, SHGDN_INFOLDER | SHGDN_FOREDITING);
    item->key = desktopKeys ? DisplayName(parent, child, SHGDN_INFOLDER | SHGDN_FORPARSING) : item->parsingName;
    if (item->key.empty()) return nullptr;
    if (item->displayName.empty()) item->displayName = item->key;
    item->isFileSystem = (attrs & SFGAO_FILESYSTEM) != 0;
    item->isLink = (attrs & SFGAO_LINK) != 0;
    // Zip files report FOLDER|STREAM; treat them as files.
    item->isFolder = (attrs & SFGAO_FOLDER) && !(attrs & SFGAO_STREAM);
    item->isBrowsable = item->isFolder && !item->isLink;

    if (item->isFileSystem) {
        std::wstring name = PathFindFileNameW(item->parsingName.c_str());
        if (EqualsI(name, L"desktop.ini")) return nullptr;
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (GetFileAttributesExW(item->parsingName.c_str(), GetFileExInfoStandard, &fad)) {
            item->created = ToTicks(fad.ftCreationTime);
            item->modified = ToTicks(fad.ftLastWriteTime);
            item->size = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
        }
        item->typeName = item->isFolder ? L"" : ExtensionOf(name);
        if (item->isLink && ExtensionOf(name) == L".lnk") item->linkTarget = ResolveLinkTarget(item->parsingName);
    }
    return item;
}

EnumResult EnumerateDesktop(const HiddenSettings& hidden) {
    ComPtr<IShellFolder> desktop;
    EnumResult result;
    result.hr = SHGetDesktopFolder(&desktop);
    if (FAILED(result.hr)) return result;
    // The desktop namespace root is the empty PIDL, so child IDs are already absolute.
    ITEMIDLIST empty{};
    return Enumerate(desktop.Get(), reinterpret_cast<PCIDLIST_ABSOLUTE>(&empty), hidden, true);
}

EnumResult EnumerateFolder(PCIDLIST_ABSOLUTE folderPidl, const HiddenSettings& hidden) {
    EnumResult result;
    ComPtr<IShellFolder> folder;
    if (ILIsEmpty(folderPidl)) {
        result.hr = SHGetDesktopFolder(&folder);
    } else {
        ComPtr<IShellFolder> desktop;
        result.hr = SHGetDesktopFolder(&desktop);
        if (SUCCEEDED(result.hr)) result.hr = desktop->BindToObject(folderPidl, nullptr, IID_PPV_ARGS(&folder));
    }
    if (FAILED(result.hr)) return result;
    return Enumerate(folder.Get(), folderPidl, hidden, false);
}

}  // namespace of
