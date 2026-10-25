#pragma once

#include <vector>

#include "core/MonitorConfig.h"

namespace of {

// Current monitors with stable device ids, bounds, work areas and effective DPI.
std::vector<MonitorDesc> EnumerateMonitors();

}  // namespace of
