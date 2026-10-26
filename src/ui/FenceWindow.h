// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <vector>

#include "core/Model.h"
#include "shell/DragDrop.h"
#include "ui/ItemView.h"
#include "ui/WindowBase.h"

namespace of {

// One fence (group) on the desktop: title bar / tab strip, item view, roll-up, drag and drop.
class FenceWindow final : public WindowBase, public DropHandler {
public:
    explicit FenceWindow(std::string groupId);
    ~FenceWindow() override;
    bool Create();

    const std::string& GroupId() const { return m_groupId; }
    // Re-reads the group from the configuration (items, style, geometry).
    void SyncFromModel();
    // Places the window from the model rectangle (honouring roll-up), plus a horizontal page-slide offset.
    void ApplyGeometry(int offsetX = 0);
    // Quick-Hide fade.
    void SetShown(bool shown, bool animate);
    bool Shown() const { return m_targetOpacity > 0; }
    bool TitleContains(POINT screen) const;
    void SetMergeHighlight(bool on);
    void BeginTitleRename();
    void SelectKeys(const std::vector<std::wstring>& keys);

    // DropHandler
    DWORD DragEnter(IDataObject* data, DWORD keys, POINT pt, DWORD allowed) override;
    DWORD DragOver(DWORD keys, POINT pt, DWORD allowed) override;
    void DragLeave() override;
    DWORD Drop(IDataObject* data, DWORD keys, POINT pt, DWORD allowed) override;

protected:
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp) override;
    void Paint(ID2D1DeviceContext* dc) override;
    bool Tick() override;
    void OnDpiChanged() override;

private:
    enum class Mode { None, Move, Resize, Band, ItemPress, TabPress, TabDetach, Scroll, TitlePress };
    enum class DropMode { None, Internal, ForwardFolder, ForwardDesktop };
    struct TabRect {
        int index;
        RectI rect;
    };

    Group* G() const;
    Tab* ActiveTab() const;
    int TitleH() const;
    RectI TitleRect() const;
    RectI MenuButtonRect() const;
    RectI CloseButtonRect() const;
    RectI BreadcrumbRect() const;
    RectI BodyRect() const;
    std::vector<TabRect> TabRects() const;
    int TabAt(PointI client) const;
    unsigned BorderHit(PointI client) const;
    bool Collapsed() const;
    bool AutoRollup() const;
    void SetExpanded(bool expanded);
    bool ShowTabs() const;
    bool IsPortal() const;

    void OnMouseMove(PointI pt, WPARAM keys);
    void OnLButtonDown(PointI pt, WPARAM keys);
    void OnLButtonUp(PointI pt, WPARAM keys);
    void OnDoubleClick(PointI pt, WPARAM keys);
    void OnRightClick(PointI pt);
    void OnKey(WPARAM vk);
    void OnWheel(int delta, bool horizontal);
    void StartItemDrag();
    void OpenItem(int index, bool forceExplorer);
    void RenameSelected();
    void PasteHere();
    DWORD ComputeDrop(DWORD keys, POINT screen, DWORD allowed);
    void UpdateViewport();

    std::string m_groupId;
    ItemView m_view;
    float m_opacity = 1.f;
    float m_targetOpacity = 1.f;
    bool m_expanded = false;     // temporarily expanded while rolled up
    bool m_hoverWindow = false;  // mouse is over the window
    bool m_mergeHighlight = false;
    bool m_tracking = false;

    Mode m_mode = Mode::None;
    POINT m_pressScreen{};
    PointI m_pressClient{};
    RectI m_startRect;
    unsigned m_resizeEdges = 0;
    int m_pressIndex = -1;
    int m_pressTab = -1;
    bool m_pressWasSelected = false;
    int m_scrollStart = 0;
    RectI m_band;
    std::string m_mergeTarget;

    // Drag and drop (as a target).
    ComPtr<IDataObject> m_dragData;
    DropForwarder m_forward;
    DropMode m_dropMode = DropMode::None;
    bool m_dropHighlight = false;
    int m_hoverTab = -1;
    std::string m_dropTabId;
    int m_dropIndex = -1;

    DropRegistration m_drop;
    int m_offsetX = 0;
};

}  // namespace of
