#pragma once

#include <windows.h>

#include "render/RenderDevice.h"

namespace of {

// A per-pixel-alpha drawing surface for one HWND (DXGI composition swap chain + DirectComposition visual).
// The window must be created with WS_EX_NOREDIRECTIONBITMAP.
class Surface {
public:
    ~Surface();
    void Attach(HWND hwnd) { m_hwnd = hwnd; }
    // Returns a context whose target is the back buffer, cleared to transparent; nullptr on failure.
    ID2D1DeviceContext* BeginDraw();
    // Presents. Returns false if the device was lost (caller should redraw after Gfx().Recreate()).
    bool EndDraw();
    void Reset();

private:
    bool EnsureResources(UINT w, UINT h);

    HWND m_hwnd = nullptr;
    uint32_t m_generation = 0;
    UINT m_width = 0, m_height = 0;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID2D1DeviceContext> m_dc;
    ComPtr<ID2D1Bitmap1> m_target;
    ComPtr<IDCompositionTarget> m_dcTarget;
    ComPtr<IDCompositionVisual> m_visual;
};

}  // namespace of
