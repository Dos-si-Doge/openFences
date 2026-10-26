// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "ui/welcome/WelcomeWindow.h"

#include <commctrl.h>
#include <shellscalingapi.h>
#include <wincodec.h>
#include <windowsx.h>
#include <wrl/client.h>

#include <algorithm>

#include "app/Controller.h"
#include "core/Hotkey.h"
#include "core/Strings.h"
#include "core/Welcome.h"
#include "render/RenderDevice.h"
#include "resource.h"
#include "ui/Help.h"
#include "ui/settings/Form.h"

namespace of {

namespace {
constexpr wchar_t kFrameClass[] = L"openFences.Welcome";
constexpr wchar_t kPageClass[] = L"openFences.WelcomePage";
constexpr int kNavId = 1;
constexpr int kSettingsButtonId = 10;
constexpr int kGuideButtonId = 11;
constexpr int kIssuesButtonId = 12;

constexpr COLORREF kAccent = RGB(40, 110, 220);
constexpr COLORREF kMuted = RGB(92, 99, 114);
constexpr COLORREF kChipFill = RGB(243, 244, 248);
constexpr COLORREF kChipLine = RGB(201, 206, 216);
constexpr COLORREF kChipText = RGB(42, 47, 56);
constexpr COLORREF kFitBackground = RGB(232, 236, 244);

// Chips for one "How to use" row, with {peek} replaced by the registered Peek shortcut.
std::vector<std::wstring> ResolveChips(const std::vector<std::wstring>& keys) {
    std::vector<std::wstring> out;
    for (const auto& k : keys) {
        if (k != L"{peek}") {
            out.push_back(k);
            continue;
        }
        const Hotkey& hk = Controller::Get().ActivePeekHotkey();
        if (!hk.Valid()) {
            out.push_back(L"Not set");  // every candidate was taken; the user can pick one in Settings → Peek
            continue;
        }
        for (const auto& part : SplitList(FormatHotkey(hk), L"+")) out.push_back(part);
    }
    return out;
}

SIZE TextSize(HDC dc, HFONT font, const std::wstring& text) {
    HGDIOBJ old = SelectObject(dc, font);
    SIZE sz{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &sz);
    SelectObject(dc, old);
    return sz;
}

int WrappedHeight(HDC dc, HFONT font, const std::wstring& text, int width) {
    HGDIOBJ old = SelectObject(dc, font);
    RECT rc{0, 0, width, 0};
    DrawTextW(dc, text.c_str(), -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    return rc.bottom;
}

using Microsoft::WRL::ComPtr;

// Opens a page's hero image from the embedded RCDATA resource "WELCOME_<HERO>".
bool OpenHeroFrame(const WelcomePage& page, ComPtr<IWICBitmapFrameDecode>& frame) {
    std::wstring name = L"WELCOME_" + page.hero;
    for (auto& c : name) c = c == L'-' ? L'_' : static_cast<wchar_t>(towupper(c));
    HRSRC res = FindResourceW(nullptr, name.c_str(), RT_RCDATA);
    HGLOBAL mem = res ? LoadResource(nullptr, res) : nullptr;
    auto* bytes = static_cast<BYTE*>(mem ? LockResource(mem) : nullptr);
    IWICImagingFactory* wic = Gfx().Wic();
    if (!bytes || !wic) return false;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    return SUCCEEDED(wic->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromMemory(bytes, SizeofResource(nullptr, res))) &&
           SUCCEEDED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) &&
           SUCCEEDED(decoder->GetFrame(0, &frame));
}
}  // namespace

SIZE WelcomeWindow::HeroImageSize(int index) {
    if (m_heroSizes.empty()) m_heroSizes.assign(WelcomePages().size(), SIZE{-1, -1});
    SIZE& size = m_heroSizes[static_cast<size_t>(index)];
    if (size.cx < 0) {
        size = {0, 0};
        ComPtr<IWICBitmapFrameDecode> frame;
        UINT w = 0, h = 0;
        if (OpenHeroFrame(WelcomePages()[static_cast<size_t>(index)], frame) && SUCCEEDED(frame->GetSize(&w, &h)))
            size = {static_cast<LONG>(w), static_cast<LONG>(h)};
    }
    return size;
}

WelcomeWindow::~WelcomeWindow() {
    if (m_hwnd) DestroyWindow(m_hwnd);
    for (HFONT f : {m_navFont, m_titleFont, m_headFont, m_textFont, m_chipFont})
        if (f) DeleteObject(f);
    if (m_hero) DeleteObject(m_hero);
}

void WelcomeWindow::CreateFonts() {
    for (HFONT* f : {&m_navFont, &m_titleFont, &m_headFont, &m_textFont, &m_chipFont}) {
        if (*f) DeleteObject(*f);
    }
    m_navFont = CreateUiFont(m_dpi, 10, false);
    m_titleFont = CreateUiFont(m_dpi, 20, true);
    m_headFont = CreateUiFont(m_dpi, 12, true);
    m_textFont = CreateUiFont(m_dpi, 10, false);
    m_chipFont = CreateUiFont(m_dpi, 9, true);
}

void WelcomeWindow::Create() {
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
        pc.lpfnWndProc = PageProc;
        pc.hInstance = inst;
        pc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        pc.lpszClassName = kPageClass;
        RegisterClassExW(&pc);
    }
    POINT pt;
    GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    UINT dx = 96, dy = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy);
    int w = std::min<int>(MulDiv(1000, static_cast<int>(dx), 96), mi.rcWork.right - mi.rcWork.left);
    int h = std::min<int>(MulDiv(680, static_cast<int>(dx), 96), mi.rcWork.bottom - mi.rcWork.top);
    int x = (mi.rcWork.left + mi.rcWork.right - w) / 2, y = (mi.rcWork.top + mi.rcWork.bottom - h) / 2;
    m_hwnd = CreateWindowExW(WS_EX_CONTROLPARENT, kFrameClass, L"Welcome to openFences", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, w, h,
                             nullptr, nullptr, inst, this);
    m_dpi = GetDpiForWindow(m_hwnd);
    CreateFonts();
    m_nav = CreateWindowExW(0, WC_LISTBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                            0, 0, 10, 10, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kNavId)), inst, nullptr);
    SendMessageW(m_nav, WM_SETFONT, reinterpret_cast<WPARAM>(m_navFont), TRUE);
    SendMessageW(m_nav, LB_SETITEMHEIGHT, 0, Px(38));
    for (const auto& page : WelcomePages()) SendMessageW(m_nav, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(page.navTitle.c_str()));
    m_page = CreateWindowExW(WS_EX_CONTROLPARENT, kPageClass, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 10, 10, m_hwnd, nullptr,
                             inst, this);
    m_settingsButton = CreateWindowExW(0, L"BUTTON", L"Open settings", WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 10, 10, m_page,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSettingsButtonId)), inst, nullptr);
    m_guideButton = CreateWindowExW(0, L"BUTTON", L"Learn more in the user guide", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 10, 10, m_page,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kGuideButtonId)), inst, nullptr);
    m_issuesButton = CreateWindowExW(0, L"BUTTON", L"Report a bug or idea", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 10, 10, m_page,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIssuesButtonId)), inst, nullptr);
    for (HWND b : {m_settingsButton, m_guideButton, m_issuesButton}) SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(m_textFont), TRUE);
    LayoutChildren();
}

