#include "ui/FenceWindow.h"

#include <windowsx.h>

#include <algorithm>

#include "app/Controller.h"
#include "core/Strings.h"
#include "shell/ContextMenu.h"
#include "ui/InlineEditor.h"

namespace of {

namespace {
constexpr UINT_PTR kTimerCollapse = 0xB001;
constexpr UINT_PTR kTimerTabHover = 0xB002;
constexpr wchar_t kClassName[] = L"openFences.Fence";

Controller& C() { return Controller::Get(); }

Color Lighten(Color c, int amount) {
    auto l = [&](uint8_t v) { return static_cast<uint8_t>(std::clamp(v + amount, 0, 255)); };
    return {l(c.r), l(c.g), l(c.b), c.a};
}
}  // namespace

FenceWindow::FenceWindow(std::string groupId) : m_groupId(std::move(groupId)) {}

FenceWindow::~FenceWindow() { m_drop.Revoke(); }

bool FenceWindow::Create() {
    Group* g = G();
    if (!g) return false;
    if (!CreateDesktopWindow(kClassName, g->tabs.empty() ? L"Fence" : g->tabs[0].title.c_str(), C().GroupScreenRect(*g))) return false;
    C().Host().Attach(m_hwnd);
    m_drop.Register(m_hwnd, this);
    SyncFromModel();
    return true;
}

Group* FenceWindow::G() const { return FindGroup(C().CurrentLayout(), m_groupId); }

Tab* FenceWindow::ActiveTab() const {
    Group* g = G();
    return g ? g->ActiveTab() : nullptr;
}

bool FenceWindow::IsPortal() const {
    Tab* t = ActiveTab();
    return t && t->kind == TabKind::Portal;
}

int FenceWindow::TitleH() const { return Px(C().Cfg().appearance.titleFontSize * 2.3f); }
RectI FenceWindow::TitleRect() const { return {0, 0, ClientRect().right, TitleH()}; }
RectI FenceWindow::MenuButtonRect() const { return {0, 0, TitleH(), TitleH()}; }
RectI FenceWindow::CloseButtonRect() const {
    int w = ClientRect().right;
    return {w - TitleH(), 0, w, TitleH()};
}

RectI FenceWindow::BreadcrumbRect() const {
    PortalModel* pm = IsPortal() ? C().Portal(ActiveTab()->id) : nullptr;
    if (!pm || pm->stack.size() <= 1) return {0, TitleH(), ClientRect().right, TitleH()};
    return {0, TitleH(), ClientRect().right, TitleH() + Px(24)};
}

RectI FenceWindow::BodyRect() const {
    RectI c = ClientRect();
    return {c.left, BreadcrumbRect().bottom, c.right, c.bottom};
}

bool FenceWindow::ShowTabs() const {
    Group* g = G();
    return g && g->tabs.size() > 1 && C().Cfg().appearance.tabStyle != TabStyle::Hidden;
}

std::vector<FenceWindow::TabRect> FenceWindow::TabRects() const {
    std::vector<TabRect> out;
    Group* g = G();
    if (!g || !ShowTabs()) return out;
    RectI area{MenuButtonRect().right, Px(3), CloseButtonRect().left, TitleH() - Px(3)};
    int n = static_cast<int>(g->tabs.size());
    TextStyle ts{C().Cfg().appearance.titleFontSize * Scale()};
    std::vector<int> widths;
    int total = 0;
    for (const Tab& t : g->tabs) {
        int w = std::clamp(static_cast<int>(Painter::MeasureText(t.title, ts)) + Px(22), Px(44), Px(220));
        widths.push_back(w);
        total += w;
    }
    int gap = Px(2);
    int avail = area.Width() - gap * (n - 1);
    if (total > avail && total > 0) {
        for (int& w : widths) w = std::max(Px(28), w * avail / total);
    }
    total = 0;
    for (int w : widths) total += w;
    int x = area.left + std::max(0, (area.Width() - total - gap * (n - 1)) / 2);
    for (int i = 0; i < n; ++i) {
        out.push_back({i, {x, area.top, x + widths[i], area.bottom}});
        x += widths[i] + gap;
    }
    return out;
}

int FenceWindow::TabAt(PointI p) const {
    for (const auto& t : TabRects())
        if (t.rect.Contains(p)) return t.index;
    return -1;
}

unsigned FenceWindow::BorderHit(PointI p) const {
    Group* g = G();
    if (!g || C().IsLayoutLocked(g) || Collapsed()) return EdgeNone;
    RectI c = ClientRect();
    int b = Px(6);
    unsigned e = EdgeNone;
    if (p.x < b) e |= EdgeLeft;
    if (p.x >= c.right - b) e |= EdgeRight;
    if (p.y >= c.bottom - b) e |= EdgeBottom;
    if (p.y < Px(3)) e |= EdgeTop;
    return e;
}

bool FenceWindow::AutoRollup() const {
    Group* g = G();
    if (!g || !C().Cfg().rollup.autoRollupAtEdges) return false;
    const MonitorDesc* m = C().MonitorFor(*g);
    if (!m) return false;
    RectI r = DenormalizeRect(g->rect, m->work);
    return (EdgesTouched(r, m->work, Px(2)) & (EdgeTop | EdgeBottom)) != 0;
}

bool FenceWindow::Collapsed() const {
    Group* g = G();
    if (!g || m_expanded) return false;
    if (C().Peeking() != PeekKind::None) return false;
    return g->rolledUp || AutoRollup();
}

void FenceWindow::SetExpanded(bool expanded) {
    if (m_expanded == expanded) return;
    m_expanded = expanded;
    ApplyGeometry(m_offsetX);
    Invalidate();
}

void FenceWindow::ApplyGeometry(int offsetX) {
    m_offsetX = offsetX;
    Group* g = G();
    if (!g) return;
    RectI r = C().GroupScreenRect(*g);
    if (Collapsed()) {
        const MonitorDesc* m = C().MonitorFor(*g);
        bool atBottom = m && (EdgesTouched(r, m->work, Px(2)) & EdgeBottom) && !(EdgesTouched(r, m->work, Px(2)) & EdgeTop);
        if (atBottom) r.top = r.bottom - TitleH();
        else r.bottom = r.top + TitleH();
    }
    C().Host().SetScreenRect(m_hwnd, r.Offset(offsetX, 0));
    UpdateViewport();
}

void FenceWindow::UpdateViewport() {
    m_view.SetViewport(BodyRect().Inflate(-Px(2)));
}

void FenceWindow::SyncFromModel() {
    Group* g = G();
    if (!g) return;
    Tab* t = g->ActiveTab();
    m_view.SetStyle(C().StyleFor(g, t, Scale()));
    m_view.SetItems(t ? C().TabItems(*t) : std::vector<ShellItemPtr>{});
    ApplyGeometry(m_offsetX);
    Invalidate();
}

void FenceWindow::OnDpiChanged() { SyncFromModel(); }

void FenceWindow::SetShown(bool shown, bool animate) {
    m_targetOpacity = shown ? 1.f : 0.f;
    if (shown && !IsWindowVisible(m_hwnd)) {
        if (!animate) m_opacity = 1.f;
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    }
    if (!animate) {
        m_opacity = m_targetOpacity;
        if (!shown) ShowWindow(m_hwnd, SW_HIDE);
        Invalidate();
        return;
    }
    StartAnimation();
}

bool FenceWindow::Tick() {
    float step = 0.12f;
    if (m_opacity < m_targetOpacity) m_opacity = std::min(m_targetOpacity, m_opacity + step);
    else if (m_opacity > m_targetOpacity) m_opacity = std::max(m_targetOpacity, m_opacity - step);
    if (m_opacity <= 0.f && m_targetOpacity <= 0.f) {
        ShowWindow(m_hwnd, SW_HIDE);
        return false;
    }
    return m_opacity != m_targetOpacity;
}

bool FenceWindow::TitleContains(POINT screen) const {
    if (!IsWindowVisible(m_hwnd)) return false;
    RectI s = ScreenRect();
    RectI t = TitleRect().Offset(s.left, s.top);
    return t.Contains({screen.x, screen.y});
}

void FenceWindow::SetMergeHighlight(bool on) {
    if (m_mergeHighlight != on) {
        m_mergeHighlight = on;
        Invalidate();
    }
}

void FenceWindow::SelectKeys(const std::vector<std::wstring>& keys) {
    m_view.ClearSelection();
    for (const auto& k : keys) {
        int i = m_view.IndexOfKey(k);
        if (i >= 0) m_view.Toggle(i);
    }
    Invalidate();
}

void FenceWindow::BeginTitleRename() {
    Group* g = G();
    Tab* t = ActiveTab();
    if (!g || !t || C().IsLayoutLocked(g)) return;
    RectI s = ScreenRect();
    RectI r{s.left + MenuButtonRect().right, s.top + Px(3), s.right - TitleH(), s.top + TitleH() - Px(3)};
    std::string tabId = t->id;
    inline_editor::Show(m_hwnd, r, t->title, C().Cfg().appearance.titleFontSize * Scale(), false, [tabId](const std::wstring& text) {
        auto [grp, tab] = FindTab(Controller::Get().CurrentLayout(), tabId);
        std::wstring name = Trim(text);
        if (!tab || name.empty()) return;
        tab->title = name;
        Controller::Get().SaveSoon();
        Controller::Get().InvalidateAll();
    });
}

// ---------------------------------------------------------------- painting

void FenceWindow::Paint(ID2D1DeviceContext* dc) {
    Group* g = G();
    if (!g) return;
    Painter p(dc);
    const AppearanceSettings& ap = C().Cfg().appearance;
    float o = m_opacity;
    RectI c = ClientRect();
    float radius = ap.cornerRadius * Scale();
    Color bg = C().BackgroundFor(*g);
    p.FillRoundRect(c, radius, bg, o);

    RectI title = TitleRect();
    bool collapsed = Collapsed();
    if (ap.darkenTitle) {
        if (collapsed) p.FillRoundRect(c, radius, Color{0, 0, 0, 60}, o);
        else p.FillTopRoundRect(title, radius, Color{0, 0, 0, 60}, o);
    }

    // Menu (hamburger) glyph.
    RectI mb = MenuButtonRect();
    {
        int cx = mb.Center().x, cy = mb.Center().y, w = Px(6);
        Color gc{255, 255, 255, static_cast<uint8_t>(m_hoverWindow ? 220 : 120)};
        for (int k = -1; k <= 1; ++k) p.Line({cx - w, cy + k * Px(4)}, {cx + w, cy + k * Px(4)}, gc, 1.4f * Scale(), o);
    }
    if (m_hoverWindow && !C().IsLayoutLocked(g)) {
        RectI cb = CloseButtonRect();
        int cx = cb.Center().x, cy = cb.Center().y, w = Px(4);
        Color gc{255, 255, 255, 170};
        p.Line({cx - w, cy - w}, {cx + w, cy + w}, gc, 1.4f * Scale(), o);
        p.Line({cx - w, cy + w}, {cx + w, cy - w}, gc, 1.4f * Scale(), o);
    }

    TextStyle ts;
    ts.sizePx = ap.titleFontSize * Scale();
    ts.bold = true;
    ts.h = HAlign::Center;
    ts.shadow = true;
    Color white{255, 255, 255, 255};
    if (ShowTabs()) {
        auto rects = TabRects();
        for (const auto& tr : rects) {
            const Tab& t = g->tabs[static_cast<size_t>(tr.index)];
            bool active = tr.index == g->activeTab;
            Color tc = (ap.tabColors && t.color) ? *t.color : Color{255, 255, 255, 255};
            switch (ap.tabStyle) {
                case TabStyle::Segmented:
                    p.FillRect(tr.rect, active ? Color{tc.r, tc.g, tc.b, 90} : Color{tc.r, tc.g, tc.b, 30}, o);
                    break;
                case TabStyle::Rounded:
                    p.FillRoundRect(tr.rect, tr.rect.Height() / 2.f, active ? Color{tc.r, tc.g, tc.b, 100} : Color{tc.r, tc.g, tc.b, 28}, o);
                    break;
                default:
                    if (active) p.FillRect({tr.rect.left + Px(6), tr.rect.bottom - Px(2), tr.rect.right - Px(6), tr.rect.bottom}, Lighten(tc, 0), o);
                    else if (ap.tabColors && t.color) p.FillRect({tr.rect.left + Px(10), tr.rect.bottom - Px(1), tr.rect.right - Px(10), tr.rect.bottom}, tc, o * 0.6f);
                    break;
            }
            TextStyle tts = ts;
            tts.bold = active;
            p.Text(t.title, tr.rect.Inflate(-Px(4)), white, tts, active ? o : o * 0.7f);
        }
    } else if (const Tab* t = g->ActiveTab()) {
        RectI tr{mb.right, 0, CloseButtonRect().left, title.bottom};
        std::wstring text = t->title;
        if (C().IsLayoutLocked(g) && g->locked) text += L"  \U0001F512";
        p.Text(text, tr, white, ts, o);
    }

    if (collapsed) {
        if (m_mergeHighlight) p.StrokeRoundRect(c, radius, Color{120, 180, 255, 255}, 2 * Scale(), o);
        return;
    }

    // Portal breadcrumb.
    Tab* t = g->ActiveTab();
    PortalModel* pm = (t && t->kind == TabKind::Portal) ? C().Portal(t->id) : nullptr;
    RectI bc = BreadcrumbRect();
    if (!bc.Empty() && pm) {
        p.FillRect(bc, Color{0, 0, 0, 40}, o);
        TextStyle bs;
        bs.sizePx = ap.titleFontSize * 0.95f * Scale();
        bs.shadow = true;
        p.Text(L"‹  " + pm->folderName, bc.Inflate(-Px(6)), white, bs, o * 0.9f);
    }

    RectI body = BodyRect();
    TextStyle hint;
    hint.sizePx = ap.titleFontSize * Scale();
    hint.h = HAlign::Center;
    hint.maxLines = 3;
    hint.shadow = true;
    if (pm && pm->state == PortalModel::State::Offline) {
        p.Text(L"Folder unavailable\n" + pm->rootPath + L"\nRetrying…", body.Inflate(-Px(10)), white, hint, o * 0.75f);
    } else if (pm && pm->state == PortalModel::State::Loading && m_view.Count() == 0) {
        p.Text(L"Loading…", body, white, hint, o * 0.6f);
    } else if (m_view.Count() == 0) {
        p.Text(t && t->kind == TabKind::Portal ? L"This folder is empty" : L"Drag icons here", body.Inflate(-Px(10)), white, hint, o * 0.5f);
    }

    m_view.Paint(p, C().Icons(), o);

    if (m_mode == Mode::Band && !m_band.Empty()) {
        p.FillRect(m_band, Color{120, 170, 255, 50}, o);
        p.StrokeRoundRect(m_band, 0, Color{140, 190, 255, 200}, 1.f, o);
    }
    if (m_mergeHighlight || m_dropHighlight) p.StrokeRoundRect(c, radius, Color{120, 180, 255, 230}, 2 * Scale(), o);
}

// ---------------------------------------------------------------- mouse & keyboard

LRESULT FenceWindow::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MOUSEMOVE:
            OnMouseMove({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}, wp);
            return 0;
        case WM_LBUTTONDOWN:
            OnLButtonDown({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}, wp);
            return 0;
        case WM_LBUTTONUP:
            OnLButtonUp({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}, wp);
            return 0;
        case WM_LBUTTONDBLCLK:
            OnDoubleClick({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}, wp);
            return 0;
        case WM_RBUTTONUP:
            OnRightClick({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
            return 0;
        case WM_MOUSEWHEEL:
            OnWheel(GET_WHEEL_DELTA_WPARAM(wp), false);
            return 0;
        case WM_MOUSEHWHEEL:
            OnWheel(-GET_WHEEL_DELTA_WPARAM(wp), true);
            return 0;
        case WM_MOUSEHOVER:
            if (Collapsed() && C().Cfg().rollup.openMode == RollupOpen::HoverDelay) SetExpanded(true);
            return 0;
        case WM_MOUSELEAVE:
            m_tracking = false;
            m_hoverWindow = false;
            m_view.hover = -1;
            m_view.SetScrollbarHot(false);
            if (m_expanded) SetTimer(m_hwnd, kTimerCollapse, 400, nullptr);
            Invalidate();
            return 0;
        case WM_TIMER:
            if (wp == kTimerCollapse) {
                POINT pt;
                GetCursorPos(&pt);
                RectI s = ScreenRect();
                bool inside = s.Contains({pt.x, pt.y});
                if (!inside && GetCapture() != m_hwnd && m_dropMode == DropMode::None && !inline_editor::Active()) {
                    KillTimer(m_hwnd, kTimerCollapse);
                    SetExpanded(false);
                } else if (inside) {
                    KillTimer(m_hwnd, kTimerCollapse);
                }
                return 0;
            }
            if (wp == kTimerTabHover) {
                KillTimer(m_hwnd, kTimerTabHover);
                Group* g = G();
                if (g && m_hoverTab >= 0 && m_hoverTab < static_cast<int>(g->tabs.size()) && m_hoverTab != g->activeTab) {
                    g->activeTab = m_hoverTab;
                    SyncFromModel();
                }
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(m_hwnd, &pt);
                unsigned e = BorderHit({pt.x, pt.y});
                LPCWSTR cur = IDC_ARROW;
                if ((e & EdgeLeft && e & EdgeBottom) || (e & EdgeRight && e & EdgeTop)) cur = IDC_SIZENESW;
                else if ((e & EdgeRight && e & EdgeBottom) || (e & EdgeLeft && e & EdgeTop)) cur = IDC_SIZENWSE;
                else if (e & (EdgeLeft | EdgeRight)) cur = IDC_SIZEWE;
                else if (e & (EdgeTop | EdgeBottom)) cur = IDC_SIZENS;
                SetCursor(LoadCursorW(nullptr, cur));
                return TRUE;
            }
            break;
        case WM_KEYDOWN:
            OnKey(wp);
            return 0;
        case WM_CAPTURECHANGED:
            if (reinterpret_cast<HWND>(lp) != m_hwnd && m_mode != Mode::None) {
                m_mode = Mode::None;
                m_band = {};
                Invalidate();
            }
            break;
    }
    return WindowBase::HandleMessage(msg, wp, lp);
}

void FenceWindow::OnMouseMove(PointI pt, WPARAM keys) {
    if (!m_tracking) {
        TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE | TME_HOVER, m_hwnd, static_cast<DWORD>(C().Cfg().rollup.hoverDelayMs)};
        TrackMouseEvent(&tme);
        m_tracking = true;
        m_hoverWindow = true;
        KillTimer(m_hwnd, kTimerCollapse);
        if (Collapsed() && C().Cfg().rollup.openMode == RollupOpen::Hover) SetExpanded(true);
        Invalidate();
    }
    POINT screen{pt.x, pt.y};
    ClientToScreen(m_hwnd, &screen);
    Group* g = G();
    if (!g) return;

