#include "desktop/DesktopHost.h"

#include "core/Log.h"

namespace of {

namespace {

struct DesktopWindows {
    HWND progman = nullptr;
    HWND defView = nullptr;
    HWND defViewParent = nullptr;
    HWND listView = nullptr;
};

DesktopWindows FindDesktopWindows() {
    DesktopWindows w;
    w.progman = FindWindowW(L"Progman", nullptr);
    w.defView = FindWindowExW(w.progman, nullptr, L"SHELLDLL_DefView", nullptr);
    w.defViewParent = w.progman;
    if (!w.defView) {
        // With wallpaper slideshows (and on some builds) DefView is hosted by a WorkerW instead.
        HWND worker = nullptr;
        while ((worker = FindWindowExW(nullptr, worker, L"WorkerW", nullptr)) != nullptr) {
            if (HWND dv = FindWindowExW(worker, nullptr, L"SHELLDLL_DefView", nullptr)) {
                w.defView = dv;
                w.defViewParent = worker;
                break;
            }
        }
    }
    if (w.defView) w.listView = FindWindowExW(w.defView, nullptr, L"SysListView32", nullptr);
    return w;
}

}  // namespace

bool DesktopHost::Refresh() {
    DesktopWindows w = FindDesktopWindows();
    m_progman = w.progman;
    m_defView = w.defView;
    m_defViewParent = w.defViewParent;
    m_listView = w.listView;
    log::Info(L"Desktop host: Progman={} DefView={} ListView={}", static_cast<void*>(m_progman), static_cast<void*>(m_defView),
              static_cast<void*>(m_listView));
    if (m_iconsHidden && m_listView) ShowWindow(m_listView, SW_HIDE);
    return m_progman != nullptr;
}

void DesktopHost::SetIconsHidden(bool hidden) {
    m_iconsHidden = hidden;
    if (!m_listView || !IsWindow(m_listView)) Refresh();
    if (m_listView) ShowWindow(m_listView, hidden ? SW_HIDE : SW_SHOW);
}

void DesktopHost::ForceShowDesktopIcons() {
    DesktopWindows w = FindDesktopWindows();
    if (w.listView) ShowWindow(w.listView, SW_SHOW);
}

HWND DesktopHost::HostParent() const {
    return m_defViewParent ? m_defViewParent : m_progman;
}

void DesktopHost::Attach(HWND hwnd) {
    if (m_mode == HostMode::ChildOfDesktop && HostParent()) {
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        style = (style & ~WS_POPUP) | WS_CHILD;
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);
        SetParent(hwnd, HostParent());
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    } else if (m_progman) {
        // Owned by the desktop: survives "Show desktop" and stays above the wallpaper.
        SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(m_progman));
    }
}

void DesktopHost::SetScreenRect(HWND hwnd, const RectI& r) {
    int x = r.left, y = r.top;
    if (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) {
        POINT pt{x, y};
        ScreenToClient(GetParent(hwnd), &pt);
        x = pt.x;
        y = pt.y;
    }
    SetWindowPos(hwnd, nullptr, x, y, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
}

void DesktopHost::RaiseTopmost(HWND hwnd) {
    RECT wr;
    GetWindowRect(hwnd, &wr);
    if (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) {
        SetParent(hwnd, nullptr);
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        SetWindowLongPtrW(hwnd, GWL_STYLE, (style & ~WS_CHILD) | WS_POPUP);
        SetWindowPos(hwnd, HWND_TOPMOST, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top, SWP_NOACTIVATE | SWP_FRAMECHANGED);
        return;
    }
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void DesktopHost::Lower(HWND hwnd) {
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (m_mode == HostMode::ChildOfDesktop && HostParent()) {
        RECT wr;
        GetWindowRect(hwnd, &wr);
        Attach(hwnd);
        SetScreenRect(hwnd, RectI{wr.left, wr.top, wr.right, wr.bottom});
    }
}

void DesktopHost::Restack(const std::vector<HWND>& topFirst) {
    for (HWND w : topFirst) {
        if (GetWindowLongPtrW(w, GWL_STYLE) & WS_CHILD) {
            // Children of the desktop host: keep above SHELLDLL_DefView, in reverse order.
            continue;
        }
        SetWindowPos(w, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
    for (auto it = topFirst.rbegin(); it != topFirst.rend(); ++it)
        if (GetWindowLongPtrW(*it, GWL_STYLE) & WS_CHILD) SetWindowPos(*it, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool DesktopHost::IsDesktopWindow(HWND hwnd) const {
    if (!hwnd) return false;
    if (hwnd == m_progman || hwnd == m_defView || hwnd == m_listView || hwnd == m_defViewParent) return true;
    wchar_t cls[64];
    if (GetClassNameW(hwnd, cls, 64) && (wcscmp(cls, L"WorkerW") == 0 || wcscmp(cls, L"Progman") == 0)) return true;
    return false;
}

}  // namespace of
