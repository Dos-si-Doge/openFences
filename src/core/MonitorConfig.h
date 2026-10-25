#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Geometry.h"
#include "core/Model.h"

namespace of {

struct MonitorDesc {
    std::wstring id;  // stable device path (or GDI device name as fallback)
    RectI bounds;
    RectI work;
    unsigned dpi = 96;
    bool primary = false;
};

// Stable hash of a monitor arrangement: ids, resolutions and positions (order-independent).
std::string ComputeMonitorConfigHash(const std::vector<MonitorDesc>& monitors);

// Picks the stored layout that shares the most monitors with the current arrangement.
// Returns "" when there is no candidate.
std::string FindClosestLayout(const std::map<std::string, Layout>& layouts, const std::vector<MonitorDesc>& monitors);

const MonitorDesc* FindMonitor(const std::vector<MonitorDesc>& monitors, const std::wstring& id);
const MonitorDesc* PrimaryMonitor(const std::vector<MonitorDesc>& monitors);
// The monitor containing most of the rectangle (falls back to primary).
const MonitorDesc* MonitorForRect(const std::vector<MonitorDesc>& monitors, const RectI& r);

}  // namespace of