    switch (m_mode) {
        case Mode::None: {
            int hover = Collapsed() ? -1 : m_view.HitTest(pt);
            bool scrollHot = m_view.Scrollable() && m_view.ScrollTrack().Inflate(Px(4)).Contains(pt);
            if (hover != m_view.hover || scrollHot) {
                m_view.hover = hover;
                m_view.SetScrollbarHot(scrollHot);
                Invalidate();
            }
            return;
        }
        case Mode::TitlePress:
        case Mode::ItemPress:
        case Mode::TabPress: {
            int dx = screen.x - m_pressScreen.x, dy = screen.y - m_pressScreen.y;
            bool moved = std::abs(dx) > GetSystemMetrics(SM_CXDRAG) || std::abs(dy) > GetSystemMetrics(SM_CYDRAG);
            if (!moved) return;
            if (m_mode == Mode::ItemPress) {
                StartItemDrag();
                return;
            }
            if (m_mode == Mode::TabPress) {
                if (std::abs(dy) > Px(22) && g->tabs.size() > 1) {
                    m_mode = Mode::TabDetach;
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                    return;
                }
                int over = TabAt({pt.x, std::clamp(pt.y, Px(4), TitleH() - Px(4))});
                if (over >= 0 && over != m_pressTab && !C().IsLayoutLocked(g)) {
                    C().ReorderTab(m_groupId, m_pressTab, over);
                    m_pressTab = over;
                }
                return;
            }
            if (C().IsLayoutLocked(g)) {
                m_mode = Mode::None;
                return;
            }
            m_mode = Mode::Move;
            [[fallthrough]];
        }
        case Mode::Move: {
            RectI proposed = m_startRect.Offset(screen.x - m_pressScreen.x, screen.y - m_pressScreen.y);
            const MonitorDesc* mon = MonitorForRect(C().Monitors(), proposed);
            RectI snapped = mon ? SnapMove(proposed, C().GroupRectsOnMonitor(mon->id, m_groupId), mon->work, C().Snap(Scale())) : proposed;
            C().Host().SetScreenRect(m_hwnd, snapped);
            // Dropping the title onto another fence's title merges them into tabs.
            FenceWindow* target = C().FenceAtTitle(screen, this);
            std::string targetId = target ? target->GroupId() : std::string();
            if (targetId != m_mergeTarget) {
                m_mergeTarget = targetId;
                C().HighlightMergeTarget(targetId);
            }
            return;
        }
        case Mode::Resize: {
            int dx = screen.x - m_pressScreen.x, dy = screen.y - m_pressScreen.y;
            RectI r = m_startRect;
            if (m_resizeEdges & EdgeLeft) r.left += dx;
            if (m_resizeEdges & EdgeRight) r.right += dx;
            if (m_resizeEdges & EdgeTop) r.top += dy;
            if (m_resizeEdges & EdgeBottom) r.bottom += dy;
            const MonitorDesc* mon = MonitorForRect(C().Monitors(), m_startRect);
            if (mon) r = SnapResize(r, m_resizeEdges, C().GroupRectsOnMonitor(mon->id, m_groupId), mon->work, C().Snap(Scale()), Px(120), TitleH() + Px(40));
            C().Host().SetScreenRect(m_hwnd, r);
            UpdateViewport();
            Invalidate();
            return;
        }
        case Mode::Band: {
            m_band = Normalized(m_pressClient, pt);
            m_band = Intersect(m_band, BodyRect());
            m_view.SelectInRect(m_band, (keys & MK_CONTROL) != 0);
            Invalidate();
            return;
        }
        case Mode::Scroll: {
            RectI track = m_view.ScrollTrack();
            int trackLen = m_view.Horizontal() ? track.Width() : track.Height();
            int delta = m_view.Horizontal() ? pt.x - m_pressClient.x : pt.y - m_pressClient.y;
            int range = std::max(1, m_view.ContentExtent() - m_view.ViewExtent());
            m_view.ScrollTo(m_scrollStart + delta * range / std::max(1, trackLen - Px(24)));
            Invalidate();
            return;
        }
        default:
            return;
    }
}

