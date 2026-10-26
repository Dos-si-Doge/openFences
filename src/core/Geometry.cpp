// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "core/Geometry.h"

#include <cmath>
#include <cstdlib>

namespace of {

RectI Normalized(PointI a, PointI b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
}

RectI Intersect(const RectI& a, const RectI& b) {
    RectI r{std::max(a.left, b.left), std::max(a.top, b.top), std::min(a.right, b.right), std::min(a.bottom, b.bottom)};
    if (r.right < r.left) r.right = r.left;
    if (r.bottom < r.top) r.bottom = r.top;
    return r;
}

RectI ClampInside(RectI r, const RectI& bounds) {
    int w = std::min(r.Width(), bounds.Width());
    int h = std::min(r.Height(), bounds.Height());
    int x = std::clamp(r.left, bounds.left, bounds.right - w);
    int y = std::clamp(r.top, bounds.top, bounds.bottom - h);
    return RectI::FromXYWH(x, y, w, h);
}

RectF NormalizeRect(const RectI& r, const RectI& work) {
    double ww = std::max(1, work.Width());
    double wh = std::max(1, work.Height());
    return {(r.left - work.left) / ww, (r.top - work.top) / wh, r.Width() / ww, r.Height() / wh};
}

RectI DenormalizeRect(const RectF& r, const RectI& work) {
    auto rnd = [](double v) { return static_cast<int>(std::lround(v)); };
    int x = work.left + rnd(r.x * work.Width());
    int y = work.top + rnd(r.y * work.Height());
    int w = rnd(r.w * work.Width());
    int h = rnd(r.h * work.Height());
    return RectI::FromXYWH(x, y, w, h);
}

namespace {

// Returns the best adjustment (smallest |delta|) moving `value` onto one of `targets`, if within threshold.
std::optional<int> BestDelta(int value, const std::vector<int>& targets, int threshold) {
    std::optional<int> best;
    for (int t : targets) {
        int d = t - value;
        if (std::abs(d) <= threshold && (!best || std::abs(d) < std::abs(*best))) best = d;
    }
    return best;
}

struct SnapTargets {
    std::vector<int> xs;  // candidate x positions for a left or right edge
    std::vector<int> ys;
};

// Candidate positions for the left/top edges (leading) and right/bottom edges (trailing).
void CollectTargets(const RectI& self, const std::vector<RectI>& others, const RectI& work, const SnapOptions& opt,
                    SnapTargets& leading, SnapTargets& trailing) {
    if (opt.toEdges) {
        leading.xs.push_back(work.left);
        leading.ys.push_back(work.top);
        trailing.xs.push_back(work.right);
        trailing.ys.push_back(work.bottom);
    }
    if (opt.toOthers) {
        for (const RectI& o : others) {
            bool vOverlap = self.top < o.bottom + opt.threshold && o.top < self.bottom + opt.threshold;
            bool hOverlap = self.left < o.right + opt.threshold && o.left < self.right + opt.threshold;
            if (vOverlap) {
                // Side by side: sit next to o with spacing, and align top/bottom edges.
                leading.xs.push_back(o.right + opt.spacing);
                trailing.xs.push_back(o.left - opt.spacing);
                leading.ys.push_back(o.top);
                trailing.ys.push_back(o.bottom);
            }
            if (hOverlap) {
                // Stacked: sit above/below o with spacing, and align left/right edges.
                leading.ys.push_back(o.bottom + opt.spacing);
                trailing.ys.push_back(o.top - opt.spacing);
                leading.xs.push_back(o.left);
                trailing.xs.push_back(o.right);
            }
        }
    }
}

int SnapToGrid(int v, int origin, int grid) {
    if (grid <= 1) return v;
    double steps = std::round(static_cast<double>(v - origin) / grid);
    return origin + static_cast<int>(steps) * grid;
}

}  // namespace

RectI SnapMove(const RectI& proposed, const std::vector<RectI>& others, const RectI& work, const SnapOptions& opt) {
    RectI r = proposed;
    SnapTargets lead, trail;
    CollectTargets(r, others, work, opt, lead, trail);

    auto dxL = BestDelta(r.left, lead.xs, opt.threshold);
    auto dxR = BestDelta(r.right, trail.xs, opt.threshold);
    std::optional<int> dx;
    if (dxL && dxR) dx = std::abs(*dxL) <= std::abs(*dxR) ? dxL : dxR;
    else dx = dxL ? dxL : dxR;

    auto dyT = BestDelta(r.top, lead.ys, opt.threshold);
    auto dyB = BestDelta(r.bottom, trail.ys, opt.threshold);
    std::optional<int> dy;
    if (dyT && dyB) dy = std::abs(*dyT) <= std::abs(*dyB) ? dyT : dyB;
    else dy = dyT ? dyT : dyB;

    if (opt.toGrid) {
        if (!dx) dx = SnapToGrid(r.left, work.left, opt.gridSize) - r.left;
        if (!dy) dy = SnapToGrid(r.top, work.top, opt.gridSize) - r.top;
    }
    return r.Offset(dx.value_or(0), dy.value_or(0));
}

RectI SnapResize(const RectI& proposed, unsigned edgeMask, const std::vector<RectI>& others, const RectI& work,
                 const SnapOptions& opt, int minW, int minH) {
    RectI r = proposed;
    SnapTargets lead, trail;
    CollectTargets(r, others, work, opt, lead, trail);
    auto apply = [&](int& v, const std::vector<int>& targets, int origin) {
        if (auto d = BestDelta(v, targets, opt.threshold)) {
            v += *d;
        } else if (opt.toGrid) {
            v = SnapToGrid(v, origin, opt.gridSize);
        }
    };
    if (edgeMask & EdgeLeft) apply(r.left, lead.xs, work.left);
    if (edgeMask & EdgeRight) apply(r.right, trail.xs, work.left);
    if (edgeMask & EdgeTop) apply(r.top, lead.ys, work.top);
    if (edgeMask & EdgeBottom) apply(r.bottom, trail.ys, work.top);
    if (r.Width() < minW) {
        if (edgeMask & EdgeLeft) r.left = r.right - minW;
        else r.right = r.left + minW;
    }
    if (r.Height() < minH) {
        if (edgeMask & EdgeTop) r.top = r.bottom - minH;
        else r.bottom = r.top + minH;
    }
    return r;
}

RectI FindFreePlacement(int w, int h, const std::vector<RectI>& others, const RectI& work, int spacing) {
    int step = std::max(8, spacing);
    for (int x = work.left + spacing; x + w <= work.right - spacing; x += step) {
        for (int y = work.top + spacing; y + h <= work.bottom - spacing; y += step) {
            RectI cand = RectI::FromXYWH(x, y, w, h);
            RectI padded = cand.Inflate(spacing - 1);
            bool free = true;
            for (const RectI& o : others) {
                if (padded.Intersects(o)) {
                    free = false;
                    // Jump below the blocker to speed up the scan.
                    y = std::max(y, o.bottom + spacing - step);
                    break;
                }
            }
            if (free) return cand;
        }
    }
    // No free spot: cascade from the top-left.
    int n = static_cast<int>(others.size()) % 10;
    return ClampInside(RectI::FromXYWH(work.left + spacing + n * 24, work.top + spacing + n * 24, w, h), work);
}

unsigned EdgesTouched(const RectI& r, const RectI& work, int tolerance) {
    unsigned e = EdgeNone;
    if (std::abs(r.left - work.left) <= tolerance) e |= EdgeLeft;
    if (std::abs(r.top - work.top) <= tolerance) e |= EdgeTop;
    if (std::abs(r.right - work.right) <= tolerance) e |= EdgeRight;
    if (std::abs(r.bottom - work.bottom) <= tolerance) e |= EdgeBottom;
    return e;
}

GridLayout ComputeGrid(int viewW, int viewH, int count, const GridSpec& spec, GridFlow flow) {
    GridLayout g;
    g.spec = spec;
    g.flow = flow;
    int cw = std::max(1, spec.cellW);
    int ch = std::max(1, spec.cellH);
    int usableW = std::max(0, viewW - 2 * spec.padding);
    int usableH = std::max(0, viewH - 2 * spec.padding);
    count = std::max(0, count);
    if (flow == GridFlow::RowsThenScrollY) {
        g.columns = std::max(1, usableW / cw);
        g.rows = (count + g.columns - 1) / g.columns;
        g.contentW = viewW;
        g.contentH = g.rows * ch + 2 * spec.padding;
    } else {
        g.rows = std::max(1, usableH / ch);
        g.columns = (count + g.rows - 1) / g.rows;
        g.contentH = viewH;
        g.contentW = g.columns * cw + 2 * spec.padding;
    }
    return g;
}

RectI GridLayout::CellRect(int index) const {
    int col, row;
    if (flow == GridFlow::RowsThenScrollY) {
        col = index % columns;
        row = index / columns;
    } else {
        row = index % rows;
        col = index / rows;
    }
    return RectI::FromXYWH(spec.padding + col * spec.cellW, spec.padding + row * spec.cellH, spec.cellW, spec.cellH);
}

int GridLayout::IndexAt(PointI p, int count) const {
    int x = p.x - spec.padding;
    int y = p.y - spec.padding;
    if (x < 0 || y < 0) return -1;
    int col = x / std::max(1, spec.cellW);
    int row = y / std::max(1, spec.cellH);
    int index;
    if (flow == GridFlow::RowsThenScrollY) {
        if (col >= columns) return -1;
        index = row * columns + col;
    } else {
        if (row >= rows) return -1;
        index = col * rows + row;
    }
    return index < count ? index : -1;
}

int ClampScroll(int scroll, int content, int view) {
    return std::clamp(scroll, 0, std::max(0, content - view));
}

}  // namespace of
