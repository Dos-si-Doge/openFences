#pragma once

#include <string>
#include <vector>

#include "app/Controller.h"
#include "shell/DragDrop.h"
#include "ui/ItemView.h"
#include "ui/WindowBase.h"

namespace of {

// Full-monitor transparent layer under the fences. Draws loose (unfenced) desktop icons and owns the
// empty-desktop gestures: drag-to-create, double-click Quick-Hide, desktop menu, page swipe, Peek backdrop.
class DesktopCanvas final : public WindowBase, public DropHandler {
public:
    explicit DesktopCanvas(MonitorDesc monitor);
    ~DesktopCanvas() override;
    bool Create();

    const MonitorDesc& Monitor() const { return m_monitor; }
    void SetMonitor(const MonitorDesc& monitor);
    void SetItems(std::vector<ShellItemPtr> items, std::vector<PointI> cells);
    ItemView& View() { return m_view; }
    void RefreshStyle();
    void SetPeek(PeekKind kind);
    void SetItemsShown(bool shown, bool animate);
    void SetPageIndicator(bool visible, float swipeFraction);
    void SelectKeys(const std::vector<std::wstring>& keys);
    // Grid cell under a screen point.
    PointI CellAtScreen(POINT screen) const;

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
    enum class Mode { None, ItemPress, Band, Swipe };
    enum class DropMode { None, Internal, ForwardDesktop };

    void OnLButtonDown(PointI pt, WPARAM keys);
    void OnMouseMove(PointI pt, WPARAM keys);
    void OnLButtonUp(PointI pt, WPARAM keys);
    void OnDoubleClick(PointI pt);
    void OnRightClick(PointI pt);
    void OnKey(WPARAM vk);
    void StartItemDrag();
    DWORD ComputeDrop(DWORD keys, POINT screen, DWORD allowed);

    MonitorDesc m_monitor;
    ItemView m_view;
    PeekKind m_peek = PeekKind::None;
    ComPtr<ID2D1Bitmap> m_wallpaper;
    uint32_t m_wallpaperGen = 0;
    DESKTOP_WALLPAPER_POSITION m_wallpaperPos = DWPOS_FILL;
    Color m_wallpaperBg{0, 0, 0, 255};
    float m_itemOpacity = 1.f;
    float m_itemTarget = 1.f;
    bool m_pageIndicator = false;
    float m_swipeFraction = 0.f;

    Mode m_mode = Mode::None;
    POINT m_pressScreen{};
    PointI m_pressClient{};
    int m_pressIndex = -1;
    bool m_pressWasSelected = false;
    RectI m_band;

    ComPtr<IDataObject> m_dragData;
    DropForwarder m_forward;
    DropMode m_dropMode = DropMode::None;
    PointI m_dropCell{};
    DropRegistration m_drop;
};

}  // namespace of