void FenceWindow::OnLButtonDown(PointI pt, WPARAM keys) {
    inline_editor::Cancel();
    Group* g = G();
    if (!g) return;
    SetFocus(m_hwnd);
    m_pressScreen = {pt.x, pt.y};
    ClientToScreen(m_hwnd, &m_pressScreen);
    m_pressClient = pt;
    m_startRect = ScreenRect();
    SetCapture(m_hwnd);

    if (unsigned edges = BorderHit(pt); edges != EdgeNone) {
        m_mode = Mode::Resize;
        m_resizeEdges = edges;
        return;
    }
    if (TitleRect().Contains(pt)) {
        if (MenuButtonRect().Contains(pt)) {
            ReleaseCapture();
            m_mode = Mode::None;
            POINT s{m_startRect.left, m_startRect.top + TitleH()};
            C().ShowFenceMenu(this, s);
            return;
        }
        if (m_hoverWindow && !C().IsLayoutLocked(g) && CloseButtonRect().Contains(pt)) {
            ReleaseCapture();
            m_mode = Mode::None;
            C().DeleteGroup(m_groupId, true);
            return;
        }
        int tab = TabAt(pt);
        if (tab >= 0) {
            if (tab != g->activeTab) {
                g->activeTab = tab;
                C().SaveSoon();
                SyncFromModel();
            }
            m_pressTab = tab;
            m_mode = Mode::TabPress;
            return;
        }
        m_mode = Mode::TitlePress;
        return;
    }
    if (Collapsed()) {
        m_mode = Mode::TitlePress;
        return;
    }
    if (BreadcrumbRect().Contains(pt) && !BreadcrumbRect().Empty()) {
        ReleaseCapture();
        m_mode = Mode::None;
        if (Tab* t = ActiveTab()) C().PortalBack(t->id);
        return;
    }
    if (m_view.Scrollable() && m_view.ScrollTrack().Inflate(Px(4)).Contains(pt)) {
        RectI thumb = m_view.ScrollThumb();
        if (!thumb.Inflate(Px(2)).Contains(pt)) {
            bool before = m_view.Horizontal() ? pt.x < thumb.left : pt.y < thumb.top;
            m_view.ScrollBy((before ? -1 : 1) * m_view.ViewExtent() * 9 / 10);
        }
        m_mode = Mode::Scroll;
        m_scrollStart = m_view.ScrollPos();
        Invalidate();
        return;
    }
    int idx = m_view.HitTest(pt);
    if (idx >= 0) {
        m_pressIndex = idx;
        m_pressWasSelected = m_view.IsSelected(idx);
        if (keys & MK_CONTROL) m_view.Toggle(idx);
        else if (keys & MK_SHIFT) m_view.SelectRange(m_view.anchor, idx);
        else if (!m_pressWasSelected) m_view.SelectOnly(idx);
        m_view.focus = idx;
        m_mode = Mode::ItemPress;
    } else {
        if (!(keys & MK_CONTROL)) m_view.ClearSelection();
        m_mode = Mode::Band;
        m_band = {};
    }
    Invalidate();
}

