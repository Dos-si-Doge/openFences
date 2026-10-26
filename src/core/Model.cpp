#include "core/Model.h"

#include <algorithm>
#include <cstdio>

#include "core/Strings.h"

namespace of {

std::optional<Color> Color::Parse(std::string_view text) {
    if (text.size() != 7 && text.size() != 9) return std::nullopt;
    if (text[0] != '#') return std::nullopt;
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    uint8_t v[4] = {0, 0, 0, 255};
    size_t n = (text.size() - 1) / 2;
    for (size_t i = 0; i < n; ++i) {
        int hi = hex(text[1 + i * 2]);
        int lo = hex(text[2 + i * 2]);
        if (hi < 0 || lo < 0) return std::nullopt;
        v[i] = static_cast<uint8_t>(hi * 16 + lo);
    }
    return Color{v[0], v[1], v[2], v[3]};
}

std::string Color::ToString() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", r, g, b, a);
    return buf;
}

Page& EnsurePage(Layout& layout) {
    if (layout.pages.empty()) {
        layout.pages.push_back(Page{NewId("p"), L"Page 1", {}});
    }
    layout.currentPage = std::clamp(layout.currentPage, 0, static_cast<int>(layout.pages.size()) - 1);
    return layout.pages[static_cast<size_t>(layout.currentPage)];
}

std::vector<Group*> VisibleGroups(Layout& layout) {
    std::vector<Group*> out;
    for (Group& g : layout.persistent) out.push_back(&g);
    Page& page = EnsurePage(layout);
    for (Group& g : page.groups) out.push_back(&g);
    return out;
}

std::vector<Group*> AllGroups(Layout& layout) {
    std::vector<Group*> out;
    for (Group& g : layout.persistent) out.push_back(&g);
    for (Page& p : layout.pages)
        for (Group& g : p.groups) out.push_back(&g);
    return out;
}

Group* FindGroup(Layout& layout, const std::string& groupId) {
    for (Group* g : AllGroups(layout))
        if (g->id == groupId) return g;
    return nullptr;
}

std::pair<Group*, Tab*> FindTab(Layout& layout, const std::string& tabId) {
    for (Group* g : AllGroups(layout))
        for (Tab& t : g->tabs)
            if (t.id == tabId) return {g, &t};
    return {nullptr, nullptr};
}

std::pair<Group*, Tab*> FindItemOwner(Layout& layout, const std::wstring& key) {
    for (Group* g : AllGroups(layout))
        for (Tab& t : g->tabs)
            if (t.kind == TabKind::Items && std::find(t.items.begin(), t.items.end(), key) != t.items.end()) return {g, &t};
    return {nullptr, nullptr};
}

void RemoveItemEverywhere(Layout& layout, const std::wstring& key) {
    for (Group* g : AllGroups(layout))
        for (Tab& t : g->tabs) std::erase(t.items, key);
}

std::optional<Group> TakeGroup(Layout& layout, const std::string& groupId) {
    auto take = [&](std::vector<Group>& list) -> std::optional<Group> {
        auto it = std::find_if(list.begin(), list.end(), [&](const Group& g) { return g.id == groupId; });
        if (it == list.end()) return std::nullopt;
        Group g = std::move(*it);
        list.erase(it);
        return g;
    };
    if (auto g = take(layout.persistent)) return g;
    for (Page& p : layout.pages)
        if (auto g = take(p.groups)) return g;
    return std::nullopt;
}

bool RemoveGroup(Layout& layout, const std::string& groupId) {
    return TakeGroup(layout, groupId).has_value();
}

std::vector<std::pair<std::string, std::wstring>> ListTabs(Layout& layout) {
    std::vector<std::pair<std::string, std::wstring>> out;
    for (Group* g : AllGroups(layout)) {
        for (const Tab& t : g->tabs) {
            if (t.kind != TabKind::Items) continue;
            std::wstring label = t.title;
            if (g->tabs.size() > 1) label = g->tabs.front().title + L" / " + t.title;
            out.emplace_back(t.id, label);
        }
    }
    return out;
}

bool LooseItemVisible(bool showLooseIcons, bool quickHidden, const std::wstring& key, const std::vector<std::wstring>& excludeItems) {
    if (!showLooseIcons) return false;
    if (!quickHidden) return true;
    return std::find(excludeItems.begin(), excludeItems.end(), key) != excludeItems.end();
}

Group MakeGroup(const std::wstring& title, const RectF& rect, const std::wstring& monitor) {
    Group g;
    g.id = NewId("g");
    g.monitor = monitor;
    g.rect = rect;
    Tab t;
    t.id = NewId("t");
    t.title = title;
    g.tabs.push_back(std::move(t));
    return g;
}

Group MakePortalGroup(const std::wstring& title, const std::wstring& path, const RectF& rect, const std::wstring& monitor, ViewMode view) {
    Group g = MakeGroup(title, rect, monitor);
    g.tabs[0].kind = TabKind::Portal;
    g.tabs[0].path = path;
    g.tabs[0].view = view;
    return g;
}

}  // namespace of
