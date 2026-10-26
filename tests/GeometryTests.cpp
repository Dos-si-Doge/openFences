// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include "core/Geometry.h"

using namespace of;

TEST(Geometry, NormalizeRoundTrip) {
    RectI work{0, 0, 1920, 1040};
    RectI r = RectI::FromXYWH(100, 200, 300, 400);
    EXPECT_EQ(DenormalizeRect(NormalizeRect(r, work), work), r);
    // Same fractions on a different resolution scale proportionally.
    RectI big{1920, 0, 1920 + 3840, 2080};
    RectI scaled = DenormalizeRect(NormalizeRect(r, work), big);
    EXPECT_EQ(scaled.left, 1920 + 200);
    EXPECT_EQ(scaled.Width(), 600);
}

TEST(Geometry, ClampInside) {
    RectI work{0, 0, 1000, 800};
    EXPECT_EQ(ClampInside(RectI::FromXYWH(950, -20, 100, 100), work), RectI::FromXYWH(900, 0, 100, 100));
    EXPECT_EQ(ClampInside(RectI::FromXYWH(0, 0, 2000, 100), work).Width(), 1000);
}

TEST(Geometry, SnapMoveToScreenEdge) {
    RectI work{0, 0, 1000, 800};
    SnapOptions opt;
    RectI r = SnapMove(RectI::FromXYWH(7, 300, 200, 200), {}, work, opt);
    EXPECT_EQ(r.left, 0);
    EXPECT_EQ(r.top, 300) << "y unchanged when no target is near";
    r = SnapMove(RectI::FromXYWH(795, 594, 200, 200), {}, work, opt);
    EXPECT_EQ(r.right, 1000);
    EXPECT_EQ(r.bottom, 800);
}

TEST(Geometry, SnapMoveNextToOtherFenceWithSpacing) {
    RectI work{0, 0, 1000, 800};
    SnapOptions opt;
    opt.spacing = 10;
    RectI other = RectI::FromXYWH(100, 100, 200, 200);  // right edge at 300
    RectI r = SnapMove(RectI::FromXYWH(314, 107, 150, 150), {other}, work, opt);
    EXPECT_EQ(r.left, 310);
    EXPECT_EQ(r.top, 100) << "top edges align";
}

TEST(Geometry, SnapDisabled) {
    RectI work{0, 0, 1000, 800};
    SnapOptions opt;
    opt.toEdges = opt.toOthers = false;
    RectI in = RectI::FromXYWH(5, 5, 100, 100);
    EXPECT_EQ(SnapMove(in, {}, work, opt), in);
}

TEST(Geometry, SnapToGrid) {
    RectI work{0, 0, 1000, 800};
    SnapOptions opt;
    opt.toEdges = opt.toOthers = false;
    opt.toGrid = true;
    opt.gridSize = 20;
    RectI r = SnapMove(RectI::FromXYWH(109, 131, 100, 100), {}, work, opt);
    EXPECT_EQ(r.left, 100);
    EXPECT_EQ(r.top, 140);
}

TEST(Geometry, SnapResizeRespectsMinimum) {
    RectI work{0, 0, 1000, 800};
    SnapOptions opt;
    RectI r = SnapResize(RectI::FromXYWH(100, 100, 20, 20), EdgeRight | EdgeBottom, {}, work, opt, 120, 80);
    EXPECT_EQ(r.Width(), 120);
    EXPECT_EQ(r.Height(), 80);
    r = SnapResize(RectI{100, 100, 994, 300}, EdgeRight, {}, work, opt, 50, 50);
    EXPECT_EQ(r.right, 1000);
}

TEST(Geometry, FindFreePlacementAvoidsOverlap) {
    RectI work{0, 0, 1000, 800};
    std::vector<RectI> others{RectI::FromXYWH(10, 10, 300, 300)};
    RectI r = FindFreePlacement(200, 200, others, work, 10);
    for (const auto& o : others) EXPECT_FALSE(r.Intersects(o));
    EXPECT_GE(r.left, work.left);
    EXPECT_LE(r.right, work.right);
}

TEST(Geometry, EdgesTouched) {
    RectI work{0, 0, 1000, 800};
    EXPECT_EQ(EdgesTouched(RectI{0, 600, 200, 800}, work, 2), EdgeLeft | EdgeBottom);
    EXPECT_EQ(EdgesTouched(RectI{50, 50, 200, 200}, work, 2), EdgeNone);
}

TEST(Geometry, GridRowsLayout) {
    GridSpec spec{80, 90, 5};
    GridLayout g = ComputeGrid(250, 200, 7, spec, GridFlow::RowsThenScrollY);
    EXPECT_EQ(g.columns, 3);
    EXPECT_EQ(g.rows, 3);
    EXPECT_EQ(g.contentH, 3 * 90 + 10);
    EXPECT_EQ(g.CellRect(4), RectI::FromXYWH(5 + 80, 5 + 90, 80, 90));
    EXPECT_EQ(g.IndexAt({5 + 80 + 1, 5 + 90 + 1}, 7), 4);
    EXPECT_EQ(g.IndexAt({5 + 160 + 1, 5 + 180 + 1}, 7), -1) << "past the last item";
}

TEST(Geometry, GridColumnsLayout) {
    GridSpec spec{80, 90, 0};
    GridLayout g = ComputeGrid(500, 270, 7, spec, GridFlow::ColumnsThenScrollX);
    EXPECT_EQ(g.rows, 3);
    EXPECT_EQ(g.columns, 3);
    EXPECT_EQ(g.CellRect(4), RectI::FromXYWH(80, 90, 80, 90));
    EXPECT_EQ(g.contentW, 240);
}

TEST(Geometry, ClampScroll) {
    EXPECT_EQ(ClampScroll(-5, 500, 200), 0);
    EXPECT_EQ(ClampScroll(400, 500, 200), 300);
    EXPECT_EQ(ClampScroll(50, 100, 200), 0);
}