void FenceWindow::OnLButtonUp(PointI pt, WPARAM keys) {
    Mode mode = m_mode;
    m_mode = Mode::None;
    if (GetCapture() == m_hwnd) ReleaseCapture();
    Group* g = G();
    if (!g) return;
    switch (mode) {
        case Mode::Move: {
            std::string target = m_mergeTarget;
            m_mergeTarget.clear();
            C().HighlightMergeTarget({});
            if (!target.empty()) {
                C().MergeGroups(m_groupId, target);  // destroys this window
                return;
            }
            RectI r = ScreenRect();
            if (Collapsed()) {
                RectI full = C().GroupScreenRect(*g);
                r = RectI::FromXYWH(r.left, r.top, full.Width(), full.Height());
            }
            C().SetGroupRect(m_groupId, r.Offset(-m_offsetX, 0));
            C().InvalidateAll();
            return;
        }
        case Mode::Resize:
            C().SetGroupRect(m_groupId, ScreenRect().Offset(-m_offsetX, 0));
            return;
        case Mode::TabDetach: {
            POINT s{pt.x, pt.y};
            ClientToScreen(m_hwnd, &s);
            RectI full = C().GroupScreenRect(*g);
            C().DetachTab(m_groupId, m_pressTab, RectI::FromXYWH(s.x - full.Width() / 2, s.y - TitleH() / 2, full.Width(), full.Height()));
            return;
        }
        case Mode::TitlePress:
            if (C().Cfg().rollup.openMode == RollupOpen::Click && (g->rolledUp || AutoRollup())) SetExpanded(!m_expanded);
            return;
        case Mode::ItemPress:
            if (m_pressWasSelected && !(keys & (MK_CONTROL | MK_SHIFT))) m_view.SelectOnly(m_pressIndex);
            Invalidate();
            return;
        case Mode::Band:
            m_band = {};
            Invalidate();
            return;
        default:
            return;
    }
}