void WelcomeWindow::Show(const std::wstring& pageId) {
    if (!m_hwnd) Create();
    SelectPage(WelcomePageIndex(pageId));
    ShowWindow(m_hwnd, IsIconic(m_hwnd) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_nav);
}

bool WelcomeWindow::TranslateDialogMessage(MSG* msg) {
    return m_hwnd && IsWindowVisible(m_hwnd) && IsDialogMessageW(m_hwnd, msg);
}

void WelcomeWindow::LayoutChildren() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int side = Px(220);
    MoveWindow(m_nav, 0, 0, side, rc.bottom, TRUE);
    MoveWindow(m_page, side, 0, rc.right - side, rc.bottom, TRUE);
    LayoutPage();
}

void WelcomeWindow::SelectPage(int index) {
    m_index = std::clamp(index, 0, static_cast<int>(WelcomePages().size()) - 1);
    SendMessageW(m_nav, LB_SETCURSEL, static_cast<WPARAM>(m_index), 0);
    m_scroll = 0;
    LayoutPage();
}

// Computes the page layout (PowerToys' OOBEPageControl: hero, title, description, "How to use", buttons).
void WelcomeWindow::LayoutPage() {
    if (!m_page) return;
    const WelcomePage& page = WelcomePages()[static_cast<size_t>(m_index)];
    RECT rc;
    GetClientRect(m_page, &rc);
    int w = std::max<int>(Px(300), rc.right);
    int margin = Px(32);
    int textW = w - 2 * margin;
    HDC dc = GetDC(m_page);

    // The hero band is as tall as the whole image at its displayed size (FitHero: proportional, at most native size), so
    // nothing is ever cropped. Screenshots get a padded frame.
    int heroPad = page.heroFit ? Px(16) : 0;
    SIZE img = HeroImageSize(m_index);
    HeroSize shown = FitHero(img.cx, img.cy, w - 2 * heroPad);
    int heroH = shown.height ? shown.height + 2 * heroPad : 0;
    m_heroRect = {0, 0, w, heroH};
    int y = heroH + Px(heroH ? 22 : 28);
    int h = WrappedHeight(dc, m_titleFont, page.title, textW);
    m_titleRect = {margin, y, margin + textW, y + h};
    y += h + Px(8);
    h = WrappedHeight(dc, m_textFont, page.description, textW);
    m_descRect = {margin, y, margin + textW, y + h};
    y += h + Px(20);

    m_rows.clear();
    if (!page.steps.empty()) {
        h = WrappedHeight(dc, m_headFont, L"How to use", textW);
        m_headRect = {margin, y, margin + textW, y + h};
        y += h + Px(12);
        int chipH = Px(26), chipPad = Px(10), plusW = TextSize(dc, m_textFont, L"+").cx + Px(10);
        // The text column starts after the widest chip group so rows line up.
        int chipsW = 0;
        for (const auto& step : page.steps) {
            int cw = 0;
            auto chips = ResolveChips(step.keys);
            for (size_t i = 0; i < chips.size(); ++i) cw += TextSize(dc, m_chipFont, chips[i]).cx + 2 * chipPad + (i ? plusW : 0);
            chipsW = std::max(chipsW, cw);
        }
        chipsW = std::min(chipsW, textW / 2);
        int textX = margin + (chipsW ? chipsW + Px(16) : Px(18));
        for (const auto& step : page.steps) {
            Row row;
            row.chips = ResolveChips(step.keys);
            row.text = step.text;
            int th = WrappedHeight(dc, m_textFont, step.text, margin + textW - textX);
            int rowH = std::max(chipH, th);
            row.chipsRect = {margin, y + (rowH - chipH) / 2, margin + chipsW, y + (rowH - chipH) / 2 + chipH};
            row.textRect = {textX, y + (rowH - th) / 2, margin + textW, y + (rowH - th) / 2 + th};
            m_rows.push_back(std::move(row));
            y += rowH + Px(10);
        }
        y += Px(10);
    }
    int creditsH = page.credits ? WrappedHeight(dc, m_textFont, CreditsLine(), textW) : 0;
    ReleaseDC(m_page, dc);

    // Buttons: "Open settings" (when the topic has a Settings page) and "Learn more" (the guide section).
    int bx = margin, bh = Px(32);
    auto place = [&](HWND button, bool show, int width) {
        ShowWindow(button, show ? SW_SHOW : SW_HIDE);
        if (!show) return;
        SetWindowPos(button, nullptr, bx, y - m_scroll, width, bh, SWP_NOZORDER | SWP_NOACTIVATE);
        bx += width + Px(10);
    };
    place(m_settingsButton, !page.settingsPage.empty(), Px(140));
    place(m_guideButton, !page.guideAnchor.empty(), Px(230));
    place(m_issuesButton, page.credits, Px(180));
    // The overview ends with the credits line (PowerToys' OOBE overview links its release notes and GitHub the same way).
    if (page.credits) {
        y += bh + Px(18);
        m_creditsRect = {margin, y, margin + textW, y + creditsH};
        y += creditsH - bh;
    }
    m_contentHeight = y + bh + Px(28);

    SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS, 0, m_contentHeight, static_cast<UINT>(rc.bottom), m_scroll};
    SetScrollInfo(m_page, SB_VERT, &si, TRUE);
    m_scroll = GetScrollPos(m_page, SB_VERT);
    InvalidateRect(m_page, nullptr, FALSE);
}

