// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "ui/settings/Form.h"

#include <commctrl.h>
#include <shellscalingapi.h>
#include <uxtheme.h>

#include "app/Controller.h"
#include "core/Hotkey.h"

namespace of {

HFONT CreateUiFont(UINT dpi, int pointSize, bool semibold) {
    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, dpi);
    LOGFONTW lf = ncm.lfMessageFont;
    lf.lfHeight = -MulDiv(pointSize, static_cast<int>(dpi), 72);
    if (semibold) lf.lfWeight = FW_SEMIBOLD;
    return CreateFontIndirectW(&lf);
}

void Form::Reset(HWND parent, HFONT font, HFONT headFont, UINT dpi, int widthPx, int originY) {
    m_parent = parent;
    m_font = font;
    m_head = headFont;
    m_dpi = dpi;
    m_width = widthPx;
    m_y = originY;
    m_colors.clear();
    m_units.clear();
}

HWND Form::Make(const wchar_t* cls, const std::wstring& text, DWORD style, int x, int y, int w, int h, int id, DWORD ex) {
    HWND hwnd = CreateWindowExW(ex, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, x, y, w, h, m_parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    return hwnd;
}

HWND Form::Heading(const std::wstring& text) {
    if (m_y > 0) m_y += Px(10);
    HWND h = Make(L"STATIC", text, SS_LEFT | SS_NOPREFIX, 0, m_y, m_width, Px(26), 0);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(m_head), TRUE);
    m_y += Px(32);
    return h;
}

HWND Form::Text(const std::wstring& text, bool muted) {
    HDC dc = GetDC(m_parent);
    HGDIOBJ old = SelectObject(dc, m_font);
    RECT rc{0, 0, m_width, 0};
    DrawTextW(dc, text.c_str(), -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    ReleaseDC(m_parent, dc);
    HWND h = Make(L"STATIC", text, SS_LEFT | SS_NOPREFIX, 0, m_y, m_width, rc.bottom + Px(2), muted ? 0xFFFE : 0);
    m_y += rc.bottom + Px(8);
    return h;
}

HWND Form::Check(int id, const std::wstring& text, bool checked, bool enabled) {
    HWND h = Make(L"BUTTON", text, BS_AUTOCHECKBOX | WS_TABSTOP | (enabled ? 0 : WS_DISABLED), 0, m_y, m_width, Px(24), id);
    SendMessageW(h, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    m_y += Px(28);
    return h;
}

HWND Form::RowLabel(const std::wstring& label, int h) {
    return Make(L"STATIC", label, SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX, 0, m_y, m_width * 2 / 5 - Px(8), h, 0);
}

HWND Form::Combo(int id, const std::wstring& label, const std::vector<std::wstring>& items, int selected, bool enabled) {
    int x = label.empty() ? 0 : m_width * 2 / 5;
    if (!label.empty()) RowLabel(label, Px(26));
    HWND h = Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP | (enabled ? 0 : WS_DISABLED), x, m_y, m_width - x, Px(300), id);
    for (const auto& item : items) SendMessageW(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
    SendMessageW(h, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
    m_y += Px(34);
    return h;
}

HWND Form::Slider(int id, const std::wstring& label, int min, int max, int value, const std::wstring& unit) {
    int x = m_width * 2 / 5;
    RowLabel(label, Px(28));
    int valueW = Px(56);
    HWND h = Make(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, x, m_y, m_width - x - valueW, Px(28), id);
    SendMessageW(h, TBM_SETRANGEMIN, FALSE, min);
    SendMessageW(h, TBM_SETRANGEMAX, FALSE, max);
    SendMessageW(h, TBM_SETPOS, TRUE, value);
    SendMessageW(h, TBM_SETPAGESIZE, 0, std::max(1, (max - min) / 10));
    m_units[id] = unit;
    Make(L"STATIC", std::to_wstring(value) + unit, SS_LEFT | SS_CENTERIMAGE, m_width - valueW + Px(6), m_y, valueW - Px(6), Px(28), id + 10000);
    m_y += Px(34);
    return h;
}

void Form::UpdateSliderLabel(int id, int value) {
    SetDlgItemTextW(m_parent, id + 10000, (std::to_wstring(value) + m_units[id]).c_str());
}

HWND Form::Edit(int id, const std::wstring& label, const std::wstring& text, bool enabled) {
    int x = label.empty() ? 0 : m_width * 2 / 5;
    if (!label.empty()) RowLabel(label, Px(26));
    HWND h = Make(L"EDIT", text, ES_AUTOHSCROLL | WS_TABSTOP | (enabled ? 0 : WS_DISABLED), x, m_y, m_width - x, Px(26), id, WS_EX_CLIENTEDGE);
    m_y += Px(34);
    return h;
}

namespace {
LRESULT CALLBACK HotkeyProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;
        case WM_SETFOCUS:
            // Free our own shortcuts so they can be recorded.
            Controller::Get().UnregisterHotkeys();
            break;
        case WM_KILLFOCUS:
            Controller::Get().RegisterHotkeys();
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            UINT vk = static_cast<UINT>(wp);
            if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_LWIN || vk == VK_RWIN) return 0;
            if (vk == VK_TAB && !(GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0)) break;
            Hotkey hk;
            hk.vk = vk;
            if (GetKeyState(VK_CONTROL) < 0) hk.modifiers |= MOD_CONTROL;
            if (GetKeyState(VK_MENU) < 0) hk.modifiers |= MOD_ALT;
            if (GetKeyState(VK_SHIFT) < 0) hk.modifiers |= MOD_SHIFT;
            if (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0) hk.modifiers |= MOD_WIN;
            std::wstring text;
            if ((vk == VK_BACK || vk == VK_DELETE) && hk.modifiers == 0) text.clear();
            else if (!KeyName(vk).empty() && hk.modifiers != 0) text = FormatHotkey(hk);
            else return 0;
            SetWindowTextW(hwnd, text.c_str());
            SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), EN_CHANGE), reinterpret_cast<LPARAM>(hwnd));
            return 0;
        }
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            return 0;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, HotkeyProc, 1);
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}
}  // namespace