void FenceWindow::OnDoubleClick(PointI pt, WPARAM keys) {
    Group* g = G();
    if (!g) return;
    if (TitleRect().Contains(pt) && !MenuButtonRect().Contains(pt)) {
        // Double-click the title bar: roll up / roll down.
        g->rolledUp = !g->rolledUp;
        m_expanded = false;
        C().SaveSoon();
        ApplyGeometry(m_offsetX);
        Invalidate();
        return;
    }
    int idx = m_view.HitTest(pt);
    if (idx >= 0) OpenItem(idx, (keys & (MK_CONTROL | MK_SHIFT)) != 0);
}

void FenceWindow::OpenItem(int index, bool forceExplorer) {
    if (index < 0 || index >= m_view.Count()) return;
    ShellItemPtr item = m_view.Items()[static_cast<size_t>(index)];
    Tab* t = ActiveTab();
    if (t && t->kind == TabKind::Portal && item->isBrowsable && t->allowNavigation && !forceExplorer) {
        C().PortalNavigate(t->id, item);
        return;
    }
    if (forceExplorer && item->isBrowsable) OpenInExplorer(item->pidl.get());
    else of::OpenItem(m_hwnd, *item);
    C().OnItemOpened();
}

void FenceWindow::OnRightClick(PointI pt) {
    POINT screen{pt.x, pt.y};
    ClientToScreen(m_hwnd, &screen);
    Tab* t = ActiveTab();
    int idx = Collapsed() ? -1 : m_view.HitTest(pt);
    if (idx >= 0 && t) {
        if (!m_view.IsSelected(idx)) m_view.SelectOnly(idx);
        Invalidate();
        RectI label = m_view.LabelRect(idx);
        RectI s = ScreenRect();
        C().ShowItemMenu(m_hwnd, m_view.SelectedItems(), screen, t->id, t->kind == TabKind::Items, label.Offset(s.left, s.top),
                         m_view.Style().fontPx);
        return;
    }
    C().ShowFenceMenu(this, screen);
}

