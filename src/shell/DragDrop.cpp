#include "shell/DragDrop.h"

#include <shellapi.h>
#include <shlwapi.h>

namespace of {

InternalDrag& CurrentDrag() {
    static InternalDrag drag;
    return drag;
}

DWORD StartShellDrag(HWND source, const std::vector<ShellItemPtr>& items, const IconPixels* image, InternalDrag info) {
    ComPtr<IShellFolder> parent;
    std::vector<PCUITEMID_CHILD> children;
    if (!GetParentAndChildren(items, parent, children)) return DROPEFFECT_NONE;
    ComPtr<IDataObject> data;
    if (FAILED(parent->GetUIObjectOf(source, static_cast<UINT>(children.size()), children.data(), IID_IDataObject, nullptr,
                                     reinterpret_cast<void**>(data.GetAddressOf()))))
        return DROPEFFECT_NONE;

    if (image && image->width > 0) {
        ComPtr<IDragSourceHelper> helper;
        if (SUCCEEDED(CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&helper)))) {
            SHDRAGIMAGE di{};
            di.sizeDragImage = {image->width, image->height};
            di.ptOffset = {image->width / 2, image->height / 2};
            di.hbmpDragImage = HBitmapFromPixels(*image);
            di.crColorKey = CLR_NONE;
            if (FAILED(helper->InitializeFromBitmap(&di, data.Get()))) DeleteObject(di.hbmpDragImage);
        }
    }

    InternalDrag& drag = CurrentDrag();
    drag = std::move(info);
    drag.active = true;
    drag.items = items;
    drag.dropHandledInternally = false;
    DWORD effect = DROPEFFECT_NONE;
    SHDoDragDrop(source, data.Get(), nullptr, DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, &effect);
    drag.active = false;
    drag.items.clear();
    return effect;
}

ComPtr<IDropTarget> FolderDropTarget(HWND owner, PCIDLIST_ABSOLUTE folder) {
    ComPtr<IDropTarget> target;
    if (!folder) return target;
    if (ILIsEmpty(folder)) {
        ComPtr<IShellFolder> desktop;
        if (SUCCEEDED(SHGetDesktopFolder(&desktop))) desktop->CreateViewObject(owner, IID_PPV_ARGS(&target));
        return target;
    }
    ComPtr<IShellFolder> parent;
    PCUITEMID_CHILD child = nullptr;
    if (SUCCEEDED(SHBindToParent(folder, IID_PPV_ARGS(&parent), &child)))
        parent->GetUIObjectOf(owner, 1, &child, IID_IDropTarget, nullptr, reinterpret_cast<void**>(target.GetAddressOf()));
    return target;
}

std::vector<std::wstring> DroppedFileNames(IDataObject* data) {
    std::vector<std::wstring> names;
    if (!data) return names;
    FORMATETC fmt{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg{};
    if (FAILED(data->GetData(&fmt, &stg))) return names;
    if (auto drop = static_cast<HDROP>(GlobalLock(stg.hGlobal))) {
        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i) {
            wchar_t path[MAX_PATH * 2];
            if (DragQueryFileW(drop, i, path, static_cast<UINT>(std::size(path)))) names.emplace_back(PathFindFileNameW(path));
        }
        GlobalUnlock(stg.hGlobal);
    }
    ReleaseStgMedium(&stg);
    return names;
}

bool PasteIntoFolder(HWND owner, PCIDLIST_ABSOLUTE folder) {
    ComPtr<IDataObject> data;
    if (FAILED(OleGetClipboard(&data)) || !data) return false;
    ComPtr<IDropTarget> target = FolderDropTarget(owner, folder);
    if (!target) return false;
    DWORD effect = DROPEFFECT_COPY;
    static CLIPFORMAT cfPreferred = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT));
    FORMATETC fmt{cfPreferred, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg{};
    if (SUCCEEDED(data->GetData(&fmt, &stg))) {
        if (auto p = static_cast<DWORD*>(GlobalLock(stg.hGlobal))) {
            effect = (*p & DROPEFFECT_MOVE) ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
            GlobalUnlock(stg.hGlobal);
        }
        ReleaseStgMedium(&stg);
    }
    POINTL pt{0, 0};
    DWORD allowed = effect;
    if (FAILED(target->DragEnter(data.Get(), MK_LBUTTON, pt, &allowed)) || allowed == DROPEFFECT_NONE) {
        target->DragLeave();
        return false;
    }
    allowed = effect;
    return SUCCEEDED(target->Drop(data.Get(), MK_LBUTTON, pt, &allowed));
}

