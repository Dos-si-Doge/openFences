#include "ui/WindowBase.h"

#include "shell/ContextMenu.h"

namespace of {

WindowBase::~WindowBase() {
    if (m_hwnd) {
        SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, 0);
        DestroyWindow(m_hwnd);
    }
}

bool WindowBase::CreateDesktopWindow(const wchar_t* className, const wchar_t* title, const RectI& screen, DWORD exStyle) {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    if (!GetClassInfoExW(inst, className, &wc)) {
        wc = WNDCLASSEXW{sizeof(wc)};
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = StaticProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = className;
        RegisterClassExW(&wc);
    }
    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP | exStyle, className, title, WS_POPUP | WS_CLIPCHILDREN, screen.left,
                             screen.top, screen.Width(), screen.Height(), nullptr, nullptr, inst, this);
    if (!m_hwnd) return false;
    m_dpi = GetDpiForWindow(m_hwnd);
    m_surface.Attach(m_hwnd);
    return true;
}

LRESULT CALLBACK WindowBase::StaticProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        auto* self = static_cast<WindowBase*>(cs->lpCreateParams);
        self->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<WindowBase*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    LRESULT menuResult = 0;
    if (HandleShellMenuMessage(hwnd, msg, wp, lp, &menuResult)) return menuResult;
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    return self->HandleMessage(msg, wp, lp);
}

LRESULT WindowBase::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            ValidateRect(m_hwnd, nullptr);
            DoPaint();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            Invalidate();
            break;
        case WM_WINDOWPOSCHANGING:
            if (!m_allowZ) reinterpret_cast<WINDOWPOS*>(lp)->flags |= SWP_NOZORDER;
            break;
        case WM_DPICHANGED:
            m_dpi = HIWORD(wp);
            OnDpiChanged();
            Invalidate();
            return 0;
        case WM_TIMER:
            if (wp == kAnimTimer) {
                if (!Tick()) {
                    KillTimer(m_hwnd, kAnimTimer);
                    m_animating = false;
                }
                Invalidate();
                return 0;
            }
            break;
        case WM_MOUSEACTIVATE:
            return MA_ACTIVATE;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

void WindowBase::DoPaint() {
    for (int attempt = 0; attempt < 2; ++attempt) {
        ID2D1DeviceContext* dc = m_surface.BeginDraw();
        if (!dc) return;
        Paint(dc);
        if (m_surface.EndDraw()) return;
        Gfx().Recreate();
    }
}

RectI WindowBase::ClientRect() const {
    RECT r{};
    GetClientRect(m_hwnd, &r);
    return {r.left, r.top, r.right, r.bottom};
}

RectI WindowBase::ScreenRect() const {
    RECT r{};
    GetWindowRect(m_hwnd, &r);
    return {r.left, r.top, r.right, r.bottom};
}

void WindowBase::Invalidate() {
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void WindowBase::StartAnimation() {
    if (m_animating) return;
    m_animating = true;
    SetTimer(m_hwnd, kAnimTimer, 15, nullptr);
}

}  // namespace of
