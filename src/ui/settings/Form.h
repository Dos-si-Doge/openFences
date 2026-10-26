// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <windows.h>

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/Model.h"

namespace of {

// Builds a vertical column of standard Win32 controls on a parent window, DPI-scaled.
// Rows: headings, wrapped text, checkboxes, "label + control" rows, button rows, lists.
class Form {
public:
    void Reset(HWND parent, HFONT font, HFONT headFont, UINT dpi, int widthPx, int originY = 0);
    int Px(int dip) const { return MulDiv(dip, static_cast<int>(m_dpi), 96); }
    int Height() const { return m_y; }
    HWND Parent() const { return m_parent; }

    HWND Heading(const std::wstring& text);
    HWND Text(const std::wstring& text, bool muted = false);
    HWND Check(int id, const std::wstring& text, bool checked, bool enabled = true);
    HWND Combo(int id, const std::wstring& label, const std::vector<std::wstring>& items, int selected, bool enabled = true);
    HWND Slider(int id, const std::wstring& label, int min, int max, int value, const std::wstring& unit = {});
    HWND Edit(int id, const std::wstring& label, const std::wstring& text, bool enabled = true);
    // Edit that records a key combination when focused (Backspace clears).
    HWND HotkeyEdit(int id, const std::wstring& label, const std::wstring& text, const std::wstring& placeholder);
    HWND ColorButton(int id, const std::wstring& label, const Color& color);
    std::vector<HWND> Buttons(const std::vector<std::pair<int, std::wstring>>& buttons, bool enabled = true);
    HWND List(int id, int heightDip);
    HWND ListView(int id, int heightDip, const std::vector<std::pair<std::wstring, int>>& columns, bool checkboxes);
    HWND OwnerDraw(int id, int heightDip);
    // Owner-drawn area as tall as the form width / aspect (so a picture of that aspect fills it), within min..max DIP.
    HWND OwnerDrawAspect(int id, double aspect, int minDip, int maxDip);
    void Gap(int dip = 8) { m_y += Px(dip); }
    int Width() const { return m_width; }

    // Slider value label (updated by the owner on WM_HSCROLL).
    void UpdateSliderLabel(int id, int value);
    std::optional<Color> ColorOf(int id) const;
    void SetColor(int id, const Color& c);
    // Draws an owner-drawn color button.
    bool DrawColorButton(const DRAWITEMSTRUCT* dis) const;

private:
    HWND Make(const wchar_t* cls, const std::wstring& text, DWORD style, int x, int y, int w, int h, int id, DWORD ex = 0);
    // Wrapped height of text at the given width (m_font when font is null).
    int TextHeight(const std::wstring& text, int width, HFONT font = nullptr) const;
    // Creates the wrapped left-column label of a "label + control" row; returns the row height and the control's top.
    int LabelRow(const std::wstring& label, int controlH, int* controlY);

    HWND m_parent = nullptr;
    HFONT m_font = nullptr;
    HFONT m_head = nullptr;
    UINT m_dpi = 96;
    int m_width = 400;
    int m_y = 0;
    std::map<int, Color> m_colors;
    std::map<int, std::wstring> m_units;
};

// Small modal text prompt. Returns nullopt on Cancel.
std::optional<std::wstring> PromptText(HWND owner, const std::wstring& title, const std::wstring& label, const std::wstring& initial);

// Runs a modal loop for a top-level window until `done` becomes true.
void RunModalLoop(HWND hwnd, const bool& done);

HFONT CreateUiFont(UINT dpi, int pointSize, bool semibold);

}  // namespace of