HWND Form::HotkeyEdit(int id, const std::wstring& label, const std::wstring& text, const std::wstring& placeholder) {
    HWND h = Edit(id, label, text);
    SendMessageW(h, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(placeholder.c_str()));
    SetWindowSubclass(h, HotkeyProc, 1, 0);
    return h;
}

HWND Form::ColorButton(int id, const std::wstring& label, const Color& color) {
    int x = m_width * 2 / 5;
    RowLabel(label, Px(28));
    HWND h = Make(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, x, m_y, Px(110), Px(28), id);
    m_colors[id] = color;
    m_y += Px(34);
    return h;
}

std::optional<Color> Form::ColorOf(int id) const {
    auto it = m_colors.find(id);
    if (it == m_colors.end()) return std::nullopt;
    return it->second;
}

void Form::SetColor(int id, const Color& c) {
    m_colors[id] = c;
    InvalidateRect(GetDlgItem(m_parent, id), nullptr, TRUE);
}

bool Form::DrawColorButton(const DRAWITEMSTRUCT* dis) const {
    auto it = m_colors.find(static_cast<int>(dis->CtlID));
    if (it == m_colors.end()) return false;
    RECT rc = dis->rcItem;
    DrawFrameControl(dis->hDC, &rc, DFC_BUTTON, DFCS_BUTTONPUSH | ((dis->itemState & ODS_SELECTED) ? DFCS_PUSHED : 0));
    InflateRect(&rc, -Px(5), -Px(5));
    HBRUSH b = CreateSolidBrush(it->second.ToColorRef());
    FillRect(dis->hDC, &rc, b);
    DeleteObject(b);
    FrameRect(dis->hDC, &rc, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
    if (dis->itemState & ODS_FOCUS) {
        RECT f = dis->rcItem;
        InflateRect(&f, -Px(2), -Px(2));
        DrawFocusRect(dis->hDC, &f);
    }
    return true;
}

std::vector<HWND> Form::Buttons(const std::vector<std::pair<int, std::wstring>>& buttons, bool enabled) {
    std::vector<HWND> out;
    int x = 0;
    HDC dc = GetDC(m_parent);
    HGDIOBJ old = SelectObject(dc, m_font);
    for (const auto& [id, text] : buttons) {
        SIZE sz{};
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &sz);
        int w = std::max(Px(88), static_cast<int>(sz.cx) + Px(24));
        if (x > 0 && x + w > m_width) {
            x = 0;
            m_y += Px(36);
        }
        out.push_back(Make(L"BUTTON", text, BS_PUSHBUTTON | WS_TABSTOP | (enabled ? 0 : WS_DISABLED), x, m_y, w, Px(28), id));
        x += w + Px(8);
    }
    SelectObject(dc, old);
    ReleaseDC(m_parent, dc);
    m_y += Px(38);
    return out;
}

