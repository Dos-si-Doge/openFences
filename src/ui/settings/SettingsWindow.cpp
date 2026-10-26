// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "ui/settings/SettingsWindow.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>

#include "app/Controller.h"
#include "resource.h"

namespace of {

namespace {
constexpr wchar_t kFrameClass[] = L"openFences.Settings";
constexpr wchar_t kPanelClass[] = L"openFences.SettingsPanel";
constexpr int kSidebarId = 1;
constexpr int kWelcomeButtonId = 2;
constexpr int kMutedId = 0xFFFE;
}  // namespace

const std::vector<SettingsWindow::PageDef>& SettingsWindow::Pages() {
    static const std::vector<PageDef> pages = {
        {L"welcome", L"Welcome", &SettingsWindow::BuildWelcome, &SettingsWindow::OnWelcome},
        {L"chameleon", L"Chameleon", &SettingsWindow::BuildChameleon, &SettingsWindow::OnChameleon},
        {L"personalization", L"Personalization", &SettingsWindow::BuildPersonalization, &SettingsWindow::OnPersonalization},
        {L"portals", L"Folder Portals", &SettingsWindow::BuildPortals, &SettingsWindow::OnPortals},
        {L"peek", L"Peek", &SettingsWindow::BuildPeek, &SettingsWindow::OnPeek},
        {L"hide", L"Hide", &SettingsWindow::BuildHide, &SettingsWindow::OnHide},
        {L"rollup", L"Rollup", &SettingsWindow::BuildRollup, &SettingsWindow::OnRollup},
        {L"tabs", L"Tabs", &SettingsWindow::BuildTabs, &SettingsWindow::OnTabs},
        {L"pages", L"Pages", &SettingsWindow::BuildPages, &SettingsWindow::OnPages},
        {L"rules", L"Rules", &SettingsWindow::BuildRules, &SettingsWindow::OnRules},
        {L"layout", L"Layout", &SettingsWindow::BuildLayout, &SettingsWindow::OnLayout},
        {L"business", L"Business", &SettingsWindow::BuildBusiness, &SettingsWindow::OnBusiness},
        {L"about", L"About", &SettingsWindow::BuildAbout, &SettingsWindow::OnAbout},
    };
    return pages;
}

SettingsWindow::~SettingsWindow() {
    if (m_hwnd) DestroyWindow(m_hwnd);
    if (m_font) DeleteObject(m_font);
    if (m_headFont) DeleteObject(m_headFont);
    if (m_navFont) DeleteObject(m_navFont);
}

void SettingsWindow::CreateFonts() {
    if (m_font) DeleteObject(m_font);
    if (m_headFont) DeleteObject(m_headFont);
    if (m_navFont) DeleteObject(m_navFont);
    m_font = CreateUiFont(m_dpi, 9, false);
    m_headFont = CreateUiFont(m_dpi, 13, true);
    m_navFont = CreateUiFont(m_dpi, 10, false);
}

void SettingsWindow::Create() {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    if (!GetClassInfoExW(inst, kFrameClass, &wc)) {
        wc = WNDCLASSEXW{sizeof(wc)};
        wc.lpfnWndProc = FrameProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        wc.lpszClassName = kFrameClass;
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP));
        RegisterClassExW(&wc);
        WNDCLASSEXW pc{sizeof(pc)};
        pc.lpfnWndProc = PanelProc;
        pc.hInstance = inst;
        pc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        pc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        pc.lpszClassName = kPanelClass;
        RegisterClassExW(&pc);
    }
    POINT pt;
    GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    UINT dx = 96, dy = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy);
    int w = MulDiv(900, static_cast<int>(dx), 96), h = MulDiv(640, static_cast<int>(dx), 96);
    int x = (mi.rcWork.left + mi.rcWork.right - w) / 2, y = (mi.rcWork.top + mi.rcWork.bottom - h) / 2;
    m_hwnd = CreateWindowExW(WS_EX_CONTROLPARENT, kFrameClass, L"Configure openFences", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, w, h, nullptr,
                             nullptr, inst, this);
    m_dpi = GetDpiForWindow(m_hwnd);
    CreateFonts();
    m_sidebar = CreateWindowExW(0, WC_LISTBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                                0, 0, 10, 10, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSidebarId)), inst, nullptr);
    SendMessageW(m_sidebar, WM_SETFONT, reinterpret_cast<WPARAM>(m_navFont), TRUE);
    SendMessageW(m_sidebar, LB_SETITEMHEIGHT, 0, MulDiv(36, static_cast<int>(m_dpi), 96));
    for (const auto& p : Pages()) SendMessageW(m_sidebar, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p.title));
    m_welcomeButton = CreateWindowExW(0, L"BUTTON", L"Welcome to openFences", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 10, 10, m_hwnd,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kWelcomeButtonId)), inst, nullptr);
    SendMessageW(m_welcomeButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    m_panel = CreateWindowExW(WS_EX_CONTROLPARENT, kPanelClass, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 10, 10, m_hwnd,
                              nullptr, inst, this);
    LayoutChildren();
}

