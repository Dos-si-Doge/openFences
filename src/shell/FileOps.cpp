#include "shell/FileOps.h"

#include <shlwapi.h>
#include <shobjidl.h>

#include "core/Strings.h"

namespace of {

std::optional<std::wstring> RenameItem(HWND owner, const ShellItem& item, const std::wstring& newName, bool desktopKey) {
    ComPtr<IShellFolder> parent;
    PCUITEMID_CHILD child = nullptr;
    if (FAILED(SHBindToParent(item.pidl.get(), IID_PPV_ARGS(&parent), &child))) return std::nullopt;
    PITEMID_CHILD renamed = nullptr;
    if (FAILED(parent->SetNameOf(owner, child, newName.c_str(), SHGDN_INFOLDER | SHGDN_FOREDITING, &renamed)) || !renamed)
        return std::nullopt;
    std::wstring key;
    STRRET sr{};
    SHGDNF flags = desktopKey ? (SHGDN_INFOLDER | SHGDN_FORPARSING) : SHGDN_FORPARSING;
    if (SUCCEEDED(parent->GetDisplayNameOf(renamed, flags, &sr))) {
        wchar_t buf[MAX_PATH * 2];
        if (SUCCEEDED(StrRetToBufW(&sr, renamed, buf, static_cast<UINT>(std::size(buf))))) key = buf;
    }
    CoTaskMemFree(renamed);
    if (key.empty()) return std::nullopt;
    return key;
}

bool MoveItemsToFolder(HWND owner, const std::vector<ShellItemPtr>& items, const std::wstring& folderPath) {
    if (items.empty()) return true;
    SharedPidl dest = ParsePath(folderPath);
    if (!dest) {
        std::wstring expanded = ExpandEnv(folderPath);
        if (SHCreateDirectoryExW(nullptr, expanded.c_str(), nullptr) == ERROR_SUCCESS) dest = ParsePath(expanded);
    }
    if (!dest) return false;
    ComPtr<IFileOperation> op;
    if (FAILED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&op)))) return false;
    op->SetOwnerWindow(owner);
    op->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR | FOFX_ADDUNDORECORD | FOF_RENAMEONCOLLISION);
    ComPtr<IShellItem> destItem;
    if (FAILED(SHCreateItemFromIDList(dest.get(), IID_PPV_ARGS(&destItem)))) return false;
    for (const auto& item : items) {
        ComPtr<IShellItem> si;
        if (SUCCEEDED(SHCreateItemFromIDList(item->pidl.get(), IID_PPV_ARGS(&si)))) op->MoveItem(si.Get(), destItem.Get(), nullptr, nullptr);
    }
    return SUCCEEDED(op->PerformOperations());
}

std::wstring PickFolder(HWND owner, const std::wstring& title, const std::wstring& initial) {
    ComPtr<IFileOpenDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return {};
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dlg->SetTitle(title.c_str());
    if (!initial.empty()) {
        ComPtr<IShellItem> folder;
        if (SUCCEEDED(SHCreateItemFromParsingName(ExpandEnv(initial).c_str(), nullptr, IID_PPV_ARGS(&folder)))) dlg->SetFolder(folder.Get());
    }
    if (dlg->Show(owner) != S_OK) return {};
    ComPtr<IShellItem> result;
    if (FAILED(dlg->GetResult(&result))) return {};
    PWSTR path = nullptr;
    std::wstring out;
    if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path))) out = path;
    CoTaskMemFree(path);
    return out;
}

std::wstring PickJsonFile(HWND owner, bool save, const std::wstring& title, const std::wstring& defaultName) {
    ComPtr<IFileDialog> dlg;
    HRESULT hr = save ? CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))
                      : CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr)) return {};
    COMDLG_FILTERSPEC filters[] = {{L"openFences layout (*.json)", L"*.json"}, {L"All files", L"*.*"}};
    dlg->SetFileTypes(2, filters);
    dlg->SetDefaultExtension(L"json");
    dlg->SetTitle(title.c_str());
    if (!defaultName.empty()) dlg->SetFileName(defaultName.c_str());
    if (dlg->Show(owner) != S_OK) return {};
    ComPtr<IShellItem> result;
    if (FAILED(dlg->GetResult(&result))) return {};
    PWSTR path = nullptr;
    std::wstring out;
    if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path))) out = path;
    CoTaskMemFree(path);
    return out;
}

}  // namespace of
