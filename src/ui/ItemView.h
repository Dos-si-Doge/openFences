// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <optional>
#include <vector>

#include "core/Geometry.h"
#include "core/Model.h"
#include "render/Painter.h"
#include "shell/IconCache.h"
#include "shell/ShellItem.h"

namespace of {

struct ItemViewStyle {
    int iconPx = 48;
    ViewMode view = ViewMode::Icons;
    bool freeLayout = false;  // desktop canvas: each item sits at an explicit grid cell
    float scale = 1.f;
    float fontPx = 12.f;
    std::optional<Color> tint;
    bool chameleon = false;
    float chameleonOpacity = 0.35f;
    bool desaturate = true;
    Color text{255, 255, 255, 255};
};

// Lays out, paints and hit-tests a list of shell items (used by fences, portals and the desktop canvas).
class ItemView {
public:
    void SetItems(std::vector<ShellItemPtr> items);
    void SetFreeCells(std::vector<PointI> cells);
    const std::vector<ShellItemPtr>& Items() const { return m_items; }
    int Count() const { return static_cast<int>(m_items.size()); }
    void SetStyle(const ItemViewStyle& style);
    const ItemViewStyle& Style() const { return m_style; }
    void SetViewport(const RectI& r);
    const RectI& Viewport() const { return m_viewport; }

    GridSpec CellSpec() const;
    void Paint(Painter& p, IconCache& icons, float opacity, bool hidden = false, const std::vector<std::wstring>* alwaysVisible = nullptr);

    int HitTest(PointI client) const;
    RectI ItemRect(int index) const;   // client coordinates
    RectI LabelRect(int index) const;  // client coordinates

    // Selection.
    bool IsSelected(int i) const;
    void SelectOnly(int i);
    void Toggle(int i);
    void SelectRange(int from, int to);
    void SelectAll();
    void ClearSelection();
    void SelectInRect(const RectI& client, bool additive);
    std::vector<ShellItemPtr> SelectedItems() const;
    std::vector<int> SelectedIndices() const;
    int SelectedCount() const;
    int IndexOfKey(const std::wstring& key) const;

    int focus = -1;
    int anchor = -1;
    int hover = -1;

    // Scrolling.
    bool Horizontal() const { return !m_style.freeLayout && m_style.view == ViewMode::HList; }
    bool Scrollable() const;
    int ScrollPos() const { return m_scroll; }
    void ScrollBy(int delta) { ScrollTo(m_scroll + delta); }
    void ScrollTo(int pos);
    int ContentExtent() const;
    int ViewExtent() const;
    void EnsureVisible(int index);
    RectI ScrollTrack() const;
    RectI ScrollThumb() const;
    void SetScrollbarHot(bool hot) { m_scrollHot = hot; }

    // Drop insertion marker (manual order).
    int InsertIndexAt(PointI client) const;
    void SetInsertMarker(int index) { m_insertMarker = index; }

    // Keyboard navigation: neighbour of `from` in the given direction.
    int Neighbor(int from, int dx, int dy) const;

    // Free layout helpers.
    PointI CellAt(PointI client) const;
    RectI CellRect(PointI cell) const;

private:
    void Relayout();
    void PaintItem(Painter& p, IconCache& icons, int index, const RectI& r, float opacity);

    std::vector<ShellItemPtr> m_items;
    std::vector<bool> m_selected;
    std::vector<PointI> m_cells;
    ItemViewStyle m_style;
    RectI m_viewport;
    GridLayout m_grid;
    int m_scroll = 0;
    int m_insertMarker = -1;
    bool m_scrollHot = false;
};

}  // namespace of
