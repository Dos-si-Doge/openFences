#include "render/RenderDevice.h"

#include "core/Log.h"

namespace of {

RenderDevice& Gfx() {
    static RenderDevice device;
    return device;
}

bool RenderDevice::Init() {
    D2D1_FACTORY_OPTIONS opts{};
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &opts,
                                 reinterpret_cast<void**>(m_d2dFactory.GetAddressOf())))) {
        log::Info(L"D2D1CreateFactory failed");
        return false;
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(m_dwrite.GetAddressOf())))) {
        log::Info(L"DWriteCreateFactory failed");
        return false;
    }
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wic)))) {
        log::Info(L"WIC factory creation failed");
        return false;
    }
    return CreateDeviceResources();
}

bool RenderDevice::CreateDeviceResources() {
    m_resourceContext.Reset();
    m_d2dDevice.Reset();
    m_dcomp.Reset();
    m_dxgiFactory.Reset();
    m_dxgiDevice.Reset();
    m_d3d.Reset();

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &m_d3d, nullptr, nullptr);
    if (FAILED(hr)) {
        log::Info(L"Hardware D3D11 device unavailable (0x{:08x}); falling back to WARP", static_cast<unsigned>(hr));
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &m_d3d, nullptr, nullptr);
    }
    if (FAILED(hr)) return false;
    if (FAILED(m_d3d.As(&m_dxgiDevice))) return false;

    ComPtr<IDXGIAdapter> adapter;
    m_dxgiDevice->GetAdapter(&adapter);
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&m_dxgiFactory)))) return false;

    if (FAILED(m_d2dFactory->CreateDevice(m_dxgiDevice.Get(), &m_d2dDevice))) return false;
    if (FAILED(m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_resourceContext))) return false;
    if (FAILED(DCompositionCreateDevice(m_dxgiDevice.Get(), IID_PPV_ARGS(&m_dcomp)))) return false;
    ++m_generation;
    return true;
}

bool RenderDevice::Recreate() {
    log::Info(L"Recreating render device");
    return CreateDeviceResources();
}

}  // namespace of
