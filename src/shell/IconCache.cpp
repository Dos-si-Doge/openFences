// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "shell/IconCache.h"

#include <shobjidl.h>

#include <algorithm>

namespace of {

std::shared_ptr<IconPixels> PixelsFromHBitmap(HBITMAP bmp) {
    BITMAP bm{};
    if (!bmp || !GetObjectW(bmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight == 0) return nullptr;
    auto px = std::make_shared<IconPixels>();
    px->width = bm.bmWidth;
    px->height = std::abs(bm.bmHeight);
    px->bgra.resize(static_cast<size_t>(px->width) * px->height);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = px->width;
    bi.bmiHeader.biHeight = -px->height;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC dc = GetDC(nullptr);
    int lines = GetDIBits(dc, bmp, 0, px->height, px->bgra.data(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (lines == 0) return nullptr;

    bool anyAlpha = false, straight = false;
    for (uint32_t p : px->bgra) {
        uint32_t a = p >> 24;
        if (a) anyAlpha = true;
        if (((p >> 16) & 0xFF) > a || ((p >> 8) & 0xFF) > a || (p & 0xFF) > a) straight = true;
    }
    for (uint32_t& p : px->bgra) {
        if (!anyAlpha) {
            p |= 0xFF000000u;  // thumbnails without an alpha channel are opaque
        } else if (straight) {
            uint32_t a = p >> 24;
            uint32_t r = ((p >> 16) & 0xFF) * a / 255, g = ((p >> 8) & 0xFF) * a / 255, b = (p & 0xFF) * a / 255;
            p = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    return px;
}

HBITMAP HBitmapFromPixels(const IconPixels& px) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = px.width;
    bi.bmiHeader.biHeight = -px.height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) memcpy(bits, px.bgra.data(), px.bgra.size() * 4);
    return bmp;
}

namespace {

std::vector<uint32_t> ApplyVariant(const IconPixels& src, const IconVariant& v) {
    std::vector<uint32_t> out = src.bgra;
    if (!v.tint && !v.desaturate) return out;
    for (uint32_t& p : out) {
        uint32_t a = p >> 24;
        if (!a) continue;
        uint32_t r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
        uint32_t lum = (r * 299 + g * 587 + b * 114) / 1000;
        if (v.tint) {
            // Multiply luminance by the tint color, lifted so dark icons still show the tint.
            uint32_t l = std::min<uint32_t>(a, lum / 2 + a / 2);
            r = l * v.tint->r / 255;
            g = l * v.tint->g / 255;
            b = l * v.tint->b / 255;
        } else {
            r = g = b = lum;
        }
        p = (a << 24) | (r << 16) | (g << 8) | b;
    }
    return out;
}

}  // namespace

std::wstring IconCache::Key(const ShellItem& item, int sizePx) {
    return item.parsingName + L"|" + std::to_wstring(sizePx);
}

void IconCache::Request(const ShellItem& item, int sizePx, Entry& entry) {
    entry.loading = true;
    std::wstring key = Key(item, sizePx);
    SharedPidl pidl = item.pidl;
    uint64_t epoch = m_epoch;
    m_worker.Post([this, key, pidl, sizePx, epoch] {
        std::shared_ptr<IconPixels> px;
        ComPtr<IShellItemImageFactory> factory;
        if (SUCCEEDED(SHCreateItemFromIDList(pidl.get(), IID_PPV_ARGS(&factory)))) {
            HBITMAP bmp = nullptr;
            HRESULT hr = factory->GetImage(SIZE{sizePx, sizePx}, SIIGBF_RESIZETOFIT, &bmp);
            if (FAILED(hr)) hr = factory->GetImage(SIZE{sizePx, sizePx}, SIIGBF_ICONONLY, &bmp);
            if (SUCCEEDED(hr) && bmp) {
                px = PixelsFromHBitmap(bmp);
                DeleteObject(bmp);
            }
        }
        PostToUi([this, key, px, epoch] {
            if (epoch != m_epoch) return;
            auto it = m_entries.find(key);
            if (it == m_entries.end()) return;
            it->second.loading = false;
            if (!px && it->second.pixels && it->second.pixels->width > 0) return;  // keep the old icon on a failed reload
            it->second.pixels = px ? px : std::make_shared<IconPixels>();
            it->second.bitmaps.clear();
            if (onLoaded) onLoaded();
        });
    });
}

ID2D1Bitmap* IconCache::Get(const ShellItem& item, int sizePx, const IconVariant& variant) {
    if (m_generation != Gfx().Generation()) {
        for (auto& [k, e] : m_entries) e.bitmaps.clear();
        m_generation = Gfx().Generation();
    }
    Entry& entry = m_entries[Key(item, sizePx)];
    if (!entry.loading && (!entry.pixels || entry.modified != item.modified || entry.stale)) {
        entry.modified = item.modified;
        entry.stale = false;
        Request(item, sizePx, entry);
    }
    if (!entry.pixels || entry.pixels->width == 0) return nullptr;
    auto& bmp = entry.bitmaps[variant];
    if (!bmp) {
        std::vector<uint32_t> data = ApplyVariant(*entry.pixels, variant);
        D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        Gfx().ResourceContext()->CreateBitmap(D2D1::SizeU(entry.pixels->width, entry.pixels->height), data.data(),
                                              static_cast<UINT32>(entry.pixels->width * 4), &props, &bmp);
    }
    return bmp.Get();
}

std::shared_ptr<const IconPixels> IconCache::Pixels(const ShellItem& item, int sizePx) {
    auto it = m_entries.find(Key(item, sizePx));
    if (it == m_entries.end()) return nullptr;
    return it->second.pixels;
}

void IconCache::Invalidate(const std::wstring& parsingName) {
    std::wstring prefix = parsingName + L"|";
    for (auto& [key, entry] : m_entries)
        if (key.rfind(prefix, 0) == 0) entry.stale = true;
}

void IconCache::MarkAllStale() {
    for (auto& [key, entry] : m_entries) entry.stale = true;
}

void IconCache::Clear() {
    m_entries.clear();
    ++m_epoch;
}

}  // namespace of
