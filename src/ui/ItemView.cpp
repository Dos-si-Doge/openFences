#include "ui/ItemView.h"

#include <algorithm>
#include <set>

namespace of {

namespace {
int ListIconPx(const ItemViewStyle& s) {
    return std::clamp(s.iconPx / 2, static_cast<int>(16 * s.scale), static_cast<int>(32 * s.scale));
}
}  // namespace

void ItemView::SetItems(std::vector<ShellItemPtr> items) {
    std::set<std::wstring> selectedKeys;
    for (int i = 0; i < Count(); ++i)
        if (m_selected[i]) selectedKeys.insert(m_items[i]->key);
    std::wstring focusKey = (focus >= 0 && focus < Count()) ? m_items[focus]->key : L"";
    m_items = std::move(items);
    m_selected.assign(m_items.size(), false);
    focus = -1;
    for (int i = 0; i < Count(); ++i) {
        if (selectedKeys.count(m_items[i]->key)) m_selected[i] = true;
        if (m_items[i]->key == focusKey) focus = i;
    }
    if (hover >= Count()) hover = -1;
    if (anchor >= Count()) anchor = -1;
    Relayout();
}

void ItemView::SetFreeCells(std::vector<PointI> cells) {
    m_cells = std::move(cells);
}

void ItemView::SetStyle(const ItemViewStyle& style) {
    m_style = style;
    Relayout();
}

void ItemView::SetViewport(const RectI& r) {
    m_viewport = r;
    Relayout();
}

GridSpec ItemView::CellSpec() const {
    const ItemViewStyle& s = m_style;
    int pad = static_cast<int>(6 * s.scale);
    float lineH = s.fontPx * 1.36f;
    if (!s.freeLayout && s.view != ViewMode::Icons) {
        int rowH = ListIconPx(s) + static_cast<int>(8 * s.scale);
        int w = s.view == ViewMode::List ? std::max(1, m_viewport.Width() - 2 * pad) : static_cast<int>(210 * s.scale);
        return {w, rowH, pad};
    }
    int cellW = std::max(s.iconPx + static_cast<int>(30 * s.scale), static_cast<int>(78 * s.scale));
    int cellH = s.iconPx + static_cast<int>(14 * s.scale) + static_cast<int>(2 * lineH);
    return {cellW, cellH, pad};
}

void ItemView::Relayout() {
    GridFlow flow = Horizontal() ? GridFlow::ColumnsThenScrollX : GridFlow::RowsThenScrollY;
    m_grid = ComputeGrid(m_viewport.Width(), m_viewport.Height(), Count(), CellSpec(), flow);
    m_scroll = ClampScroll(m_scroll, ContentExtent(), ViewExtent());
}

bool ItemView::Scrollable() const { return !m_style.freeLayout && ContentExtent() > ViewExtent(); }
int ItemView::ContentExtent() const { return Horizontal() ? m_grid.contentW : m_grid.contentH; }
int ItemView::ViewExtent() const { return Horizontal() ? m_viewport.Width() : m_viewport.Height(); }
void ItemView::ScrollTo(int pos) { m_scroll = ClampScroll(pos, ContentExtent(), ViewExtent()); }

RectI ItemView::ItemRect(int index) const {
    if (index < 0 || index >= Count()) return {};
    if (m_style.freeLayout) {
        PointI cell = index < static_cast<int>(m_cells.size()) ? m_cells[index] : PointI{0, index};
        return CellRect(cell);
    }
    RectI r = m_grid.CellRect(index);
    if (Horizontal()) return r.Offset(m_viewport.left - m_scroll, m_viewport.top);
    return r.Offset(m_viewport.left, m_viewport.top - m_scroll);
}

RectI ItemView::CellRect(PointI cell) const {
    GridSpec spec = CellSpec();
    return RectI::FromXYWH(m_viewport.left + spec.padding + cell.x * spec.cellW, m_viewport.top + spec.padding + cell.y * spec.cellH, spec.cellW,
                           spec.cellH);
}

PointI ItemView::CellAt(PointI p) const {
    GridSpec spec = CellSpec();
    int col = std::max(0, (p.x - m_viewport.left - spec.padding) / std::max(1, spec.cellW));
    int row = std::max(0, (p.y - m_viewport.top - spec.padding) / std::max(1, spec.cellH));
    return {col, row};
}

RectI ItemView::LabelRect(int index) const {
    RectI r = ItemRect(index);
    const ItemViewStyle& s = m_style;
    if (!s.freeLayout && s.view != ViewMode::Icons) {
        int icon = ListIconPx(s);
        return {r.left + icon + static_cast<int>(10 * s.scale), r.top, r.right - static_cast<int>(4 * s.scale), r.bottom};
    }
    int top = r.top + static_cast<int>(4 * s.scale) + s.iconPx + static_cast<int>(4 * s.scale);
    return {r.left + static_cast<int>(2 * s.scale), top, r.right - static_cast<int>(2 * s.scale), r.bottom - static_cast<int>(2 * s.scale)};
}

int ItemView::HitTest(PointI p) const {
    if (!m_viewport.Contains(p)) return -1;
    if (m_style.freeLayout) {
        for (int i = Count() - 1; i >= 0; --i)
            if (ItemRect(i).Contains(p)) return i;
        return -1;
    }
    PointI content = Horizontal() ? PointI{p.x - m_viewport.left + m_scroll, p.y - m_viewport.top}
                                  : PointI{p.x - m_viewport.left, p.y - m_viewport.top + m_scroll};
    return m_grid.IndexAt(content, Count());
}

int ItemView::InsertIndexAt(PointI p) const {
    if (Count() == 0) return 0;
    for (int i = 0; i < Count(); ++i) {
        RectI r = ItemRect(i);
        bool rowMatch = Horizontal() || m_style.view == ViewMode::List ? false : (p.y >= r.top && p.y < r.bottom);
        if (m_style.view == ViewMode::Icons || m_style.freeLayout) {
            if (rowMatch && p.x < r.Center().x) return i;
            if (p.y < r.top) return i;
        } else if (m_style.view == ViewMode::List) {
            if (p.y < r.Center().y) return i;
        } else {
            if (p.x < r.left) return i;
            if (p.x >= r.left && p.x < r.right && p.y < r.Center().y) return i;
        }
    }
    return Count();
}

void ItemView::PaintItem(Painter& p, IconCache& icons, int i, const RectI& r, float opacity) {
    const ItemViewStyle& s = m_style;
    const ShellItem& item = *m_items[i];
    bool sel = m_selected[i];
    bool hot = (i == hover);
    float iconOpacity = opacity;
    IconVariant variant;
    variant.tint = s.tint;
    if (s.chameleon && !hot && !sel) {
        iconOpacity *= s.chameleonOpacity;
        variant.desaturate = s.desaturate && !s.tint;
    }
    if (sel || hot) {
        Color hl = sel ? Color{140, 190, 255, 90} : Color{255, 255, 255, 40};
        p.FillRoundRect(r.Inflate(-static_cast<int>(2 * s.scale)), 4 * s.scale, hl, opacity);
        if (sel && i == focus) p.StrokeRoundRect(r.Inflate(-static_cast<int>(2 * s.scale)), 4 * s.scale, Color{170, 210, 255, 160}, 1.f, opacity);
    }
    bool list = !s.freeLayout && s.view != ViewMode::Icons;
    int iconPx = list ? ListIconPx(s) : s.iconPx;
    RectI iconRect = list ? RectI::FromXYWH(r.left + static_cast<int>(5 * s.scale), r.top + (r.Height() - iconPx) / 2, iconPx, iconPx)
                          : RectI::FromXYWH(r.left + (r.Width() - iconPx) / 2, r.top + static_cast<int>(4 * s.scale), iconPx, iconPx);
    if (ID2D1Bitmap* bmp = icons.Get(item, iconPx, variant)) {
        D2D1_SIZE_F sz = bmp->GetSize();
        // Keep aspect ratio for thumbnails.
        float k = std::min(iconPx / std::max(1.f, sz.width), iconPx / std::max(1.f, sz.height));
        int w = static_cast<int>(sz.width * k), h = static_cast<int>(sz.height * k);
        RectI fitted = RectI::FromXYWH(iconRect.left + (iconPx - w) / 2, iconRect.top + (iconPx - h), w, h);
        if (list) fitted = RectI::FromXYWH(iconRect.left + (iconPx - w) / 2, iconRect.top + (iconPx - h) / 2, w, h);
        p.Bitmap(bmp, fitted, iconOpacity);
    }
    TextStyle ts;
    ts.sizePx = s.fontPx;
    ts.shadow = true;
    if (list) {
        ts.h = HAlign::Left;
        ts.v = VAlign::Middle;
        ts.maxLines = 1;
    } else {
        ts.h = HAlign::Center;
        ts.v = VAlign::Top;
        ts.maxLines = sel ? 4 : 2;
    }
    float textOpacity = (s.chameleon && !hot && !sel) ? opacity * std::max(0.5f, s.chameleonOpacity) : opacity;
    RectI label = LabelRect(i);
    if (!list && sel) label.bottom = label.top + static_cast<int>(s.fontPx * 1.36f * 4) + 2;
    p.Text(item.displayName, label, s.text, ts, textOpacity);
}

void ItemView::Paint(Painter& p, IconCache& icons, float opacity, bool hidden, const std::vector<std::wstring>* alwaysVisible) {
    if (m_viewport.Empty()) return;
    p.PushClip(m_viewport);
    for (int i = 0; i < Count(); ++i) {
        RectI r = ItemRect(i);
        if (!r.Intersects(m_viewport)) continue;
        float o = opacity;
        if (hidden) {
            bool keep = alwaysVisible && std::find(alwaysVisible->begin(), alwaysVisible->end(), m_items[i]->key) != alwaysVisible->end();
            if (!keep) continue;
        }
        PaintItem(p, icons, i, r, o);
    }
    if (m_insertMarker >= 0) {
        float s = m_style.scale;
        Color c{120, 180, 255, 230};
        RectI ref = m_insertMarker < Count() ? ItemRect(m_insertMarker) : (Count() ? ItemRect(Count() - 1) : CellRect({0, 0}));
        bool after = m_insertMarker >= Count() && Count() > 0;
        if (m_style.view == ViewMode::List && !m_style.freeLayout) {
            int y = after ? ref.bottom : ref.top;
            p.Line({ref.left, y}, {ref.right, y}, c, 2 * s, opacity);
        } else {
            int x = after ? ref.right : ref.left;
            p.Line({x, ref.top + static_cast<int>(4 * s)}, {x, ref.bottom - static_cast<int>(4 * s)}, c, 2 * s, opacity);
        }
    }
    p.PopClip();
    if (Scrollable()) {
        p.FillRoundRect(ScrollTrack(), 3 * m_style.scale, Color{255, 255, 255, static_cast<uint8_t>(m_scrollHot ? 40 : 0)}, opacity);
        p.FillRoundRect(ScrollThumb(), 3 * m_style.scale, Color{255, 255, 255, static_cast<uint8_t>(m_scrollHot ? 170 : 110)}, opacity);
    }
}

RectI ItemView::ScrollTrack() const {
    int w = static_cast<int>((m_scrollHot ? 8 : 5) * m_style.scale);
    int m = static_cast<int>(2 * m_style.scale);
    if (Horizontal()) return {m_viewport.left + m, m_viewport.bottom - w - m, m_viewport.right - m, m_viewport.bottom - m};
    return {m_viewport.right - w - m, m_viewport.top + m, m_viewport.right - m, m_viewport.bottom - m};
}

RectI ItemView::ScrollThumb() const {
    RectI t = ScrollTrack();
    int content = std::max(1, ContentExtent());
    int view = ViewExtent();
    if (Horizontal()) {
        int len = std::max(static_cast<int>(24 * m_style.scale), t.Width() * view / content);
        int range = std::max(1, content - view);
        int x = t.left + (t.Width() - len) * m_scroll / range;
        return {x, t.top, x + len, t.bottom};
    }
    int len = std::max(static_cast<int>(24 * m_style.scale), t.Height() * view / content);
    int range = std::max(1, content - view);
    int y = t.top + (t.Height() - len) * m_scroll / range;
    return {t.left, y, t.right, y + len};
}

void ItemView::EnsureVisible(int index) {
    if (index < 0 || index >= Count() || m_style.freeLayout) return;
    RectI r = ItemRect(index);
    if (Horizontal()) {
        if (r.left < m_viewport.left) ScrollBy(r.left - m_viewport.left);
        else if (r.right > m_viewport.right) ScrollBy(r.right - m_viewport.right);
    } else {
        if (r.top < m_viewport.top) ScrollBy(r.top - m_viewport.top);
        else if (r.bottom > m_viewport.bottom) ScrollBy(r.bottom - m_viewport.bottom);
    }
}

bool ItemView::IsSelected(int i) const { return i >= 0 && i < Count() && m_selected[i]; }

void ItemView::SelectOnly(int i) {
    m_selected.assign(m_items.size(), false);
    if (i >= 0 && i < Count()) m_selected[i] = true;
    focus = anchor = i;
}

void ItemView::Toggle(int i) {
    if (i < 0 || i >= Count()) return;
    m_selected[i] = !m_selected[i];
    focus = anchor = i;
}

void ItemView::SelectRange(int from, int to) {
    if (from < 0) from = to;
    m_selected.assign(m_items.size(), false);
    for (int i = std::min(from, to); i <= std::max(from, to); ++i)
        if (i >= 0 && i < Count()) m_selected[i] = true;
    focus = to;
}

void ItemView::SelectAll() { m_selected.assign(m_items.size(), true); }
void ItemView::ClearSelection() { m_selected.assign(m_items.size(), false); }

void ItemView::SelectInRect(const RectI& r, bool additive) {
    if (!additive) ClearSelection();
    for (int i = 0; i < Count(); ++i)
        if (ItemRect(i).Intersects(r)) m_selected[i] = true;
}

std::vector<ShellItemPtr> ItemView::SelectedItems() const {
    std::vector<ShellItemPtr> out;
    for (int i = 0; i < Count(); ++i)
        if (m_selected[i]) out.push_back(m_items[i]);
    return out;
}

std::vector<int> ItemView::SelectedIndices() const {
    std::vector<int> out;
    for (int i = 0; i < Count(); ++i)
        if (m_selected[i]) out.push_back(i);
    return out;
}

int ItemView::SelectedCount() const { return static_cast<int>(std::count(m_selected.begin(), m_selected.end(), true)); }

int ItemView::IndexOfKey(const std::wstring& key) const {
    for (int i = 0; i < Count(); ++i)
        if (m_items[i]->key == key) return i;
    return -1;
}

int ItemView::Neighbor(int from, int dx, int dy) const {
    if (Count() == 0) return -1;
    if (from < 0) return 0;
    RectI cur = ItemRect(from);
    PointI c = cur.Center();
    int best = -1;
    long long bestScore = LLONG_MAX;
    for (int i = 0; i < Count(); ++i) {
        if (i == from) continue;
        PointI o = ItemRect(i).Center();
        int ddx = o.x - c.x, ddy = o.y - c.y;
        if ((dx > 0 && ddx <= 0) || (dx < 0 && ddx >= 0) || (dy > 0 && ddy <= 0) || (dy < 0 && ddy >= 0)) continue;
        // Prefer items in the movement direction, penalise perpendicular distance.
        long long along = dx ? std::abs(ddx) : std::abs(ddy);
        long long across = dx ? std::abs(ddy) : std::abs(ddx);
        long long score = along + across * 4;
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best < 0 ? from : best;
}

}  // namespace of
