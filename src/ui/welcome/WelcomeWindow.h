// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace of {

// "Welcome to openFences": built-in documentation modeled on PowerToys' OOBE window. A sidebar lists the topics in
// core/Welcome.cpp; each page shows a hero image, a description, "How to use" rows with key chips, and buttons that
// open the matching Settings page or the section of the external HTML user guide.
class WelcomeWindow {
public:
    WelcomeWindow() = default;
    ~WelcomeWindow();
    WelcomeWindow(const WelcomeWindow&) = delete;
    WelcomeWindow& operator=(const WelcomeWindow&) = delete;

    void Show(const std::wstring& pageId);
    HWND Hwnd() const { return m_hwnd; }
    bool TranslateDialogMessage(MSG* msg);

private:
    struct Row {
        std::vector<std::wstring> chips;
        std::wstring text;
        RECT chipsRect{};
        RECT textRect{};
    };

    static LRESULT CALLBACK FrameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK PageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnFrameMessage(UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnPageMessage(UINT msg, WPARAM wp, LPARAM lp);
    void Create();
    void CreateFonts();
    void LayoutChildren();
    void SelectPage(int index);
    void LayoutPage();
    void PaintPage(HDC dc);
    void DrawNavItem(const DRAWITEMSTRUCT* dis);
    void ScrollTo(int pos);
    void EnsureHero(int width, int height);
    SIZE HeroImageSize(int index);  // native pixel size of a page's hero image (cached; 0x0 if missing)
    int Px(int dip) const { return MulDiv(dip, static_cast<int>(m_dpi), 96); }

    HWND m_hwnd = nullptr;
    HWND m_nav = nullptr;
    HWND m_page = nullptr;
    HWND m_settingsButton = nullptr;
    HWND m_guideButton = nullptr;
    HWND m_issuesButton = nullptr;
    HFONT m_navFont = nullptr;
    HFONT m_titleFont = nullptr;
    HFONT m_headFont = nullptr;
    HFONT m_textFont = nullptr;
    HFONT m_chipFont = nullptr;
    UINT m_dpi = 96;
    int m_index = 0;
    int m_scroll = 0;
    int m_contentHeight = 0;

    // Layout of the current page (page client coordinates, before scrolling).
    RECT m_heroRect{};
    RECT m_titleRect{};
    RECT m_descRect{};
    RECT m_headRect{};
    RECT m_creditsRect{};
    std::vector<Row> m_rows;

    std::vector<SIZE> m_heroSizes;
    // Scaled hero image for the current page and size.
    HBITMAP m_hero = nullptr;
    int m_heroPage = -1;
    int m_heroW = 0;
    int m_heroH = 0;
};

}  // namespace of
