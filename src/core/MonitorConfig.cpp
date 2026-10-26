// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "core/MonitorConfig.h"

#include <algorithm>
#include <cstdio>

#include "core/Strings.h"

namespace of {

std::string ComputeMonitorConfigHash(const std::vector<MonitorDesc>& monitors) {
    std::vector<std::wstring> parts;
    for (const auto& m : monitors) {
        wchar_t buf[128];
        swprintf_s(buf, L"|%dx%d@%d,%d", m.bounds.Width(), m.bounds.Height(), m.bounds.left, m.bounds.top);
        parts.push_back(ToLower(m.id) + buf);
    }
    std::sort(parts.begin(), parts.end());
    // FNV-1a 64-bit.
    uint64_t h = 1469598103934665603ull;
    for (const auto& p : parts) {
        for (wchar_t c : p) {
            h ^= static_cast<uint64_t>(c);
            h *= 1099511628211ull;
        }
        h ^= 0xFF;
        h *= 1099511628211ull;
    }
    char out[24];
    std::snprintf(out, sizeof(out), "%016llx", static_cast<unsigned long long>(h));
    return out;
}

std::string FindClosestLayout(const std::map<std::string, Layout>& layouts, const std::vector<MonitorDesc>& monitors) {
    std::string best;
    int bestScore = -1;
    for (const auto& [key, layout] : layouts) {
        int score = 0;
        for (const auto& id : layout.monitors)
            for (const auto& m : monitors)
                if (EqualsI(id, m.id)) ++score;
        // Prefer layouts with groups in them; tie-break on fewer extra monitors.
        int groups = 0;
        for (const auto& p : layout.pages) groups += static_cast<int>(p.groups.size());
        groups += static_cast<int>(layout.persistent.size());
        int weighted = score * 1000 + (groups > 0 ? 100 : 0) - static_cast<int>(layout.monitors.size());
        if (weighted > bestScore) {
            bestScore = weighted;
            best = key;
        }
    }
    return best;
}

const MonitorDesc* FindMonitor(const std::vector<MonitorDesc>& monitors, const std::wstring& id) {
    for (const auto& m : monitors)
        if (EqualsI(m.id, id)) return &m;
    return nullptr;
}

const MonitorDesc* PrimaryMonitor(const std::vector<MonitorDesc>& monitors) {
    for (const auto& m : monitors)
        if (m.primary) return &m;
    return monitors.empty() ? nullptr : &monitors.front();
}

const MonitorDesc* MonitorForRect(const std::vector<MonitorDesc>& monitors, const RectI& r) {
    const MonitorDesc* best = nullptr;
    long long bestArea = 0;
    for (const auto& m : monitors) {
        RectI i = Intersect(m.bounds, r);
        long long area = static_cast<long long>(i.Width()) * i.Height();
        if (area > bestArea) {
            bestArea = area;
            best = &m;
        }
    }
    return best ? best : PrimaryMonitor(monitors);
}

}  // namespace of