void WelcomeWindow::ScrollTo(int pos) {
    RECT rc;
    GetClientRect(m_page, &rc);
    pos = std::clamp(pos, 0, std::max(0, m_contentHeight - static_cast<int>(rc.bottom)));
    if (pos == m_scroll) return;
    m_scroll = pos;
    LayoutPage();
}

// Decodes the page's hero image from the embedded resources and scales it to fit width x height whole, keeping its
// proportions and never enlarging it past its native size.
void WelcomeWindow::EnsureHero(int width, int height) {
    if (m_hero && m_heroPage == m_index && m_heroW == width && m_heroH == height) return;
    if (m_hero) DeleteObject(m_hero);
    m_hero = nullptr;
    m_heroPage = m_index;
    m_heroW = width;
    m_heroH = height;
    const WelcomePage& page = WelcomePages()[static_cast<size_t>(m_index)];
    IWICImagingFactory* wic = Gfx().Wic();
    ComPtr<IWICBitmapFrameDecode> frame;
    if (!wic || !OpenHeroFrame(page, frame)) return;
    UINT iw = 0, ih = 0;
    frame->GetSize(&iw, &ih);
    if (!iw || !ih) return;
    double s = std::min({static_cast<double>(width) / iw, static_cast<double>(height) / ih, 1.0});
    UINT outW = std::max(1u, static_cast<UINT>(iw * s + 0.5)), outH = std::max(1u, static_cast<UINT>(ih * s + 0.5));
    ComPtr<IWICBitmapSource> source = frame;
    ComPtr<IWICBitmapScaler> scaler;
    if (outW != iw || outH != ih) {
        if (FAILED(wic->CreateBitmapScaler(&scaler)) ||
            FAILED(scaler->Initialize(frame.Get(), outW, outH, WICBitmapInterpolationModeHighQualityCubic)))
            return;
        source = scaler;
    }
    ComPtr<IWICFormatConverter> conv;
    if (FAILED(wic->CreateFormatConverter(&conv)) ||
        FAILED(conv->Initialize(source.Get(), GUID_WICPixelFormat32bppBGR, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
        return;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = static_cast<LONG>(outW);
    bi.bmiHeader.biHeight = -static_cast<LONG>(outH);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) return;
    if (FAILED(conv->CopyPixels(nullptr, outW * 4, outW * outH * 4, static_cast<BYTE*>(bits)))) {
        DeleteObject(bmp);
        return;
    }
    m_hero = bmp;
}

void WelcomeWindow::PaintPage(HDC dc) {
    const WelcomePage& page = WelcomePages()[static_cast<size_t>(m_index)];
    RECT rc;
    GetClientRect(m_page, &rc);
    FillRect(dc, &rc, GetSysColorBrush(COLOR_WINDOW));
    SetBkMode(dc, TRANSPARENT);
    auto shift = [&](RECT r) {
        OffsetRect(&r, 0, -m_scroll);
        return r;
    };

    RECT hero = shift(m_heroRect);
    int heroPad = page.heroFit ? Px(16) : 0;
    if (hero.bottom > hero.top) EnsureHero(hero.right - hero.left - 2 * heroPad, hero.bottom - hero.top - 2 * heroPad);
    if (m_hero) {
        // Visible only around screenshots and beside images in a band wider than their native size.
        HBRUSH bg = CreateSolidBrush(kFitBackground);
        FillRect(dc, &hero, bg);
        DeleteObject(bg);
    }
    if (m_hero) {
        BITMAP bm{};
        GetObjectW(m_hero, sizeof(bm), &bm);
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(mem, m_hero);
        int x = hero.left + (hero.right - hero.left - bm.bmWidth) / 2;
        int y = hero.top + (hero.bottom - hero.top - bm.bmHeight) / 2;
        BitBlt(dc, x, y, bm.bmWidth, bm.bmHeight, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
    }

    auto text = [&](HFONT font, COLORREF color, const std::wstring& s, RECT r, UINT flags) {
        HGDIOBJ old = SelectObject(dc, font);
        SetTextColor(dc, color);
        r = shift(r);
        DrawTextW(dc, s.c_str(), -1, &r, flags | DT_NOPREFIX);
        SelectObject(dc, old);
    };
    text(m_titleFont, GetSysColor(COLOR_WINDOWTEXT), page.title, m_titleRect, DT_WORDBREAK);
    text(m_textFont, kMuted, page.description, m_descRect, DT_WORDBREAK);
    if (!m_rows.empty()) text(m_headFont, GetSysColor(COLOR_WINDOWTEXT), L"How to use", m_headRect, DT_SINGLELINE);

    HPEN pen = CreatePen(PS_SOLID, 1, kChipLine);
    HBRUSH fill = CreateSolidBrush(kChipFill);
    for (const Row& row : m_rows) {
        RECT chips = shift(row.chipsRect);
        int x = chips.left;
        if (row.chips.empty()) {
            text(m_textFont, kAccent, L"•", RECT{row.textRect.left - Px(14), row.textRect.top, row.textRect.left, row.textRect.bottom}, DT_SINGLELINE);
        }
        for (size_t i = 0; i < row.chips.size(); ++i) {
            if (i) {
                RECT plus{x, chips.top, x + Px(18), chips.bottom};
                OffsetRect(&plus, 0, m_scroll);
                text(m_textFont, kMuted, L"+", plus, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                x += Px(18);
            }
            int cw = TextSize(dc, m_chipFont, row.chips[i]).cx + Px(20);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, fill);
            RoundRect(dc, x, chips.top, x + cw, chips.bottom, Px(8), Px(8));
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            RECT chip{x, chips.top, x + cw, chips.bottom};
            OffsetRect(&chip, 0, m_scroll);
            text(m_chipFont, kChipText, row.chips[i], chip, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            x += cw;
        }
        text(m_textFont, GetSysColor(COLOR_WINDOWTEXT), row.text, row.textRect, DT_WORDBREAK);
    }
    DeleteObject(pen);
    DeleteObject(fill);
    if (page.credits) text(m_textFont, kMuted, CreditsLine(), m_creditsRect, DT_WORDBREAK);
}

void WelcomeWindow::DrawNavItem(const DRAWITEMSTRUCT* dis) {
    if (dis->itemID == static_cast<UINT>(-1)) return;
    bool sel = (dis->itemState & ODS_SELECTED) != 0;
    HBRUSH bg = CreateSolidBrush(sel ? RGB(225, 235, 250) : GetSysColor(COLOR_WINDOW));
    FillRect(dis->hDC, &dis->rcItem, bg);
    DeleteObject(bg);
    if (sel) {
        RECT bar = dis->rcItem;
        bar.right = bar.left + Px(4);
        HBRUSH accent = CreateSolidBrush(kAccent);
        FillRect(dis->hDC, &bar, accent);
        DeleteObject(accent);
    }
    const WelcomePage& page = WelcomePages()[dis->itemID];
    RECT tr = dis->rcItem;
    tr.left += Px(18);
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, GetSysColor(COLOR_WINDOWTEXT));
    HGDIOBJ old = SelectObject(dis->hDC, m_navFont);
    DrawTextW(dis->hDC, page.navTitle.c_str(), -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dis->hDC, old);
    // A divider before "What's new", like PowerToys' footer items.
    if (page.id == kWhatsNewPageId) {
        HPEN pen = CreatePen(PS_SOLID, 1, kChipLine);
        HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
        MoveToEx(dis->hDC, dis->rcItem.left + Px(12), dis->rcItem.top, nullptr);
        LineTo(dis->hDC, dis->rcItem.right - Px(12), dis->rcItem.top);
        SelectObject(dis->hDC, oldPen);
        DeleteObject(pen);
    }
}

LRESULT CALLBACK WelcomeWindow::FrameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<WelcomeWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        self->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<WelcomeWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->OnFrameMessage(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT WelcomeWindow::OnFrameMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            if (m_nav) LayoutChildren();
            return 0;
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize = {Px(640), Px(460)};
            return 0;
        }
        case WM_DPICHANGED: {
            m_dpi = HIWORD(wp);
            CreateFonts();
            SendMessageW(m_nav, WM_SETFONT, reinterpret_cast<WPARAM>(m_navFont), TRUE);
            SendMessageW(m_nav, LB_SETITEMHEIGHT, 0, Px(38));
            for (HWND b : {m_settingsButton, m_guideButton, m_issuesButton}) SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(m_textFont), TRUE);
            auto* r = reinterpret_cast<RECT*>(lp);
            SetWindowPos(m_hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == kNavId && HIWORD(wp) == LBN_SELCHANGE) {
                SelectPage(static_cast<int>(SendMessageW(m_nav, LB_GETCURSEL, 0, 0)));
                return 0;
            }
            break;
        case WM_DRAWITEM:
            DrawNavItem(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
            return TRUE;
        case WM_CLOSE:
            ShowWindow(m_hwnd, SW_HIDE);
            return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

LRESULT CALLBACK WelcomeWindow::PageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
    auto* self = reinterpret_cast<WelcomeWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self && self->m_page ? self->OnPageMessage(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT WelcomeWindow::OnPageMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(m_page, &ps);
            RECT rc;
            GetClientRect(m_page, &rc);
            // Double-buffered to avoid flicker while scrolling and resizing.
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, std::max<LONG>(1, rc.right), std::max<LONG>(1, rc.bottom));
            HGDIOBJ old = SelectObject(mem, bmp);
            PaintPage(mem);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(m_page, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_COMMAND: {
            const WelcomePage& page = WelcomePages()[static_cast<size_t>(m_index)];
            if (LOWORD(wp) == kSettingsButtonId) Controller::Get().OpenSettings(page.settingsPage);
            else if (LOWORD(wp) == kGuideButtonId) OpenUserGuide(m_hwnd, page.guideAnchor);
            else if (LOWORD(wp) == kIssuesButtonId) OpenIssuesPage(m_hwnd);
            return 0;
        }
        case WM_VSCROLL: {
            SCROLLINFO si{sizeof(si), SIF_ALL};
            GetScrollInfo(m_page, SB_VERT, &si);
            int pos = m_scroll;
            switch (LOWORD(wp)) {
                case SB_LINEUP: pos -= Px(40); break;
                case SB_LINEDOWN: pos += Px(40); break;
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
            ScrollTo(m_scroll - GET_WHEEL_DELTA_WPARAM(wp) * Px(60) / WHEEL_DELTA);
            return 0;
        case WM_SIZE:
            LayoutPage();
            return 0;
    }
    return DefWindowProcW(m_page, msg, wp, lp);
}

}  // namespace of
