// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "ui/DesktopCanvas.h"

#include <windowsx.h>

#include <algorithm>

#include "shell/ContextMenu.h"
#include "shell/Wallpaper.h"
#include "ui/InlineEditor.h"

namespace of {

namespace {
constexpr wchar_t kClassName[] = L"openFences.Desktop";
Controller& C() { return Controller::Get(); }
}  // namespace

DesktopCanvas::DesktopCanvas(MonitorDesc monitor) : m_monitor(std::move(monitor)) {}

DesktopCanvas::~DesktopCanvas() { m_drop.Revoke(); }

bool DesktopCanvas::Create() {
    if (!CreateDesktopWindow(kClassName, L"openFences desktop", m_monitor.work)) return false;
    C().Host().Attach(m_hwnd);
    m_drop.Register(m_hwnd, this);
    RefreshStyle();
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    return true;
}

void DesktopCanvas::SetMonitor(const MonitorDesc& monitor) {
    m_monitor = monitor;
    C().Host().SetScreenRect(m_hwnd, m_monitor.work);
    m_wallpaper.Reset();
    RefreshStyle();
}

void DesktopCanvas::RefreshStyle() {
    ItemViewStyle style = C().StyleFor(nullptr, nullptr, Scale());
    style.freeLayout = true;
    m_view.SetStyle(style);
    m_view.SetViewport(ClientRect());
    Invalidate();
}

void DesktopCanvas::OnDpiChanged() { RefreshStyle(); }

void DesktopCanvas::SetItems(std::vector<ShellItemPtr> items, std::vector<PointI> cells) {
    m_view.SetFreeCells(std::move(cells));
    m_view.SetItems(std::move(items));
    Invalidate();
}

PointI DesktopCanvas::CellAtScreen(POINT screen) const {
    POINT p = screen;
    ScreenToClient(m_hwnd, &p);
    return m_view.CellAt({p.x, p.y});
}

void DesktopCanvas::SelectKeys(const std::vector<std::wstring>& keys) {
    m_view.ClearSelection();
    for (const auto& k : keys)
        if (int i = m_view.IndexOfKey(k); i >= 0) m_view.Toggle(i);
    Invalidate();
}

void DesktopCanvas::SetPeek(PeekKind kind) {
    m_peek = kind;
    if (kind == PeekKind::DesktopDisplay && (!m_wallpaper || m_wallpaperGen != Gfx().Generation())) {
        WallpaperInfo info = GetWallpaperFor(m_monitor.bounds);
        m_wallpaperPos = info.position;
        m_wallpaperBg = info.background;
        m_wallpaper = LoadWallpaperBitmap(info.path, static_cast<UINT>(m_monitor.bounds.Width()), static_cast<UINT>(m_monitor.bounds.Height()));
        m_wallpaperGen = Gfx().Generation();
    }
    Invalidate();
}

void DesktopCanvas::SetItemsShown(bool shown, bool animate) {
    m_itemTarget = shown ? 1.f : 0.f;
    if (!animate) {
        m_itemOpacity = m_itemTarget;
        Invalidate();
        return;
    }
    StartAnimation();
}

void DesktopCanvas::SetPageIndicator(bool visible, float swipeFraction) {
    m_pageIndicator = visible;
    m_swipeFraction = swipeFraction;
    Invalidate();
}

bool DesktopCanvas::Tick() {
    float step = 0.12f;
    if (m_itemOpacity < m_itemTarget) m_itemOpacity = std::min(m_itemTarget, m_itemOpacity + step);
    else if (m_itemOpacity > m_itemTarget) m_itemOpacity = std::max(m_itemTarget, m_itemOpacity - step);
    return m_itemOpacity != m_itemTarget;
}

void DesktopCanvas::Paint(ID2D1DeviceContext* dc) {
    Painter p(dc);
    RectI c = ClientRect();
    if (m_peek == PeekKind::DesktopDisplay) {
        // Opaque wallpaper: an instant "show desktop" that never touches other windows.
        RectI dest = m_monitor.bounds.Offset(-m_monitor.work.left, -m_monitor.work.top);
        DrawWallpaper(p, m_wallpaper.Get(), m_wallpaperPos, dest, m_wallpaperBg);
    } else if (m_peek == PeekKind::Peek) {
        p.FillRect(c, Color{0, 0, 0, static_cast<uint8_t>(C().Cfg().peek.dimPercent * 255 / 100)});
    } else {
        // Nearly transparent fill so the whole surface stays hit-testable on every compositor path.
        p.FillRect(c, Color{0, 0, 0, 1});
    }

    bool hidden = C().Hidden() && m_peek == PeekKind::None;
    if (C().Cfg().general.showLooseIcons) {
        float o = m_peek != PeekKind::None ? 1.f : m_itemOpacity;
        if (hidden && o <= 0.f) m_view.Paint(p, C().Icons(), 1.f, true, &C().Cfg().hide.excludeItems);
        else m_view.Paint(p, C().Icons(), std::max(o, 0.f));
    }

    if (m_mode == Mode::Band && !m_band.Empty()) {
        p.FillRect(m_band, Color{120, 170, 255, 50});
        p.StrokeRoundRect(m_band, 0, Color{140, 190, 255, 200}, 1.f);
    }

    if (m_pageIndicator) {
        int count = C().PageCount();
        int current = C().CurrentLayout().currentPage;
        int dot = Px(8), gap = Px(10);
        int total = count * dot + (count - 1) * gap;
        int x = c.Center().x - total / 2;
        int y = c.bottom - Px(40);
        p.FillRoundRect({x - Px(12), y - Px(10), x + total + Px(12), y + dot + Px(10)}, static_cast<float>(Px(14)), Color{0, 0, 0, 110});
        for (int i = 0; i < count; ++i) {
            RectI d = RectI::FromXYWH(x + i * (dot + gap), y, dot, dot);
            p.FillRoundRect(d, dot / 2.f, Color{255, 255, 255, static_cast<uint8_t>(i == current ? 240 : 90)});
        }
        TextStyle ts;
        ts.sizePx = 13 * Scale();
        ts.h = HAlign::Center;
        ts.shadow = true;
        const auto& pages = C().CurrentLayout().pages;
        if (current >= 0 && current < static_cast<int>(pages.size()))
            p.Text(pages[static_cast<size_t>(current)].name, {c.left, y - Px(36), c.right, y - Px(14)}, Color{255, 255, 255, 255}, ts);
        if (m_swipeFraction != 0.f) {
            int edge = m_swipeFraction > 0 ? c.left : c.right;
            int w = static_cast<int>(std::abs(m_swipeFraction) * c.Width() * 0.5f);
            RectI glow = m_swipeFraction > 0 ? RectI{edge, c.top, edge + w, c.bottom} : RectI{edge - w, c.top, edge, c.bottom};
            p.FillRect(glow, Color{120, 170, 255, 40});
        }
    }
}

LRESULT DesktopCanvas::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_LBUTTONDOWN:
            OnLButtonDown({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}, wp);
            return 0;
        case WM_MOUSEMOVE:
            OnMouseMove({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}, wp);
            return 0;
        case WM_LBUTTONUP:
            OnLButtonUp({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}, wp);
            return 0;
        case WM_LBUTTONDBLCLK:
            OnDoubleClick({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
            return 0;
        case WM_RBUTTONUP:
            OnRightClick({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
            return 0;
        case WM_MOUSELEAVE:
            m_view.hover = -1;
            Invalidate();
            return 0;
        case WM_KEYDOWN:
            OnKey(wp);
            return 0;
        case WM_CAPTURECHANGED:
            if (reinterpret_cast<HWND>(lp) != m_hwnd && m_mode != Mode::None) {
                if (m_mode == Mode::Swipe) C().PageSwipeEnd(0, 1);
                m_mode = Mode::None;
                m_band = {};
                Invalidate();
            }
            break;
    }
    return WindowBase::HandleMessage(msg, wp, lp);
}

int DesktopCanvas::VisibleItemAt(PointI pt) const {
    int idx = m_view.HitTest(pt);
    if (idx < 0) return -1;
    const Config& cfg = C().Cfg();
    // Hidden() is false while peeking, so every icon is clickable then.
    bool visible = LooseItemVisible(cfg.general.showLooseIcons, C().Hidden(), m_view.Items()[static_cast<size_t>(idx)]->key, cfg.hide.excludeItems);
    return visible ? idx : -1;
}

void DesktopCanvas::OnLButtonDown(PointI pt, WPARAM keys) {
    inline_editor::Cancel();
    SetFocus(m_hwnd);
    m_pressScreen = {pt.x, pt.y};
    ClientToScreen(m_hwnd, &m_pressScreen);
    m_pressClient = pt;
    SetCapture(m_hwnd);
    int idx = VisibleItemAt(pt);
    const Config& cfg = C().Cfg();
    int edge = Px(6);
    RectI c = ClientRect();
    if (idx < 0 && cfg.pages.enabled && cfg.pages.edgeSwipe && C().Peeking() == PeekKind::None &&
        (pt.x <= c.left + edge || pt.x >= c.right - edge)) {
        m_mode = Mode::Swipe;
        return;
    }
    if (idx >= 0) {
        m_pressIndex = idx;
        m_pressWasSelected = m_view.IsSelected(idx);
        if (keys & MK_CONTROL) m_view.Toggle(idx);
        else if (keys & MK_SHIFT) m_view.SelectRange(m_view.anchor, idx);
        else if (!m_pressWasSelected) m_view.SelectOnly(idx);
        m_mode = Mode::ItemPress;
    } else {
        if (!(keys & MK_CONTROL)) m_view.ClearSelection();
        m_mode = Mode::Band;
        m_band = {};
    }
    Invalidate();
}

void DesktopCanvas::OnMouseMove(PointI pt, WPARAM keys) {
    TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, m_hwnd, 0};
    TrackMouseEvent(&tme);
    POINT screen{pt.x, pt.y};
    ClientToScreen(m_hwnd, &screen);
    switch (m_mode) {
        case Mode::None: {
            int hover = VisibleItemAt(pt);
            if (hover != m_view.hover) {
                m_view.hover = hover;
                Invalidate();
            }
            C().ExecuteCommand(L"desktop-activity", L"");
            return;
        }
        case Mode::ItemPress:
            if (std::abs(screen.x - m_pressScreen.x) > GetSystemMetrics(SM_CXDRAG) || std::abs(screen.y - m_pressScreen.y) > GetSystemMetrics(SM_CYDRAG))
                StartItemDrag();
            return;
        case Mode::Band:
            m_band = Normalized(m_pressClient, pt);
            m_view.SelectInRect(m_band, (keys & MK_CONTROL) != 0);
            Invalidate();
            return;
        case Mode::Swipe:
            C().PageSwipeUpdate(screen.x - m_pressScreen.x, ClientRect().Width());
            return;
    }
}

void DesktopCanvas::OnLButtonUp(PointI pt, WPARAM keys) {
    Mode mode = m_mode;
    m_mode = Mode::None;
    if (GetCapture() == m_hwnd) ReleaseCapture();
    POINT screen{pt.x, pt.y};
    ClientToScreen(m_hwnd, &screen);
    switch (mode) {
        case Mode::ItemPress:
            if (m_pressWasSelected && !(keys & (MK_CONTROL | MK_SHIFT))) m_view.SelectOnly(m_pressIndex);
            Invalidate();
            return;
        case Mode::Swipe:
            C().PageSwipeEnd(screen.x - m_pressScreen.x, ClientRect().Width());
            return;
        case Mode::Band: {
            RectI band = m_band;
            m_band = {};
            Invalidate();
            if (C().Peeking() != PeekKind::None && band.Width() < Px(4) && band.Height() < Px(4)) {
                C().EndPeek();
                return;
            }
            if (band.Width() >= Px(100) && band.Height() >= Px(70) && !C().IsLayoutLocked(nullptr)) {
                std::vector<std::wstring> keys2;
                for (const auto& item : m_view.SelectedItems()) keys2.push_back(item->key);
                RectI s = ScreenRect();
                C().ShowCreateMenu(this, band.Offset(s.left, s.top), keys2, screen);
            }
            return;
        }
        default:
            return;
    }
}

void DesktopCanvas::OnDoubleClick(PointI pt) {
    // Icons kept visible during Quick-Hide open normally; only empty desktop toggles Quick-Hide.
    int idx = VisibleItemAt(pt);
    if (idx >= 0) {
        OpenItem(m_hwnd, *m_view.Items()[static_cast<size_t>(idx)]);
        C().OnItemOpened();
        return;
    }
    if (C().Peeking() != PeekKind::None) {
        C().EndPeek();
        return;
    }
    if (C().Cfg().hide.enabled) C().ToggleHide();
}

void DesktopCanvas::OnRightClick(PointI pt) {
    POINT screen{pt.x, pt.y};
    ClientToScreen(m_hwnd, &screen);
    int idx = VisibleItemAt(pt);
    if (idx >= 0) {
        if (!m_view.IsSelected(idx)) m_view.SelectOnly(idx);
        Invalidate();
        RectI s = ScreenRect();
        C().ShowItemMenu(m_hwnd, m_view.SelectedItems(), screen, {}, true, m_view.LabelRect(idx).Offset(s.left, s.top), m_view.Style().fontPx);
        return;
    }
    C().ShowDesktopMenu(this, screen);
}

void DesktopCanvas::OnKey(WPARAM vk) {
    if (vk == VK_ESCAPE && C().Peeking() != PeekKind::None) {
        C().EndPeek();
        return;
    }
    bool ctrl = GetKeyState(VK_CONTROL) < 0;
    bool shift = GetKeyState(VK_SHIFT) < 0;
    auto sel = m_view.SelectedItems();
    switch (vk) {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN: {
            int dx = vk == VK_LEFT ? -1 : vk == VK_RIGHT ? 1 : 0;
            int dy = vk == VK_UP ? -1 : vk == VK_DOWN ? 1 : 0;
            int next = m_view.Neighbor(m_view.focus, dx, dy);
            if (next >= 0) {
                if (shift) m_view.SelectRange(m_view.anchor, next);
                else m_view.SelectOnly(next);
                Invalidate();
            }
            return;
        }
        case VK_RETURN:
            for (const auto& item : sel) OpenItem(m_hwnd, *item);
            if (!sel.empty()) C().OnItemOpened();
            return;
        case VK_DELETE:
            if (!sel.empty()) InvokeVerb(m_hwnd, sel, "delete");
            return;
        case VK_F2:
            if (!sel.empty()) {
                int i = m_view.SelectedIndices()[0];
                RectI s = ScreenRect();
                C().RenameItemInteractive(m_hwnd, sel[0], m_view.LabelRect(i).Offset(s.left, s.top), m_view.Style().fontPx);
            }
            return;
        case VK_F5:
            C().ExecuteCommand(L"refresh", L"");
            return;
        case VK_ESCAPE:
            m_view.ClearSelection();
            Invalidate();
            return;
    }
    if (ctrl) {
        if (vk == 'A') {
            m_view.SelectAll();
            Invalidate();
        } else if (vk == 'C' && !sel.empty()) {
            InvokeVerb(m_hwnd, sel, "copy");
        } else if (vk == 'X' && !sel.empty()) {
            InvokeVerb(m_hwnd, sel, "cut");
        } else if (vk == 'V') {
            if (auto desk = ParsePath(C().DesktopFolderPath())) PasteIntoFolder(m_hwnd, desk.get());
        }
    }
}

void DesktopCanvas::StartItemDrag() {
    m_mode = Mode::None;
    if (GetCapture() == m_hwnd) ReleaseCapture();
    auto items = m_view.SelectedItems();
    if (items.empty()) return;
    InternalDrag info;
    info.desktopItems = true;
    for (const auto& i : items) info.keys.push_back(i->key);
    auto px = C().Icons().Pixels(*items[0], m_view.Style().iconPx);
    StartShellDrag(m_hwnd, items, px.get(), std::move(info));
    Invalidate();
}

DWORD DesktopCanvas::ComputeDrop(DWORD keys, POINT screen, DWORD allowed) {
    m_dropCell = CellAtScreen(screen);
    InternalDrag& drag = CurrentDrag();
    if (drag.active && drag.desktopItems) {
        m_forward.Leave();
        m_dropMode = DropMode::Internal;
        if (!drag.sourceTabId.empty()) {
            auto [g, t] = FindTab(C().CurrentLayout(), drag.sourceTabId);
            if (C().IsContentLocked(g)) {
                m_dropMode = DropMode::None;
                return DROPEFFECT_NONE;
            }
        }
        return DROPEFFECT_MOVE;
    }
    auto desk = ParsePath(C().DesktopFolderPath());
    if (!desk) return DROPEFFECT_NONE;
    m_dropMode = DropMode::ForwardDesktop;
    return m_forward.Enter(m_hwnd, desk.get(), m_dragData.Get(), keys, screen, allowed);
}

DWORD DesktopCanvas::DragEnter(IDataObject* data, DWORD keys, POINT pt, DWORD allowed) {
    m_dragData = data;
    return ComputeDrop(keys, pt, allowed);
}

DWORD DesktopCanvas::DragOver(DWORD keys, POINT pt, DWORD allowed) { return ComputeDrop(keys, pt, allowed); }

void DesktopCanvas::DragLeave() {
    m_forward.Leave();
    m_dragData.Reset();
    m_dropMode = DropMode::None;
}

DWORD DesktopCanvas::Drop(IDataObject* data, DWORD keys, POINT pt, DWORD allowed) {
    m_dragData = data;
    ComputeDrop(keys, pt, allowed);
    DWORD effect = DROPEFFECT_NONE;
    if (m_dropMode == DropMode::Internal) {
        CurrentDrag().dropHandledInternally = true;
        C().MakeLoose(CurrentDrag().keys, m_monitor.id, m_dropCell);
    } else if (m_dropMode == DropMode::ForwardDesktop) {
        C().ExpectDroppedFiles(DroppedFileNames(data), {}, m_monitor.id, m_dropCell);
        effect = m_forward.Drop(data, keys, pt, allowed);
    }
    m_dropMode = DropMode::None;
    m_dragData.Reset();
    return effect;
}

}  // namespace of
