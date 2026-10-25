#pragma once

#include <algorithm>
#include <optional>
#include <vector>

namespace of {

struct PointI {
    int x = 0, y = 0;
    bool operator==(const PointI&) const = default;
};

struct RectI {
    int left = 0, top = 0, right = 0, bottom = 0;

    int Width() const { return right - left; }
    int Height() const { return bottom - top; }
    bool Empty() const { return Width() <= 0 || Height() <= 0; }
    bool Contains(PointI p) const { return p.x >= left && p.x < right && p.y >= top && p.y < bottom; }
    bool Intersects(const RectI& o) const { return left < o.right && o.left < right && top < o.bottom && o.top < bottom; }
    RectI Offset(int dx, int dy) const { return {left + dx, top + dy, right + dx, bottom + dy}; }
    RectI Inflate(int d) const { return {left - d, top - d, right + d, bottom + d}; }
    PointI Center() const { return {(left + right) / 2, (top + bottom) / 2}; }
    static RectI FromXYWH(int x, int y, int w, int h) { return {x, y, x + w, y + h}; }
    bool operator==(const RectI&) const = default;
};

RectI Normalized(PointI a, PointI b);  // rectangle spanned by two corners
RectI Intersect(const RectI& a, const RectI& b);
// Moves r so it lies inside bounds (shrinking only if it is larger than bounds).
RectI ClampInside(RectI r, const RectI& bounds);

// Fractional rectangle relative to a monitor work area (0..1).
struct RectF {
    double x = 0, y = 0, w = 0, h = 0;
    bool operator==(const RectF&) const = default;
};

RectF NormalizeRect(const RectI& r, const RectI& work);
RectI DenormalizeRect(const RectF& r, const RectI& work);

// ---- snapping ----
struct SnapOptions {
    bool toOthers = true;
    bool toEdges = true;
    bool toGrid = false;
    int gridSize = 16;
    int threshold = 10;  // px distance at which snapping engages
    int spacing = 10;    // gap kept between snapped fences
};

enum Edge : unsigned { EdgeNone = 0, EdgeLeft = 1, EdgeTop = 2, EdgeRight = 4, EdgeBottom = 8 };

// Snaps a rectangle that is being moved (size unchanged).
RectI SnapMove(const RectI& proposed, const std::vector<RectI>& others, const RectI& work, const SnapOptions& opt);
// Snaps the edges listed in edgeMask of a rectangle that is being resized.
RectI SnapResize(const RectI& proposed, unsigned edgeMask, const std::vector<RectI>& others, const RectI& work,
                 const SnapOptions& opt, int minW, int minH);
// Finds the first free spot (scanning columns left to right, top to bottom) for a new rectangle.
RectI FindFreePlacement(int w, int h, const std::vector<RectI>& others, const RectI& work, int spacing);
// Which work-area edges the rectangle touches (within tolerance).
unsigned EdgesTouched(const RectI& r, const RectI& work, int tolerance);

// ---- item grid layout ----
enum class GridFlow { RowsThenScrollY, ColumnsThenScrollX };

struct GridSpec {
    int cellW = 80;
    int cellH = 90;
    int padding = 6;
};

struct GridLayout {
    int columns = 1;
    int rows = 1;
    int contentW = 0;
    int contentH = 0;
    GridFlow flow = GridFlow::RowsThenScrollY;
    GridSpec spec;

    RectI CellRect(int index) const;  // in content coordinates
    int IndexAt(PointI contentPt, int count) const;
};

GridLayout ComputeGrid(int viewW, int viewH, int count, const GridSpec& spec, GridFlow flow);
int ClampScroll(int scroll, int content, int view);

}  // namespace of
