// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/Worker.h"
#include "core/Model.h"
#include "render/RenderDevice.h"
#include "shell/ShellItem.h"

namespace of {

struct IconPixels {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> bgra;  // premultiplied, top-down
};

struct IconVariant {
    std::optional<Color> tint;  // Icon Tint
    bool desaturate = false;    // Chameleon
    bool operator<(const IconVariant& o) const {
        auto key = [](const IconVariant& v) { return std::make_tuple(v.tint.has_value(), v.tint ? v.tint->ToColorRef() : 0u, v.desaturate); };
        return key(*this) < key(o);
    }
};

// Loads shell icons/thumbnails on a worker thread and caches them as Direct2D bitmaps (UI thread only).
class IconCache {
public:
    explicit IconCache(Worker& worker) : m_worker(worker) {}

    // Returns the bitmap if loaded; otherwise starts loading and returns nullptr.
    ID2D1Bitmap* Get(const ShellItem& item, int sizePx, const IconVariant& variant = {});
    std::shared_ptr<const IconPixels> Pixels(const ShellItem& item, int sizePx);
    // Reloads the icon in the background; the old bitmap stays visible until the new one arrives.
    void Invalidate(const std::wstring& parsingName);
    void MarkAllStale();
    void Clear();

    // Invoked on the UI thread after one or more icons finish loading.
    std::function<void()> onLoaded;

private:
    struct Entry {
        std::shared_ptr<const IconPixels> pixels;
        std::map<IconVariant, ComPtr<ID2D1Bitmap>> bitmaps;
        uint64_t modified = 0;
        bool loading = false;
        bool stale = false;
    };
    static std::wstring Key(const ShellItem& item, int sizePx);
    void Request(const ShellItem& item, int sizePx, Entry& entry);

    Worker& m_worker;
    std::unordered_map<std::wstring, Entry> m_entries;
    uint32_t m_generation = 0;
    uint64_t m_epoch = 0;  // bumps on Clear() so stale loads are dropped
};

// Converts a shell HBITMAP into premultiplied pixels (exposed for drag images).
std::shared_ptr<IconPixels> PixelsFromHBitmap(HBITMAP bmp);
HBITMAP HBitmapFromPixels(const IconPixels& px);

}  // namespace of