void SettingsWindow::Show(const std::wstring& page) {
    if (!m_hwnd) Create();
    int index = m_page;
    for (size_t i = 0; i < Pages().size(); ++i)
        if (page == Pages()[i].key) index = static_cast<int>(i);
    SelectPage(index);
    ShowWindow(m_hwnd, IsIconic(m_hwnd) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(m_hwnd);
}

bool SettingsWindow::TranslateDialogMessage(MSG* msg) {
    return m_hwnd && IsWindowVisible(m_hwnd) && IsDialogMessageW(m_hwnd, msg);
}

void SettingsWindow::LayoutChildren() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int side = MulDiv(200, static_cast<int>(m_dpi), 96);
    int footer = MulDiv(56, static_cast<int>(m_dpi), 96), pad = MulDiv(12, static_cast<int>(m_dpi), 96);
    MoveWindow(m_sidebar, 0, 0, side, rc.bottom - footer, TRUE);
    MoveWindow(m_welcomeButton, pad, rc.bottom - footer + pad / 2, side - 2 * pad, footer - pad - pad / 2, TRUE);
    MoveWindow(m_panel, side, 0, rc.right - side, rc.bottom, TRUE);
}

void SettingsWindow::SelectPage(int index) {
    m_page = std::clamp(index, 0, static_cast<int>(Pages().size()) - 1);
    SendMessageW(m_sidebar, LB_SETCURSEL, static_cast<WPARAM>(m_page), 0);
    m_scroll = 0;
    RebuildPage();
}

