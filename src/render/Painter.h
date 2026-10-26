// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <string_view>

#include "core/Geometry.h"
#include "core/Model.h"
#include "render/RenderDevice.h"

namespace of {

enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Middle, Bottom };

struct TextStyle {
    float sizePx = 12.f;
    bool bold = false;
    HAlign h = HAlign::Left;
    VAlign v = VAlign::Middle;
    int maxLines = 1;     // >1 enables word wrap, trimmed to that many lines
    bool shadow = false;  // dark halo for legibility over wallpaper
};

inline D2D1_RECT_F ToD2D(const RectI& r) {
    return D2D1::RectF(static_cast<float>(r.left), static_cast<float>(r.top), static_cast<float>(r.right), static_cast<float>(r.bottom));
}
inline D2D1_COLOR_F ToD2D(const Color& c, float opacity = 1.f) {
    return D2D1::ColorF(c.r / 255.f, c.g / 255.f, c.b / 255.f, (c.a / 255.f) * opacity);
}

// Thin convenience wrapper over a device context. All coordinates are physical pixels.
class Painter {
public:
    explicit Painter(ID2D1DeviceContext* dc);
    ID2D1DeviceContext* DC() const { return m_dc; }

    void FillRect(const RectI& r, const Color& c, float opacity = 1.f);
    void FillRoundRect(const RectI& r, float radius, const Color& c, float opacity = 1.f);
    // Rounded only on the top corners (title bars).
    void FillTopRoundRect(const RectI& r, float radius, const Color& c, float opacity = 1.f);
    void StrokeRoundRect(const RectI& r, float radius, const Color& c, float width, float opacity = 1.f);
    void Line(PointI a, PointI b, const Color& c, float width, float opacity = 1.f);
    void Text(std::wstring_view text, const RectI& r, const Color& c, const TextStyle& style, float opacity = 1.f);
    void Bitmap(ID2D1Bitmap* bmp, const RectI& r, float opacity = 1.f);
    void PushClip(const RectI& r);
    void PopClip();

    // Measures text width in pixels for a single line.
    static float MeasureText(std::wstring_view text, const TextStyle& style);

private:
    ID2D1SolidColorBrush* Brush(const Color& c, float opacity);

    ID2D1DeviceContext* m_dc;
    ComPtr<ID2D1SolidColorBrush> m_brush;
};

IDWriteTextFormat* GetTextFormat(const TextStyle& style);

}  // namespace of
