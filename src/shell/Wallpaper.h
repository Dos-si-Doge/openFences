#pragma once

#include <shobjidl.h>

#include <optional>
#include <string>

#include "core/Geometry.h"
#include "core/Model.h"
#include "render/Painter.h"

namespace of {

struct WallpaperInfo {
    std::wstring path;
    DESKTOP_WALLPAPER_POSITION position = DWPOS_FILL;
    Color background{0, 0, 0, 255};
};

// Wallpaper for the monitor whose bounds match (falls back to the first monitor).
WallpaperInfo GetWallpaperFor(const RectI& monitorBounds);
// Decodes and downsizes the wallpaper image into a bitmap on the shared resource context.
ComPtr<ID2D1Bitmap> LoadWallpaperBitmap(const std::wstring& path, UINT maxW, UINT maxH);
// Average color of the image (used for wallpaper-aware fence coloring).
std::optional<Color> AverageImageColor(const std::wstring& path);
void DrawWallpaper(Painter& p, ID2D1Bitmap* bmp, DESKTOP_WALLPAPER_POSITION pos, const RectI& dest, const Color& background);

}  // namespace of