void FenceWindow::OnWheel(int delta, bool horizontal) {
    if (Collapsed() || !m_view.Scrollable()) return;
    UINT lines = 3;
    SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    int step = m_view.Horizontal() ? m_view.CellSpec().cellW : m_view.CellSpec().cellH;
    int amount = -delta * static_cast<int>(std::max(1u, lines)) * step / (3 * WHEEL_DELTA);
    (void)horizontal;
    m_view.ScrollBy(amount);
    Invalidate();
}

void FenceWindow::OnKey(WPARAM vk) {
    bool ctrl = GetKeyState(VK_CONTROL) < 0;
    bool shift = GetKeyState(VK_SHIFT) < 0;
    auto sel = m_view.SelectedItems();
    Tab* t = ActiveTab();
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
                m_view.EnsureVisible(next);
                Invalidate();
            }
            return;
        }
        case VK_RETURN:
            for (int i : m_view.SelectedIndices()) OpenItem(i, false);
            return;
        case VK_BACK:
            if (t && t->kind == TabKind::Portal) C().PortalBack(t->id);
            return;
        case VK_DELETE:
            if (!sel.empty()) InvokeVerb(m_hwnd, sel, "delete");
            return;
        case VK_F2:
            if (!sel.empty()) RenameSelected();
            else BeginTitleRename();
            return;
        case VK_F5:
            if (t && t->kind == TabKind::Portal) C().PortalRefresh(t->id);
            else C().ExecuteCommand(L"refresh", L"");
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
            PasteHere();
        }
    }
}

