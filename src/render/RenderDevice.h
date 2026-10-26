// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>

namespace of {

using Microsoft::WRL::ComPtr;

// Process-wide Direct3D 11 / Direct2D / DirectComposition / DirectWrite objects.
// All windows render through these; a device loss recreates everything and bumps Generation().
class RenderDevice {
public:
    bool Init();
    bool Recreate();
    uint32_t Generation() const { return m_generation; }

    ID3D11Device* D3D() const { return m_d3d.Get(); }
    IDXGIFactory2* DxgiFactory() const { return m_dxgiFactory.Get(); }
    ID2D1Factory1* D2DFactory() const { return m_d2dFactory.Get(); }
    ID2D1Device* D2DDevice() const { return m_d2dDevice.Get(); }
    IDCompositionDevice* DComp() const { return m_dcomp.Get(); }
    IDWriteFactory* DWrite() const { return m_dwrite.Get(); }
    IWICImagingFactory* Wic() const { return m_wic.Get(); }
    // Context used to create bitmaps shared by every window's context.
    ID2D1DeviceContext* ResourceContext() const { return m_resourceContext.Get(); }

private:
    bool CreateDeviceResources();

    uint32_t m_generation = 0;
    ComPtr<ID3D11Device> m_d3d;
    ComPtr<IDXGIDevice> m_dxgiDevice;
    ComPtr<IDXGIFactory2> m_dxgiFactory;
    ComPtr<ID2D1Factory1> m_d2dFactory;
    ComPtr<ID2D1Device> m_d2dDevice;
    ComPtr<ID2D1DeviceContext> m_resourceContext;
    ComPtr<IDCompositionDevice> m_dcomp;
    ComPtr<IDWriteFactory> m_dwrite;
    ComPtr<IWICImagingFactory> m_wic;
};

RenderDevice& Gfx();

}  // namespace of
