// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "shell/Wallpaper.h"

#include <algorithm>

namespace of {

WallpaperInfo GetWallpaperFor(const RectI& bounds) {
    WallpaperInfo info;
    ComPtr<IDesktopWallpaper> dw;
    if (FAILED(CoCreateInstance(CLSID_DesktopWallpaper, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dw)))) return info;
    COLORREF bg = 0;
    if (SUCCEEDED(dw->GetBackgroundColor(&bg))) info.background = Color::FromColorRef(bg, 255);
    dw->GetPosition(&info.position);
    UINT count = 0;
    dw->GetMonitorDevicePathCount(&count);
    std::wstring fallback;
    for (UINT i = 0; i < count; ++i) {
        PWSTR id = nullptr;
        if (FAILED(dw->GetMonitorDevicePathAt(i, &id))) continue;
        RECT r{};
        PWSTR path = nullptr;
        if (SUCCEEDED(dw->GetMonitorRECT(id, &r)) && SUCCEEDED(dw->GetWallpaper(id, &path)) && path) {
            if (fallback.empty()) fallback = path;
            if (r.left == bounds.left && r.top == bounds.top && r.right == bounds.right && r.bottom == bounds.bottom) info.path = path;
        }
        CoTaskMemFree(path);
        CoTaskMemFree(id);
        if (!info.path.empty()) break;
    }
    if (info.path.empty()) info.path = fallback;
    return info;
}

namespace {
ComPtr<IWICBitmapSource> DecodeScaled(const std::wstring& path, UINT maxW, UINT maxH) {
    IWICImagingFactory* wic = Gfx().Wic();
    ComPtr<IWICBitmapDecoder> decoder;
    if (path.empty() || FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
        return nullptr;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return nullptr;
    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);
    ComPtr<IWICBitmapSource> source = frame;
    // Scale by the larger ratio so "fill" cropping still has enough resolution.
    double scale = std::min(1.0, std::max(static_cast<double>(maxW) / std::max(1u, w), static_cast<double>(maxH) / std::max(1u, h)));
    if (scale < 1.0) {
        ComPtr<IWICBitmapScaler> scaler;
        if (SUCCEEDED(wic->CreateBitmapScaler(&scaler)) &&
            SUCCEEDED(scaler->Initialize(frame.Get(), std::max(1u, static_cast<UINT>(w * scale)), std::max(1u, static_cast<UINT>(h * scale)),
                                         WICBitmapInterpolationModeFant)))
            source = scaler;
    }
    ComPtr<IWICFormatConverter> conv;
    if (FAILED(wic->CreateFormatConverter(&conv)) ||
        FAILED(conv->Initialize(source.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
        return nullptr;
    return conv;
}
}  // namespace

ComPtr<ID2D1Bitmap> LoadWallpaperBitmap(const std::wstring& path, UINT maxW, UINT maxH) {
    ComPtr<ID2D1Bitmap> bmp;
    if (auto src = DecodeScaled(path, maxW, maxH)) Gfx().ResourceContext()->CreateBitmapFromWicBitmap(src.Get(), nullptr, &bmp);
    return bmp;
}

std::optional<Color> AverageImageColor(const std::wstring& path) {
    auto src = DecodeScaled(path, 32, 32);
    if (!src) return std::nullopt;
    UINT w = 0, h = 0;
    src->GetSize(&w, &h);
    std::vector<uint32_t> px(static_cast<size_t>(w) * h);
    if (FAILED(src->CopyPixels(nullptr, w * 4, static_cast<UINT>(px.size() * 4), reinterpret_cast<BYTE*>(px.data())))) return std::nullopt;
    uint64_t r = 0, g = 0, b = 0;
    for (uint32_t p : px) {
        r += (p >> 16) & 0xFF;
        g += (p >> 8) & 0xFF;
        b += p & 0xFF;
    }
    size_t n = std::max<size_t>(1, px.size());
    return Color{static_cast<uint8_t>(r / n), static_cast<uint8_t>(g / n), static_cast<uint8_t>(b / n), 255};
}

void DrawWallpaper(Painter& p, ID2D1Bitmap* bmp, DESKTOP_WALLPAPER_POSITION pos, const RectI& dest, const Color& background) {
    p.FillRect(dest, background);
    if (!bmp) return;
    D2D1_SIZE_F s = bmp->GetSize();
    float dw = static_cast<float>(dest.Width()), dh = static_cast<float>(dest.Height());
    D2D1_RECT_F target = ToD2D(dest);
    switch (pos) {
        case DWPOS_STRETCH: break;
        case DWPOS_CENTER:
            target = D2D1::RectF(dest.left + (dw - s.width) / 2, dest.top + (dh - s.height) / 2, dest.left + (dw + s.width) / 2,
                                 dest.top + (dh + s.height) / 2);
            break;
        case DWPOS_TILE: {
            p.PushClip(dest);
            for (float y = static_cast<float>(dest.top); y < dest.bottom; y += s.height)
                for (float x = static_cast<float>(dest.left); x < dest.right; x += s.width)
                    p.DC()->DrawBitmap(bmp, D2D1::RectF(x, y, x + s.width, y + s.height));
            p.PopClip();
            return;
        }
        case DWPOS_FIT: {
            float k = std::min(dw / s.width, dh / s.height);
            float w = s.width * k, h = s.height * k;
            target = D2D1::RectF(dest.left + (dw - w) / 2, dest.top + (dh - h) / 2, dest.left + (dw + w) / 2, dest.top + (dh + h) / 2);
            break;
        }
        default: {  // fill / span
            float k = std::max(dw / s.width, dh / s.height);
            float w = s.width * k, h = s.height * k;
            target = D2D1::RectF(dest.left + (dw - w) / 2, dest.top + (dh - h) / 2, dest.left + (dw + w) / 2, dest.top + (dh + h) / 2);
            break;
        }
    }
    p.PushClip(dest);
    p.DC()->DrawBitmap(bmp, target, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    p.PopClip();
}

}  // namespace of
