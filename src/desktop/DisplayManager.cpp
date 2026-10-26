// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "desktop/DisplayManager.h"

#include <windows.h>
#include <shellscalingapi.h>

#include <map>

namespace of {

namespace {

// Maps GDI device names (\\.\DISPLAY1) to monitor device paths, which survive reboots and port changes.
std::map<std::wstring, std::wstring> MonitorDevicePaths() {
    std::map<std::wstring, std::wstring> out;
    UINT32 pathCount = 0, modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) return out;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS) return out;
    for (UINT32 i = 0; i < pathCount; ++i) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source.header.size = sizeof(source);
        source.header.adapterId = paths[i].sourceInfo.adapterId;
        source.header.id = paths[i].sourceInfo.id;
        DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
        target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        target.header.size = sizeof(target);
        target.header.adapterId = paths[i].targetInfo.adapterId;
        target.header.id = paths[i].targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&source.header) == ERROR_SUCCESS && DisplayConfigGetDeviceInfo(&target.header) == ERROR_SUCCESS &&
            target.monitorDevicePath[0]) {
            out.emplace(source.viewGdiDeviceName, target.monitorDevicePath);
        }
    }
    return out;
}

}  // namespace

std::vector<MonitorDesc> EnumerateMonitors() {
    struct Ctx {
        std::vector<MonitorDesc> list;
        std::map<std::wstring, std::wstring> paths;
    } ctx;
    ctx.paths = MonitorDevicePaths();
    EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR mon, HDC, LPRECT, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<Ctx*>(lp);
            MONITORINFOEXW mi{};
            mi.cbSize = sizeof(mi);
            if (!GetMonitorInfoW(mon, &mi)) return TRUE;
            MonitorDesc d;
            auto it = c->paths.find(mi.szDevice);
            d.id = it != c->paths.end() ? it->second : std::wstring(mi.szDevice);
            d.bounds = {mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right, mi.rcMonitor.bottom};
            d.work = {mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom};
            d.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
            UINT dx = 96, dy = 96;
            if (SUCCEEDED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy))) d.dpi = dx;
            c->list.push_back(std::move(d));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&ctx));
    return ctx.list;
}

}  // namespace of
