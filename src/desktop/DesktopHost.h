// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <windows.h>

#include <vector>

#include "core/Geometry.h"
#include "core/Model.h"

namespace of {

// Everything that touches Explorer's desktop window hierarchy (Progman / WorkerW / SHELLDLL_DefView /
// SysListView32) lives here, so undocumented behaviour stays in one place.
class DesktopHost {
public:
    // Re-discovers the desktop windows (call at start and after Explorer restarts).
    bool Refresh();
    HWND Progman() const { return m_progman; }
    HWND DefView() const { return m_defView; }
    HWND ListView() const { return m_listView; }

    // Hides or shows Explorer's own desktop icon view.
    void SetIconsHidden(bool hidden);
    bool IconsHidden() const { return m_iconsHidden; }

    void SetMode(HostMode mode) { m_mode = mode; }
    HostMode Mode() const { return m_mode; }

    // Turns a freshly created popup into a desktop-level window.
    void Attach(HWND hwnd);
    // Moves/resizes a desktop-level window given screen coordinates.
    void SetScreenRect(HWND hwnd, const RectI& screen);
    // Peek: lift above all windows / return to the desktop layer.
    void RaiseTopmost(HWND hwnd);
    void Lower(HWND hwnd);
    // Re-applies desktop-level z-order. `topFirst` lists our windows from top to bottom.
    void Restack(const std::vector<HWND>& topFirst);

    bool IsDesktopWindow(HWND hwnd) const;

    // Makes Explorer's desktop icons visible again (watchdog / crash path; no state needed).
    static void ForceShowDesktopIcons();

private:
    HWND HostParent() const;

    HWND m_progman = nullptr;
    HWND m_defView = nullptr;
    HWND m_defViewParent = nullptr;
    HWND m_listView = nullptr;
    HostMode m_mode = HostMode::OwnedByDesktop;
    bool m_iconsHidden = false;
};

}  // namespace of