HWND Form::List(int id, int heightDip) {
    HWND h = Make(WC_LISTBOXW, L"", LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP, 0, m_y, m_width, Px(heightDip), id, WS_EX_CLIENTEDGE);
    m_y += Px(heightDip) + Px(8);
    return h;
}

HWND Form::ListView(int id, int heightDip, const std::vector<std::pair<std::wstring, int>>& columns, bool checkboxes) {
    HWND h = Make(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_TABSTOP, 0, m_y, m_width, Px(heightDip), id,
                  WS_EX_CLIENTEDGE);
    SetWindowTheme(h, L"Explorer", nullptr);
    ListView_SetExtendedListViewStyle(h, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | (checkboxes ? LVS_EX_CHECKBOXES : 0));
    int col = 0;
    for (const auto& [title, widthDip] : columns) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = const_cast<wchar_t*>(title.c_str());
        c.cx = Px(widthDip);
        ListView_InsertColumn(h, col++, &c);
    }
    m_y += Px(heightDip) + Px(8);
    return h;
}

HWND Form::OwnerDraw(int id, int heightDip) {
    HWND h = Make(L"STATIC", L"", SS_OWNERDRAW, 0, m_y, m_width, Px(heightDip), id);
    m_y += Px(heightDip) + Px(8);
    return h;
}

// ---------------------------------------------------------------- modal helpers

void RunModalLoop(HWND hwnd, const bool& done) {
    MSG msg;
    while (!done && IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(hwnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

namespace {
struct PromptState {
    bool done = false;
    bool ok = false;
    std::wstring text;
    HWND edit = nullptr;
};

LRESULT CALLBACK PromptProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<PromptState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_COMMAND:
            if (!st) break;
            if (LOWORD(wp) == IDOK) {
                int len = GetWindowTextLengthW(st->edit);
                st->text.resize(static_cast<size_t>(len) + 1);
                GetWindowTextW(st->edit, st->text.data(), len + 1);
                st->text.resize(static_cast<size_t>(len));
                st->ok = true;
                st->done = true;
                return 0;
            }
            if (LOWORD(wp) == IDCANCEL) {
                st->done = true;
                return 0;
            }
            break;
        case WM_CLOSE:
            if (st) st->done = true;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}  // namespace

std::optional<std::wstring> PromptText(HWND owner, const std::wstring& title, const std::wstring& label, const std::wstring& initial) {
    static bool registered = false;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = PromptProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        wc.lpszClassName = L"openFences.Prompt";
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(101));
        RegisterClassExW(&wc);
        registered = true;
    }
    POINT pt;
    GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    UINT dpi = 96, dy = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpi, &dy);
    auto px = [&](int v) { return MulDiv(v, static_cast<int>(dpi), 96); };
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    int w = px(380), h = px(170);
    int x = (mi.rcWork.left + mi.rcWork.right - w) / 2, y = (mi.rcWork.top + mi.rcWork.bottom - h) / 2;
    PromptState st;
    st.text = initial;
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"openFences.Prompt", title.c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                x, y, w, h, owner, nullptr, inst, nullptr);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
    HFONT font = CreateUiFont(dpi, 9, false);
    RECT client;
    GetClientRect(hwnd, &client);
    int cw = client.right - px(32);
    Form f;
    f.Reset(hwnd, font, font, dpi, cw, px(14));
    // Offset controls by the margin.
    HWND lbl = f.Text(label);
    SetWindowPos(lbl, nullptr, px(16), px(14), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    st.edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", initial.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, px(16), px(42), cw,
                              px(26), hwnd, reinterpret_cast<HMENU>(100), inst, nullptr);
    SendMessageW(st.edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    HWND ok = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, client.right - px(196), px(86), px(88), px(28),
                              hwnd, reinterpret_cast<HMENU>(IDOK), inst, nullptr);
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, client.right - px(100), px(86), px(88), px(28), hwnd,
                                  reinterpret_cast<HMENU>(IDCANCEL), inst, nullptr);
    SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(st.edit);
    SendMessageW(st.edit, EM_SETSEL, 0, -1);
    RunModalLoop(hwnd, st.done);
    DestroyWindow(hwnd);
    DeleteObject(font);
    if (!st.ok) return std::nullopt;
    return st.text;
}

}  // namespace of
