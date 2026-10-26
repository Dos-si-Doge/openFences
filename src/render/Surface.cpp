// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "render/Surface.h"

#include "core/Log.h"

namespace of {

Surface::~Surface() { Reset(); }

void Surface::Reset() {
    if (m_dc) m_dc->SetTarget(nullptr);
    m_target.Reset();
    m_dc.Reset();
    m_swapChain.Reset();
    m_visual.Reset();
    m_dcTarget.Reset();
    m_width = m_height = 0;
}

bool Surface::EnsureResources(UINT w, UINT h) {
    RenderDevice& gfx = Gfx();
    if (m_generation != gfx.Generation()) {
        Reset();
        m_generation = gfx.Generation();
    }
    if (!m_swapChain) {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = w;
        desc.Height = h;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        HRESULT hr = gfx.DxgiFactory()->CreateSwapChainForComposition(gfx.D3D(), &desc, nullptr, &m_swapChain);
        if (FAILED(hr)) {
            log::Info(L"CreateSwapChainForComposition failed 0x{:08x}", static_cast<unsigned>(hr));
            return false;
        }
        if (FAILED(gfx.DComp()->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcTarget))) return false;
        if (FAILED(gfx.DComp()->CreateVisual(&m_visual))) return false;
        m_visual->SetContent(m_swapChain.Get());
        m_dcTarget->SetRoot(m_visual.Get());
        gfx.DComp()->Commit();
        if (FAILED(gfx.D2DDevice()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_dc))) return false;
        m_width = w;
        m_height = h;
    } else if (w != m_width || h != m_height) {
        m_dc->SetTarget(nullptr);
        m_target.Reset();
        HRESULT hr = m_swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) {
            log::Info(L"ResizeBuffers failed 0x{:08x}", static_cast<unsigned>(hr));
            Reset();
            return false;
        }
        m_width = w;
        m_height = h;
    }
    if (!m_target) {
        ComPtr<IDXGISurface> buffer;
        if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)))) return false;
        D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        if (FAILED(m_dc->CreateBitmapFromDxgiSurface(buffer.Get(), &props, &m_target))) return false;
        m_dc->SetTarget(m_target.Get());
    }
    return true;
}

ID2D1DeviceContext* Surface::BeginDraw() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    UINT w = static_cast<UINT>(std::max<LONG>(1, rc.right - rc.left));
    UINT h = static_cast<UINT>(std::max<LONG>(1, rc.bottom - rc.top));
    if (!EnsureResources(w, h)) return nullptr;
    m_dc->BeginDraw();
    m_dc->SetTransform(D2D1::Matrix3x2F::Identity());
    m_dc->Clear(D2D1::ColorF(0, 0, 0, 0));
    return m_dc.Get();
}

bool Surface::EndDraw() {
    if (!m_dc) return true;
    HRESULT hr = m_dc->EndDraw();
    if (SUCCEEDED(hr)) hr = m_swapChain->Present(1, 0);
    if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        Reset();
        return false;
    }
    return true;
}

}  // namespace of
