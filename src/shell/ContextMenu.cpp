#include "shell/ContextMenu.h"

#include "core/Log.h"

namespace of {

namespace {
ComPtr<IContextMenu2> g_menu2;
ComPtr<IContextMenu3> g_menu3;

void Invoke(HWND owner, IContextMenu* cm, UINT offset, POINT pt) {
    CMINVOKECOMMANDINFOEX ici{sizeof(ici)};
    ici.fMask = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE | CMIC_MASK_ASYNCOK;
    if (GetKeyState(VK_CONTROL) < 0) ici.fMask |= CMIC_MASK_CONTROL_DOWN;
    if (GetKeyState(VK_SHIFT) < 0) ici.fMask |= CMIC_MASK_SHIFT_DOWN;
    ici.hwnd = owner;
    ici.lpVerb = MAKEINTRESOURCEA(offset);
    ici.lpVerbW = MAKEINTRESOURCEW(offset);
    ici.nShow = SW_SHOWNORMAL;
    ici.ptInvoke = pt;
    HRESULT hr = cm->InvokeCommand(reinterpret_cast<CMINVOKECOMMANDINFO*>(&ici));
    if (FAILED(hr)) log::Debug(L"InvokeCommand({}) failed 0x{:08x}", offset, static_cast<unsigned>(hr));
}
}  // namespace

ComPtr<IContextMenu> ItemContextMenu(HWND owner, const std::vector<ShellItemPtr>& items) {
    ComPtr<IShellFolder> parent;
    std::vector<PCUITEMID_CHILD> children;
    if (!GetParentAndChildren(items, parent, children)) return nullptr;
    ComPtr<IContextMenu> cm;
    parent->GetUIObjectOf(owner, static_cast<UINT>(children.size()), children.data(), IID_IContextMenu, nullptr,
                          reinterpret_cast<void**>(cm.GetAddressOf()));
    return cm;
}

ComPtr<IContextMenu> BackgroundContextMenu(HWND owner, PCIDLIST_ABSOLUTE folder) {
    ComPtr<IShellFolder> desktop;
    if (FAILED(SHGetDesktopFolder(&desktop))) return nullptr;
    ComPtr<IShellFolder> target = desktop;
    if (folder && !ILIsEmpty(folder)) {
        target.Reset();
        if (FAILED(desktop->BindToObject(folder, nullptr, IID_PPV_ARGS(&target)))) return nullptr;
    }
    ComPtr<IContextMenu> cm;
    target->CreateViewObject(owner, IID_PPV_ARGS(&cm));
    return cm;
}

ShellMenuResult TrackShellMenu(HWND owner, IContextMenu* cm, HMENU menu, UINT insertPos, POINT pt, UINT cmfFlags) {
    ShellMenuResult result;
    if (cm) {
        if (GetKeyState(VK_SHIFT) < 0) cmfFlags |= CMF_EXTENDEDVERBS;
        cm->QueryContextMenu(menu, insertPos, 1, kFirstAppMenuId - 1, cmfFlags);
        cm->QueryInterface(IID_PPV_ARGS(&g_menu2));
        cm->QueryInterface(IID_PPV_ARGS(&g_menu3));
    }
    SetForegroundWindow(owner);
    UINT id = static_cast<UINT>(TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, owner, nullptr));
    PostMessageW(owner, WM_NULL, 0, 0);
    if (id >= kFirstAppMenuId) {
        result.appId = id;
    } else if (id > 0 && cm) {
        wchar_t verb[64] = {};
        if (SUCCEEDED(cm->GetCommandString(id - 1, GCS_VERBW, nullptr, reinterpret_cast<char*>(verb), static_cast<UINT>(std::size(verb)))) &&
            _wcsicmp(verb, L"rename") == 0) {
            result.renameRequested = true;
        } else {
            Invoke(owner, cm, id - 1, pt);
            result.invoked = true;
        }
    }
    g_menu2.Reset();
    g_menu3.Reset();
    return result;
}

UINT TrackAppMenu(HWND owner, HMENU menu, POINT pt) {
    SetForegroundWindow(owner);
    UINT id = static_cast<UINT>(TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, owner, nullptr));
    PostMessageW(owner, WM_NULL, 0, 0);
    return id;
}

bool InvokeVerb(HWND owner, const std::vector<ShellItemPtr>& items, const char* verb) {
    ComPtr<IContextMenu> cm = ItemContextMenu(owner, items);
    if (!cm) return false;
    HMENU menu = CreatePopupMenu();
    cm->QueryContextMenu(menu, 0, 1, kFirstAppMenuId - 1, CMF_NORMAL);
    CMINVOKECOMMANDINFOEX ici{sizeof(ici)};
    ici.fMask = CMIC_MASK_ASYNCOK;
    if (GetKeyState(VK_SHIFT) < 0) ici.fMask |= CMIC_MASK_SHIFT_DOWN;
    ici.hwnd = owner;
    ici.lpVerb = verb;
    ici.nShow = SW_SHOWNORMAL;
    HRESULT hr = cm->InvokeCommand(reinterpret_cast<CMINVOKECOMMANDINFO*>(&ici));
    DestroyMenu(menu);
    return SUCCEEDED(hr);
}

bool HandleShellMenuMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT* result) {
    (void)hwnd;
    if (msg != WM_INITMENUPOPUP && msg != WM_DRAWITEM && msg != WM_MEASUREITEM && msg != WM_MENUCHAR) return false;
    if (g_menu3) {
        LRESULT r = 0;
        if (SUCCEEDED(g_menu3->HandleMenuMsg2(msg, wp, lp, &r))) {
            *result = r;
            return true;
        }
    } else if (g_menu2) {
        if (SUCCEEDED(g_menu2->HandleMenuMsg(msg, wp, lp))) {
            *result = 0;
            return true;
        }
    }
    return false;
}

}  // namespace of