void SettingsWindow::RebuildPage() {
    m_building = true;
    SendMessageW(m_panel, WM_SETREDRAW, FALSE, 0);
    while (HWND child = GetWindow(m_panel, GW_CHILD)) DestroyWindow(child);
    RECT rc;
    GetClientRect(m_panel, &rc);
    int margin = MulDiv(28, static_cast<int>(m_dpi), 96);
    int width = std::max(MulDiv(320, static_cast<int>(m_dpi), 96), static_cast<int>(rc.right) - 2 * margin);
    m_form.Reset(m_panel, m_font, m_headFont, m_dpi, width, 0);
    (this->*Pages()[static_cast<size_t>(m_page)].build)();
    m_contentHeight = m_form.Height() + 2 * margin;
    // Shift everything by the margin and current scroll position.
    for (HWND child = GetWindow(m_panel, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        RECT cr;
        GetWindowRect(child, &cr);
        MapWindowPoints(nullptr, m_panel, reinterpret_cast<POINT*>(&cr), 2);
        SetWindowPos(child, nullptr, cr.left + margin, cr.top + margin - m_scroll, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS, 0, m_contentHeight, static_cast<UINT>(rc.bottom), m_scroll};
    SetScrollInfo(m_panel, SB_VERT, &si, TRUE);
    SendMessageW(m_panel, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(m_panel, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    m_building = false;
}

void SettingsWindow::ScrollTo(int pos) {
    RECT rc;
    GetClientRect(m_panel, &rc);
    pos = std::clamp(pos, 0, std::max(0, m_contentHeight - static_cast<int>(rc.bottom)));
    int delta = m_scroll - pos;
    if (!delta) return;
    m_scroll = pos;
    ScrollWindowEx(m_panel, 0, delta, nullptr, nullptr, nullptr, nullptr, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    SetScrollPos(m_panel, SB_VERT, m_scroll, TRUE);
}

void SettingsWindow::Changed() {
    if (m_building) return;
    Controller::Get().SettingsChanged();
}

bool SettingsWindow::Checked(int id) const { return IsDlgButtonChecked(m_panel, id) == BST_CHECKED; }
int SettingsWindow::ComboIndex(int id) const { return static_cast<int>(SendDlgItemMessageW(m_panel, id, CB_GETCURSEL, 0, 0)); }
int SettingsWindow::SliderPos(int id) const { return static_cast<int>(SendDlgItemMessageW(m_panel, id, TBM_GETPOS, 0, 0)); }

std::wstring SettingsWindow::TextOf(int id) const {
    HWND h = GetDlgItem(m_panel, id);
    int len = GetWindowTextLengthW(h);
    std::wstring s(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    s.resize(static_cast<size_t>(len));
    return s;
}

void SettingsWindow::OnSlider(HWND slider) {
    int id = GetDlgCtrlID(slider);
    int pos = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
    m_form.UpdateSliderLabel(id, pos);
    OnSliderChanged(id, pos);
}

void SettingsWindow::OnDrawItem(DRAWITEMSTRUCT* dis) {
    if (dis->CtlType == ODT_LISTBOX && dis->CtlID == kSidebarId) {
        if (dis->itemID == static_cast<UINT>(-1)) return;
        bool sel = (dis->itemState & ODS_SELECTED) != 0;
        HBRUSH bg = CreateSolidBrush(sel ? RGB(225, 235, 250) : GetSysColor(COLOR_WINDOW));
        FillRect(dis->hDC, &dis->rcItem, bg);
        DeleteObject(bg);
        if (sel) {
            RECT bar = dis->rcItem;
            bar.right = bar.left + MulDiv(4, static_cast<int>(m_dpi), 96);
            HBRUSH accent = CreateSolidBrush(RGB(40, 110, 220));
            FillRect(dis->hDC, &bar, accent);
            DeleteObject(accent);
        }
        wchar_t text[64] = {};
        SendMessageW(m_sidebar, LB_GETTEXT, dis->itemID, reinterpret_cast<LPARAM>(text));
        RECT tr = dis->rcItem;
        tr.left += MulDiv(18, static_cast<int>(m_dpi), 96);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, GetSysColor(COLOR_WINDOWTEXT));
        HGDIOBJ old = SelectObject(dis->hDC, m_navFont);
        DrawTextW(dis->hDC, text, -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        SelectObject(dis->hDC, old);
        return;
    }
    if (m_form.DrawColorButton(dis)) return;
    DrawSnapshotPreview(dis);
}

LRESULT CALLBACK SettingsWindow::FrameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        self->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->OnFrameMessage(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT SettingsWindow::OnFrameMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            if (m_sidebar) {
                LayoutChildren();
                RebuildPage();
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize = {MulDiv(640, static_cast<int>(m_dpi), 96), MulDiv(420, static_cast<int>(m_dpi), 96)};
            return 0;
        }
        case WM_DPICHANGED: {
            m_dpi = HIWORD(wp);
            CreateFonts();
            SendMessageW(m_sidebar, WM_SETFONT, reinterpret_cast<WPARAM>(m_navFont), TRUE);
            SendMessageW(m_sidebar, LB_SETITEMHEIGHT, 0, MulDiv(36, static_cast<int>(m_dpi), 96));
            SendMessageW(m_welcomeButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
            auto* r = reinterpret_cast<RECT*>(lp);
            SetWindowPos(m_hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == kWelcomeButtonId) {
                Controller::Get().OpenWelcome();
                return 0;
            }
            if (LOWORD(wp) == kSidebarId && HIWORD(wp) == LBN_SELCHANGE) {
                SelectPage(static_cast<int>(SendMessageW(m_sidebar, LB_GETCURSEL, 0, 0)));
                return 0;
            }
            break;
        case WM_DRAWITEM:
            OnDrawItem(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
            return TRUE;
        case WM_CLOSE:
            ShowWindow(m_hwnd, SW_HIDE);
            return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

LRESULT CALLBACK SettingsWindow::PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self && self->m_panel ? self->OnPanelMessage(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT SettingsWindow::OnPanelMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND:
            if (!m_building) (this->*Pages()[static_cast<size_t>(m_page)].command)(LOWORD(wp), HIWORD(wp));
            return 0;
        case WM_HSCROLL:
            if (lp && !m_building) OnSlider(reinterpret_cast<HWND>(lp));
            return 0;
        case WM_NOTIFY:
            return OnNotify(reinterpret_cast<NMHDR*>(lp));
        case WM_DRAWITEM:
            OnDrawItem(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
            return TRUE;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkColor(dc, GetSysColor(COLOR_WINDOW));
            SetTextColor(dc, GetDlgCtrlID(reinterpret_cast<HWND>(lp)) == kMutedId ? GetSysColor(COLOR_GRAYTEXT) : GetSysColor(COLOR_WINDOWTEXT));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        case WM_CTLCOLORBTN:
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        case WM_VSCROLL: {
            SCROLLINFO si{sizeof(si), SIF_ALL};
            GetScrollInfo(m_panel, SB_VERT, &si);
            int pos = m_scroll;
            int line = MulDiv(40, static_cast<int>(m_dpi), 96);
            switch (LOWORD(wp)) {
                case SB_LINEUP: pos -= line; break;
                case SB_LINEDOWN: pos += line; break;
                case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
                case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: pos = si.nTrackPos; break;
                case SB_TOP: pos = 0; break;
                case SB_BOTTOM: pos = m_contentHeight; break;
            }
            ScrollTo(pos);
            return 0;
        }
        case WM_MOUSEWHEEL:
            ScrollTo(m_scroll - GET_WHEEL_DELTA_WPARAM(wp) * MulDiv(60, static_cast<int>(m_dpi), 96) / WHEEL_DELTA);
            return 0;
    }
    return DefWindowProcW(m_panel, msg, wp, lp);
}

}  // namespace of
