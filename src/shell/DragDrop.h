// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <oleidl.h>
#include <shobjidl.h>

#include <atomic>
#include <string>
#include <vector>

#include "shell/IconCache.h"
#include "shell/ShellItem.h"

namespace of {

// Describes a drag that started inside openFences, so our own drop targets can treat it as a
// membership change instead of a file operation.
struct InternalDrag {
    bool active = false;
    bool desktopItems = false;          // the items are desktop items (keys valid)
    std::vector<std::wstring> keys;     // desktop item keys
    std::vector<ShellItemPtr> items;
    std::string sourceTabId;            // "" = loose desktop icon
    bool dropHandledInternally = false; // set by a target that consumed the drop
};
InternalDrag& CurrentDrag();

// Runs a modal OLE drag of shell items. Returns the final drop effect.
DWORD StartShellDrag(HWND source, const std::vector<ShellItemPtr>& items, const IconPixels* image, InternalDrag info);

// Drop target that performs the shell's native move/copy/link into a folder.
ComPtr<IDropTarget> FolderDropTarget(HWND owner, PCIDLIST_ABSOLUTE folder);
// File names (no directory) carried by a data object (CF_HDROP).
std::vector<std::wstring> DroppedFileNames(IDataObject* data);
// Pastes the clipboard into a folder through the folder's drop target.
bool PasteIntoFolder(HWND owner, PCIDLIST_ABSOLUTE folder);

// Callback interface implemented by windows that accept drops.
class DropHandler {
public:
    virtual ~DropHandler() = default;
    virtual DWORD DragEnter(IDataObject* data, DWORD keys, POINT pt, DWORD allowed) = 0;
    virtual DWORD DragOver(DWORD keys, POINT pt, DWORD allowed) = 0;
    virtual void DragLeave() = 0;
    virtual DWORD Drop(IDataObject* data, DWORD keys, POINT pt, DWORD allowed) = 0;
};

// COM IDropTarget that forwards to a DropHandler and draws shell drag images.
class DropTarget final : public IDropTarget {
public:
    DropTarget(HWND hwnd, DropHandler* handler);
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    // IDropTarget
    IFACEMETHODIMP DragEnter(IDataObject* data, DWORD keys, POINTL pt, DWORD* effect) override;
    IFACEMETHODIMP DragOver(DWORD keys, POINTL pt, DWORD* effect) override;
    IFACEMETHODIMP DragLeave() override;
    IFACEMETHODIMP Drop(IDataObject* data, DWORD keys, POINTL pt, DWORD* effect) override;

    void Detach() { m_handler = nullptr; }

private:
    std::atomic<ULONG> m_ref{1};
    HWND m_hwnd;
    DropHandler* m_handler;
    ComPtr<IDropTargetHelper> m_helper;
};

// Registers/revokes a DropTarget for a window.
class DropRegistration {
public:
    ~DropRegistration() { Revoke(); }
    void Register(HWND hwnd, DropHandler* handler);
    void Revoke();

private:
    HWND m_hwnd = nullptr;
    DropTarget* m_target = nullptr;
};

// Forwards a drag to the native drop target of one folder at a time.
class DropForwarder {
public:
    // Switches to `folder` (re-entering if it changed). Returns the effect the folder accepts.
    DWORD Enter(HWND owner, PCIDLIST_ABSOLUTE folder, IDataObject* data, DWORD keys, POINT pt, DWORD allowed);
    DWORD Over(DWORD keys, POINT pt, DWORD allowed);
    void Leave();
    DWORD Drop(IDataObject* data, DWORD keys, POINT pt, DWORD allowed);
    bool Active() const { return m_target != nullptr; }

private:
    ComPtr<IDropTarget> m_target;
    SharedPidl m_folder;
};

}  // namespace of
