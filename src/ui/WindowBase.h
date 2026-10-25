#pragma once

#include <windows.h>

#include "core/Geometry.h"
#include "render/Surface.h"

namespace of {

// Base for openFences' desktop-layer windows: a DirectComposition surface, DPI tracking, animation
// timer and z-order protection (Explorer and activation must not lift us above other windows).
class WindowBase {
public:
    virtual ~WindowBase();
    HWND Hwnd() const { return m_hwnd; }
    float Scale() const { return m_dpi / 96.f; }
    int Px(float dip) const { return static_cast<int>(dip * Scale() + 0.5f); }
    RectI ClientRect() const;
    RectI ScreenRect() const;
    void Invalidate();
    // While false, WM_WINDOWPOSCHANGING strips z-order changes.
    void AllowZOrder(bool allow) { m_allowZ = allow; }

    // Animation helper: runs Tick() at ~60 fps until it returns false.
    void StartAnimation();

protected:
    bool CreateDesktopWindow(const wchar_t* className, const wchar_t* title, const RectI& screen, DWORD exStyle = 0);
    virtual LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);
    virtual void Paint(ID2D1DeviceContext* dc) = 0;
    virtual bool Tick() { return false; }
    virtual void OnDpiChanged() {}

    HWND m_hwnd = nullptr;
    UINT m_dpi = 96;
    Surface m_surface;

private:
    static LRESULT CALLBACK StaticProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void DoPaint();
    bool m_allowZ = false;
    bool m_animating = false;
};

constexpr UINT_PTR kAnimTimer = 0xA001;

}  // namespace of