// ---- DropTarget ----
DropTarget::DropTarget(HWND hwnd, DropHandler* handler) : m_hwnd(hwnd), m_handler(handler) {
    CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_helper));
}

IFACEMETHODIMP DropTarget::QueryInterface(REFIID riid, void** ppv) {
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
        *ppv = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) DropTarget::AddRef() { return ++m_ref; }

IFACEMETHODIMP_(ULONG) DropTarget::Release() {
    ULONG r = --m_ref;
    if (r == 0) delete this;
    return r;
}

IFACEMETHODIMP DropTarget::DragEnter(IDataObject* data, DWORD keys, POINTL pt, DWORD* effect) {
    POINT p{pt.x, pt.y};
    DWORD allowed = *effect;
    *effect = m_handler ? m_handler->DragEnter(data, keys, p, allowed) : DROPEFFECT_NONE;
    if (m_helper) m_helper->DragEnter(m_hwnd, data, &p, *effect);
    return S_OK;
}

IFACEMETHODIMP DropTarget::DragOver(DWORD keys, POINTL pt, DWORD* effect) {
    POINT p{pt.x, pt.y};
    DWORD allowed = *effect;
    *effect = m_handler ? m_handler->DragOver(keys, p, allowed) : DROPEFFECT_NONE;
    if (m_helper) m_helper->DragOver(&p, *effect);
    return S_OK;
}

IFACEMETHODIMP DropTarget::DragLeave() {
    if (m_handler) m_handler->DragLeave();
    if (m_helper) m_helper->DragLeave();
    return S_OK;
}

IFACEMETHODIMP DropTarget::Drop(IDataObject* data, DWORD keys, POINTL pt, DWORD* effect) {
    POINT p{pt.x, pt.y};
    DWORD allowed = *effect;
    if (m_helper) m_helper->Drop(data, &p, *effect);
    *effect = m_handler ? m_handler->Drop(data, keys, p, allowed) : DROPEFFECT_NONE;
    return S_OK;
}

void DropRegistration::Register(HWND hwnd, DropHandler* handler) {
    Revoke();
    m_target = new DropTarget(hwnd, handler);
    if (SUCCEEDED(RegisterDragDrop(hwnd, m_target))) {
        m_hwnd = hwnd;
    } else {
        m_target->Release();
        m_target = nullptr;
    }
}

void DropRegistration::Revoke() {
    if (m_target) {
        m_target->Detach();
        if (m_hwnd) RevokeDragDrop(m_hwnd);
        m_target->Release();
    }
    m_target = nullptr;
    m_hwnd = nullptr;
}

// ---- DropForwarder ----
DWORD DropForwarder::Enter(HWND owner, PCIDLIST_ABSOLUTE folder, IDataObject* data, DWORD keys, POINT pt, DWORD allowed) {
    if (m_target && m_folder && folder && ILIsEqual(m_folder.get(), folder)) return Over(keys, pt, allowed);
    Leave();
    m_target = FolderDropTarget(owner, folder);
    if (!m_target) return DROPEFFECT_NONE;
    m_folder = ClonePidl(folder);
    DWORD effect = allowed;
    if (FAILED(m_target->DragEnter(data, keys, POINTL{pt.x, pt.y}, &effect))) {
        m_target.Reset();
        return DROPEFFECT_NONE;
    }
    return effect;
}

DWORD DropForwarder::Over(DWORD keys, POINT pt, DWORD allowed) {
    if (!m_target) return DROPEFFECT_NONE;
    DWORD effect = allowed;
    m_target->DragOver(keys, POINTL{pt.x, pt.y}, &effect);
    return effect;
}

void DropForwarder::Leave() {
    if (m_target) m_target->DragLeave();
    m_target.Reset();
    m_folder.reset();
}

DWORD DropForwarder::Drop(IDataObject* data, DWORD keys, POINT pt, DWORD allowed) {
    if (!m_target) return DROPEFFECT_NONE;
    DWORD effect = allowed;
    m_target->Drop(data, keys, POINTL{pt.x, pt.y}, &effect);
    m_target.Reset();
    m_folder.reset();
    return effect;
}

}  // namespace of
