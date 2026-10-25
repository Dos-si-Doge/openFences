#include "render/Painter.h"

#include <cmath>
#include <map>
#include <tuple>

namespace of {

namespace {
using FormatKey = std::tuple<int, bool, int, int, bool>;
std::map<FormatKey, ComPtr<IDWriteTextFormat>>& FormatCache() {
    static std::map<FormatKey, ComPtr<IDWriteTextFormat>> cache;
    return cache;
}
}  // namespace

IDWriteTextFormat* GetTextFormat(const TextStyle& style) {
    bool wrap = style.maxLines > 1;
    FormatKey key{static_cast<int>(std::lround(style.sizePx * 10)), style.bold, static_cast<int>(style.h), static_cast<int>(style.v), wrap};
    auto& cache = FormatCache();
    if (auto it = cache.find(key); it != cache.end()) return it->second.Get();
    ComPtr<IDWriteTextFormat> fmt;
    Gfx().DWrite()->CreateTextFormat(L"Segoe UI", nullptr, style.bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                     DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, style.sizePx, L"", &fmt);
    if (!fmt) return nullptr;
    fmt->SetTextAlignment(style.h == HAlign::Left ? DWRITE_TEXT_ALIGNMENT_LEADING
                          : style.h == HAlign::Center ? DWRITE_TEXT_ALIGNMENT_CENTER
                                                      : DWRITE_TEXT_ALIGNMENT_TRAILING);
    fmt->SetParagraphAlignment(style.v == VAlign::Top ? DWRITE_PARAGRAPH_ALIGNMENT_NEAR
                               : style.v == VAlign::Middle ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER
                                                           : DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    fmt->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_EMERGENCY_BREAK : DWRITE_WORD_WRAPPING_NO_WRAP);
    ComPtr<IDWriteInlineObject> ellipsis;
    Gfx().DWrite()->CreateEllipsisTrimmingSign(fmt.Get(), &ellipsis);
    DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    fmt->SetTrimming(&trim, ellipsis.Get());
    cache[key] = fmt;
    return fmt.Get();
}

Painter::Painter(ID2D1DeviceContext* dc) : m_dc(dc) {
    dc->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), &m_brush);
    dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

ID2D1SolidColorBrush* Painter::Brush(const Color& c, float opacity) {
    m_brush->SetColor(ToD2D(c, opacity));
    return m_brush.Get();
}

void Painter::FillRect(const RectI& r, const Color& c, float opacity) {
    m_dc->FillRectangle(ToD2D(r), Brush(c, opacity));
}

void Painter::FillRoundRect(const RectI& r, float radius, const Color& c, float opacity) {
    if (radius <= 0.5f) return FillRect(r, c, opacity);
    m_dc->FillRoundedRectangle(D2D1::RoundedRect(ToD2D(r), radius, radius), Brush(c, opacity));
}

void Painter::FillTopRoundRect(const RectI& r, float radius, const Color& c, float opacity) {
    if (radius <= 0.5f) return FillRect(r, c, opacity);
    ComPtr<ID2D1PathGeometry> path;
    Gfx().D2DFactory()->CreatePathGeometry(&path);
    ComPtr<ID2D1GeometrySink> sink;
    path->Open(&sink);
    D2D1_RECT_F f = ToD2D(r);
    float rr = std::min(radius, (f.bottom - f.top));
    sink->BeginFigure(D2D1::Point2F(f.left, f.bottom), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(f.left, f.top + rr));
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(f.left + rr, f.top), D2D1::SizeF(rr, rr), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->AddLine(D2D1::Point2F(f.right - rr, f.top));
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(f.right, f.top + rr), D2D1::SizeF(rr, rr), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->AddLine(D2D1::Point2F(f.right, f.bottom));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    m_dc->FillGeometry(path.Get(), Brush(c, opacity));
}

void Painter::StrokeRoundRect(const RectI& r, float radius, const Color& c, float width, float opacity) {
    D2D1_RECT_F f = ToD2D(r);
    float h = width / 2;
    f = D2D1::RectF(f.left + h, f.top + h, f.right - h, f.bottom - h);
    m_dc->DrawRoundedRectangle(D2D1::RoundedRect(f, radius, radius), Brush(c, opacity), width);
}

void Painter::Line(PointI a, PointI b, const Color& c, float width, float opacity) {
    m_dc->DrawLine(D2D1::Point2F(static_cast<float>(a.x), static_cast<float>(a.y)), D2D1::Point2F(static_cast<float>(b.x), static_cast<float>(b.y)),
                   Brush(c, opacity), width);
}

void Painter::Text(std::wstring_view text, const RectI& r, const Color& c, const TextStyle& style, float opacity) {
    IDWriteTextFormat* fmt = GetTextFormat(style);
    if (!fmt || text.empty() || r.Empty()) return;
    float maxH = static_cast<float>(r.Height());
    if (style.maxLines > 1) maxH = std::min(maxH, style.sizePx * 1.36f * style.maxLines);
    ComPtr<IDWriteTextLayout> layout;
    Gfx().DWrite()->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), fmt, static_cast<float>(r.Width()), maxH, &layout);
    if (!layout) return;
    float top = static_cast<float>(r.top);
    if (style.maxLines > 1 && style.v == VAlign::Top) {
        // keep top alignment
    } else if (style.maxLines > 1) {
        top += (r.Height() - maxH) / 2;
    }
    D2D1_POINT_2F origin = D2D1::Point2F(static_cast<float>(r.left), top);
    if (style.shadow) {
        Color shadow{0, 0, 0, 200};
        for (auto [dx, dy] : {std::pair{1.f, 1.f}, {0.f, 1.f}, {1.f, 0.f}, {-0.6f, 0.6f}})
            m_dc->DrawTextLayout(D2D1::Point2F(origin.x + dx, origin.y + dy), layout.Get(), Brush(shadow, opacity * 0.55f));
    }
    m_dc->DrawTextLayout(origin, layout.Get(), Brush(c, opacity));
}

void Painter::Bitmap(ID2D1Bitmap* bmp, const RectI& r, float opacity) {
    if (!bmp) return;
    m_dc->DrawBitmap(bmp, ToD2D(r), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

void Painter::PushClip(const RectI& r) { m_dc->PushAxisAlignedClip(ToD2D(r), D2D1_ANTIALIAS_MODE_ALIASED); }
void Painter::PopClip() { m_dc->PopAxisAlignedClip(); }

float Painter::MeasureText(std::wstring_view text, const TextStyle& style) {
    TextStyle s = style;
    s.maxLines = 1;
    IDWriteTextFormat* fmt = GetTextFormat(s);
    if (!fmt) return 0;
    ComPtr<IDWriteTextLayout> layout;
    Gfx().DWrite()->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), fmt, 100000.f, 1000.f, &layout);
    DWRITE_TEXT_METRICS m{};
    if (layout) layout->GetMetrics(&m);
    return m.widthIncludingTrailingWhitespace;
}

}  // namespace of