void FenceWindow::RenameSelected() {
    auto idx = m_view.SelectedIndices();
    if (idx.empty()) return;
    RectI s = ScreenRect();
    C().RenameItemInteractive(m_hwnd, m_view.Items()[static_cast<size_t>(idx[0])], m_view.LabelRect(idx[0]).Offset(s.left, s.top),
                              m_view.Style().fontPx);
}

void FenceWindow::PasteHere() {
    Tab* t = ActiveTab();
    if (!t) return;
    if (t->kind == TabKind::Portal) {
        if (PortalModel* pm = C().Portal(t->id); pm && !pm->stack.empty()) PasteIntoFolder(m_hwnd, pm->stack.back().get());
        return;
    }
    ComPtr<IDataObject> data;
    if (SUCCEEDED(OleGetClipboard(&data))) C().ExpectDroppedFiles(DroppedFileNames(data.Get()), t->id, L"", {});
    if (auto desk = ParsePath(C().DesktopFolderPath())) PasteIntoFolder(m_hwnd, desk.get());
}

void FenceWindow::StartItemDrag() {
    m_mode = Mode::None;
    if (GetCapture() == m_hwnd) ReleaseCapture();
    Group* g = G();
    Tab* t = ActiveTab();
    auto items = m_view.SelectedItems();
    if (!g || !t || items.empty()) return;
    InternalDrag info;
    info.desktopItems = t->kind == TabKind::Items;
    info.sourceTabId = t->id;
    if (info.desktopItems) {
        if (C().IsContentLocked(g)) return;
        for (const auto& i : items) info.keys.push_back(i->key);
    }
    auto px = C().Icons().Pixels(*items[0], m_view.Style().iconPx);
    StartShellDrag(m_hwnd, items, px.get(), std::move(info));
    Invalidate();
}

// ---------------------------------------------------------------- drop target

DWORD FenceWindow::ComputeDrop(DWORD keys, POINT screen, DWORD allowed) {
    Group* g = G();
    if (!g) return DROPEFFECT_NONE;
    POINT cp = screen;
    ScreenToClient(m_hwnd, &cp);
    PointI pt{cp.x, cp.y};

    int tabIdx = g->activeTab;
    int overTab = TabAt(pt);
    if (overTab >= 0) {
        tabIdx = overTab;
        if (overTab != g->activeTab && overTab != m_hoverTab) {
            m_hoverTab = overTab;
            SetTimer(m_hwnd, kTimerTabHover, 500, nullptr);
        }
    } else {
        m_hoverTab = -1;
        KillTimer(m_hwnd, kTimerTabHover);
    }
    const Tab& tab = g->tabs[static_cast<size_t>(std::clamp(tabIdx, 0, static_cast<int>(g->tabs.size()) - 1))];
    m_dropTabId = tab.id;
    InternalDrag& drag = CurrentDrag();
    DWORD effect = DROPEFFECT_NONE;
    bool sameTabManual = tabIdx == g->activeTab && tab.sort == SortMode::Manual && overTab < 0;
    m_dropIndex = -1;

    if (tab.kind == TabKind::Portal) {
        PortalModel* pm = C().Portal(tab.id);
        if (drag.active && drag.sourceTabId == tab.id) {
            m_forward.Leave();
            m_dropMode = DropMode::None;
            effect = DROPEFFECT_NONE;
        } else if (pm && !pm->stack.empty()) {
            effect = m_forward.Enter(m_hwnd, pm->stack.back().get(), m_dragData.Get(), keys, screen, allowed);
            m_dropMode = DropMode::ForwardFolder;
        }
    } else if (C().IsContentLocked(g)) {
        m_forward.Leave();
        m_dropMode = DropMode::None;
    } else if (drag.active && drag.desktopItems) {
        m_forward.Leave();
        m_dropMode = DropMode::Internal;
        effect = DROPEFFECT_MOVE;
        if (sameTabManual) m_dropIndex = m_view.InsertIndexAt(pt);
    } else if (auto desk = ParsePath(C().DesktopFolderPath())) {
        effect = m_forward.Enter(m_hwnd, desk.get(), m_dragData.Get(), keys, screen, allowed);
        m_dropMode = DropMode::ForwardDesktop;
        if (sameTabManual) m_dropIndex = m_view.InsertIndexAt(pt);
    }
    m_view.SetInsertMarker(m_dropIndex);
    bool highlight = effect != DROPEFFECT_NONE;
    if (highlight != m_dropHighlight) m_dropHighlight = highlight;
    Invalidate();
    return effect;
}

DWORD FenceWindow::DragEnter(IDataObject* data, DWORD keys, POINT pt, DWORD allowed) {
    m_dragData = data;
    KillTimer(m_hwnd, kTimerCollapse);
    if (Collapsed()) SetExpanded(true);
    return ComputeDrop(keys, pt, allowed);
}

DWORD FenceWindow::DragOver(DWORD keys, POINT pt, DWORD allowed) { return ComputeDrop(keys, pt, allowed); }

void FenceWindow::DragLeave() {
    m_forward.Leave();
    m_dragData.Reset();
    m_dropMode = DropMode::None;
    m_dropHighlight = false;
    m_hoverTab = -1;
    KillTimer(m_hwnd, kTimerTabHover);
    m_view.SetInsertMarker(-1);
    if (m_expanded) SetTimer(m_hwnd, kTimerCollapse, 400, nullptr);
    Invalidate();
}

DWORD FenceWindow::Drop(IDataObject* data, DWORD keys, POINT pt, DWORD allowed) {
    m_dragData = data;
    ComputeDrop(keys, pt, allowed);
    DropMode mode = m_dropMode;
    std::string tabId = m_dropTabId;
    int index = m_dropIndex;
    DWORD effect = DROPEFFECT_NONE;
    switch (mode) {
        case DropMode::Internal:
            CurrentDrag().dropHandledInternally = true;
            C().AssignItems(CurrentDrag().keys, tabId, index);
            break;
        case DropMode::ForwardFolder:
            effect = m_forward.Drop(data, keys, pt, allowed);
            break;
        case DropMode::ForwardDesktop:
            C().ExpectDroppedFiles(DroppedFileNames(data), tabId, L"", {});
            effect = m_forward.Drop(data, keys, pt, allowed);
            break;
        default:
            break;
    }
    m_dropMode = DropMode::None;
    m_dragData.Reset();
    m_dropHighlight = false;
    m_view.SetInsertMarker(-1);
    KillTimer(m_hwnd, kTimerTabHover);
    Invalidate();
    return effect;
}

}  // namespace of
