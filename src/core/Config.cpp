#include "core/Config.h"

#include <windows.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "core/Strings.h"

using nlohmann::json;

namespace of {

namespace {

// ---- enum <-> string tables ----
template <typename E>
struct EnumName {
    E value;
    const char* name;
};

constexpr EnumName<TabKind> kTabKinds[] = {{TabKind::Items, "items"}, {TabKind::Portal, "portal"}};
constexpr EnumName<ViewMode> kViewModes[] = {{ViewMode::Icons, "icons"}, {ViewMode::List, "list"}, {ViewMode::HList, "hlist"}};
constexpr EnumName<SortMode> kSortModes[] = {{SortMode::Manual, "manual"}, {SortMode::Name, "name"}, {SortMode::Type, "type"}, {SortMode::Date, "date"}};
constexpr EnumName<MatchMode> kMatchModes[] = {{MatchMode::All, "all"}, {MatchMode::Any, "any"}};
constexpr EnumName<ItemKindFilter> kKinds[] = {{ItemKindFilter::Any, "any"}, {ItemKindFilter::File, "file"}, {ItemKindFilter::Folder, "folder"}, {ItemKindFilter::Shortcut, "shortcut"}};
constexpr EnumName<RuleAction> kActions[] = {{RuleAction::AssignToTab, "assign"}, {RuleAction::MoveToFolder, "moveToFolder"}};
constexpr EnumName<TabStyle> kTabStyles[] = {{TabStyle::Flat, "flat"}, {TabStyle::Segmented, "segmented"}, {TabStyle::Rounded, "rounded"}, {TabStyle::Hidden, "hidden"}};
constexpr EnumName<RollupOpen> kRollupOpen[] = {{RollupOpen::Hover, "hover"}, {RollupOpen::HoverDelay, "hoverDelay"}, {RollupOpen::Click, "click"}};
constexpr EnumName<TriggerMode> kTriggerModes[] = {{TriggerMode::Toggle, "toggle"}, {TriggerMode::Hold, "hold"}};
constexpr EnumName<HotCorner> kCorners[] = {{HotCorner::TaskbarEnd, "taskbarEnd"}, {HotCorner::TopLeft, "topLeft"}, {HotCorner::TopRight, "topRight"}, {HotCorner::BottomLeft, "bottomLeft"}, {HotCorner::BottomRight, "bottomRight"}};
constexpr EnumName<HostMode> kHostModes[] = {{HostMode::OwnedByDesktop, "owned"}, {HostMode::ChildOfDesktop, "child"}};

template <typename E, size_t N>
std::string EnumToStr(const EnumName<E> (&table)[N], E v) {
    for (const auto& e : table)
        if (e.value == v) return e.name;
    return table[0].name;
}

template <typename E, size_t N>
E EnumFromStr(const EnumName<E> (&table)[N], const json& j, const char* key, E fallback) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return fallback;
    std::string s = it->get<std::string>();
    for (const auto& e : table)
        if (s == e.name) return e.value;
    return fallback;
}

// ---- primitive helpers ----
template <typename T>
T Get(const json& j, const char* key, T fallback) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return fallback;
    try {
        return it->get<T>();
    } catch (const json::exception&) {
        return fallback;
    }
}

std::wstring GetW(const json& j, const char* key, const std::wstring& fallback = {}) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return fallback;
    return FromUtf8(it->get<std::string>());
}

std::optional<int> GetOptInt(const json& j, const char* key) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer()) return std::nullopt;
    return it->get<int>();
}

std::optional<Color> GetColor(const json& j, const char* key) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return std::nullopt;
    return Color::Parse(it->get<std::string>());
}

void PutColor(json& j, const char* key, const std::optional<Color>& c) {
    if (c) j[key] = c->ToString();
}

json WList(const std::vector<std::wstring>& v) {
    json a = json::array();
    for (const auto& s : v) a.push_back(ToUtf8(s));
    return a;
}

std::vector<std::wstring> GetWList(const json& j, const char* key) {
    std::vector<std::wstring> out;
    auto it = j.find(key);
    if (it == j.end() || !it->is_array()) return out;
    for (const auto& e : *it)
        if (e.is_string()) out.push_back(FromUtf8(e.get<std::string>()));
    return out;
}

// ---- model <-> json ----
json ToJson(const Tab& t) {
    json j;
    j["id"] = t.id;
    j["title"] = ToUtf8(t.title);
    PutColor(j, "color", t.color);
    j["kind"] = EnumToStr(kTabKinds, t.kind);
    if (t.kind == TabKind::Items) j["items"] = WList(t.items);
    if (!t.path.empty()) j["path"] = ToUtf8(t.path);
    j["view"] = EnumToStr(kViewModes, t.view);
    j["sort"] = EnumToStr(kSortModes, t.sort);
    if (t.iconSize) j["iconSize"] = t.iconSize;
    j["allowNavigation"] = t.allowNavigation;
    return j;
}

Tab TabFromJson(const json& j) {
    Tab t;
    t.id = Get<std::string>(j, "id", NewId("t"));
    t.title = GetW(j, "title", L"Fence");
    t.color = GetColor(j, "color");
    t.kind = EnumFromStr(kTabKinds, j, "kind", TabKind::Items);
    t.items = GetWList(j, "items");
    t.path = GetW(j, "path");
    t.view = EnumFromStr(kViewModes, j, "view", ViewMode::Icons);
    t.sort = EnumFromStr(kSortModes, j, "sort", SortMode::Manual);
    t.iconSize = Get<int>(j, "iconSize", 0);
    t.allowNavigation = Get<bool>(j, "allowNavigation", true);
    return t;
}

json ToJson(const Group& g) {
    json j;
    j["id"] = g.id;
    j["monitor"] = ToUtf8(g.monitor);
    j["rect"] = {g.rect.x, g.rect.y, g.rect.w, g.rect.h};
    j["rolledUp"] = g.rolledUp;
    j["locked"] = g.locked;
    j["contentLocked"] = g.contentLocked;
    j["hideExclude"] = g.hideExclude;
    j["activeTab"] = g.activeTab;
    PutColor(j, "background", g.background);
    PutColor(j, "iconTint", g.iconTint);
    json tabs = json::array();
    for (const Tab& t : g.tabs) tabs.push_back(ToJson(t));
    j["tabs"] = tabs;
    return j;
}

Group GroupFromJson(const json& j) {
    Group g;
    g.id = Get<std::string>(j, "id", NewId("g"));
    g.monitor = GetW(j, "monitor");
    if (auto it = j.find("rect"); it != j.end() && it->is_array() && it->size() == 4) {
        g.rect = {(*it)[0].get<double>(), (*it)[1].get<double>(), (*it)[2].get<double>(), (*it)[3].get<double>()};
    } else {
        g.rect = {0.05, 0.05, 0.2, 0.3};
    }
    g.rolledUp = Get<bool>(j, "rolledUp", false);
    g.locked = Get<bool>(j, "locked", false);
    g.contentLocked = Get<bool>(j, "contentLocked", false);
    g.hideExclude = Get<bool>(j, "hideExclude", false);
    g.activeTab = Get<int>(j, "activeTab", 0);
    g.background = GetColor(j, "background");
    g.iconTint = GetColor(j, "iconTint");
    if (auto it = j.find("tabs"); it != j.end() && it->is_array())
        for (const auto& t : *it) g.tabs.push_back(TabFromJson(t));
    if (g.tabs.empty()) {
        Tab t;
        t.id = NewId("t");
        t.title = L"Fence";
        g.tabs.push_back(t);
    }
    return g;
}

json ToJson(const Layout& l) {
    json j;
    j["monitors"] = WList(l.monitors);
    j["currentPage"] = l.currentPage;
    json pages = json::array();
    for (const Page& p : l.pages) {
        json pj;
        pj["id"] = p.id;
        pj["name"] = ToUtf8(p.name);
        json groups = json::array();
        for (const Group& g : p.groups) groups.push_back(ToJson(g));
        pj["groups"] = groups;
        pages.push_back(pj);
    }
    j["pages"] = pages;
    json persistent = json::array();
    for (const Group& g : l.persistent) persistent.push_back(ToJson(g));
    j["persistent"] = persistent;
    json loose = json::object();
    for (const auto& [key, pos] : l.loose) loose[ToUtf8(key)] = {{"monitor", ToUtf8(pos.monitor)}, {"col", pos.col}, {"row", pos.row}};
    j["loose"] = loose;
    return j;
}

Layout LayoutFromJson(const json& j) {
    Layout l;
    l.monitors = GetWList(j, "monitors");
    l.currentPage = Get<int>(j, "currentPage", 0);
    if (auto it = j.find("pages"); it != j.end() && it->is_array()) {
        for (const auto& pj : *it) {
            Page p;
            p.id = Get<std::string>(pj, "id", NewId("p"));
            p.name = GetW(pj, "name", L"Page");
            if (auto gi = pj.find("groups"); gi != pj.end() && gi->is_array())
                for (const auto& gj : *gi) p.groups.push_back(GroupFromJson(gj));
            l.pages.push_back(std::move(p));
        }
    }
    if (auto it = j.find("persistent"); it != j.end() && it->is_array())
        for (const auto& gj : *it) l.persistent.push_back(GroupFromJson(gj));
    if (auto it = j.find("loose"); it != j.end() && it->is_object()) {
        for (auto e = it->begin(); e != it->end(); ++e) {
            LoosePos pos;
            pos.monitor = GetW(e.value(), "monitor");
            pos.col = Get<int>(e.value(), "col", 0);
            pos.row = Get<int>(e.value(), "row", 0);
            l.loose[FromUtf8(e.key())] = pos;
        }
    }
    EnsurePage(l);
    return l;
}

json ToJson(const Rule& r) {
    json m;
    if (!r.match.extensions.empty()) m["ext"] = WList(r.match.extensions);
    if (!r.match.nameGlob.empty()) m["nameGlob"] = ToUtf8(r.match.nameGlob);
    if (!r.match.nameRegex.empty()) m["nameRegex"] = ToUtf8(r.match.nameRegex);
    if (!r.match.targetPrefix.empty()) m["lnkTargetPrefix"] = ToUtf8(r.match.targetPrefix);
    if (r.match.kind != ItemKindFilter::Any) m["kind"] = EnumToStr(kKinds, r.match.kind);
    if (r.match.modifiedOlderThanDays) m["modifiedOlderThanDays"] = *r.match.modifiedOlderThanDays;
    if (r.match.modifiedNewerThanDays) m["modifiedNewerThanDays"] = *r.match.modifiedNewerThanDays;
    if (r.match.createdOlderThanDays) m["createdOlderThanDays"] = *r.match.createdOlderThanDays;
    if (r.match.createdNewerThanDays) m["createdNewerThanDays"] = *r.match.createdNewerThanDays;
    json j;
    j["id"] = r.id;
    j["name"] = ToUtf8(r.name);
    j["enabled"] = r.enabled;
    j["mode"] = EnumToStr(kMatchModes, r.mode);
    j["match"] = m.is_null() ? json::object() : m;
    j["action"] = EnumToStr(kActions, r.action);
    if (!r.targetTab.empty()) j["target"] = r.targetTab;
    if (!r.targetFolder.empty()) j["targetFolder"] = ToUtf8(r.targetFolder);
    return j;
}

Rule RuleFromJson(const json& j) {
    Rule r;
    r.id = Get<std::string>(j, "id", NewId("r"));
    r.name = GetW(j, "name", L"Rule");
    r.enabled = Get<bool>(j, "enabled", true);
    r.mode = EnumFromStr(kMatchModes, j, "mode", MatchMode::All);
    json m = j.value("match", json::object());
    r.match.extensions = GetWList(m, "ext");
    for (auto& e : r.match.extensions) {
        e = ToLower(e);
        if (!e.empty() && e[0] != L'.') e.insert(e.begin(), L'.');
    }
    r.match.nameGlob = GetW(m, "nameGlob");
    r.match.nameRegex = GetW(m, "nameRegex");
    r.match.targetPrefix = GetW(m, "lnkTargetPrefix");
    r.match.kind = EnumFromStr(kKinds, m, "kind", ItemKindFilter::Any);
    r.match.modifiedOlderThanDays = GetOptInt(m, "modifiedOlderThanDays");
    r.match.modifiedNewerThanDays = GetOptInt(m, "modifiedNewerThanDays");
    r.match.createdOlderThanDays = GetOptInt(m, "createdOlderThanDays");
    r.match.createdNewerThanDays = GetOptInt(m, "createdNewerThanDays");
    r.action = EnumFromStr(kActions, j, "action", RuleAction::AssignToTab);
    r.targetTab = Get<std::string>(j, "target", "");
    r.targetFolder = GetW(j, "targetFolder");
    return r;
}

json ToJson(const Config& c) {
    json j;
    j["version"] = kConfigVersion;
    j["general"] = {
        {"firstRunDone", c.general.firstRunDone},
        {"hostMode", EnumToStr(kHostModes, c.general.hostMode)},
        {"showLooseIcons", c.general.showLooseIcons},
        {"language", ToUtf8(c.general.language)},
    };
    json ap = {
        {"background", c.appearance.background.ToString()},
        {"darkenTitle", c.appearance.darkenTitle},
        {"cornerRadius", c.appearance.cornerRadius},
        {"iconSize", c.appearance.iconSize},
        {"titleFontSize", c.appearance.titleFontSize},
        {"wallpaperAware", c.appearance.wallpaperAware},
        {"tabStyle", EnumToStr(kTabStyles, c.appearance.tabStyle)},
        {"tabColors", c.appearance.tabColors},
    };
    PutColor(ap, "iconTint", c.appearance.iconTint);
    j["appearance"] = ap;
    j["chameleon"] = {{"enabled", c.chameleon.enabled}, {"opacity", c.chameleon.opacity}, {"desaturate", c.chameleon.desaturate}};
    j["hide"] = {{"enabled", c.hide.enabled}, {"hidden", c.hide.hidden}, {"autoHide", c.hide.autoHide}, {"excludeItems", WList(c.hide.excludeItems)}};
    j["rollup"] = {{"openMode", EnumToStr(kRollupOpen, c.rollup.openMode)}, {"hoverDelayMs", c.rollup.hoverDelayMs}, {"autoRollupAtEdges", c.rollup.autoRollupAtEdges}};
    j["peek"] = {
        {"enabled", c.peek.enabled},
        {"hotkey", ToUtf8(c.peek.hotkey)},
        {"mode", EnumToStr(kTriggerModes, c.peek.mode)},
        {"trayClick", c.peek.trayClick},
        {"hotCorner", c.peek.hotCornerEnabled},
        {"corner", EnumToStr(kCorners, c.peek.corner)},
        {"dimPercent", c.peek.dimPercent},
    };
    j["desktopDisplay"] = {
        {"enabled", c.desktopDisplay.enabled},
        {"interceptWinD", c.desktopDisplay.interceptWinD},
        {"hotkey", ToUtf8(c.desktopDisplay.hotkey)},
        {"mode", EnumToStr(kTriggerModes, c.desktopDisplay.mode)},
    };
    j["pages"] = {{"enabled", c.pages.enabled}, {"edgeSwipe", c.pages.edgeSwipe}};
    j["layout"] = {
        {"snapToOthers", c.layout.snapToOthers}, {"snapToEdges", c.layout.snapToEdges}, {"snapToGrid", c.layout.snapToGrid},
        {"gridSize", c.layout.gridSize},         {"spacing", c.layout.spacing},         {"perMonitorConfig", c.layout.perMonitorConfig},
        {"dailyBackups", c.layout.dailyBackups},
    };
    j["lock"] = {{"layoutLocked", c.lock.layoutLocked}, {"contentLocked", c.lock.contentLocked}};
    j["portals"] = {{"defaultView", EnumToStr(kViewModes, c.portals.defaultView)}, {"allowNavigation", c.portals.allowNavigation}};
    json layouts = json::object();
    for (const auto& [key, layout] : c.layouts) layouts[key] = ToJson(layout);
    j["layouts"] = layouts;
    json rules = json::array();
    for (const Rule& r : c.rules) rules.push_back(ToJson(r));
    j["rules"] = rules;
    return j;
}

Config ConfigFromJson(const json& j) {
    Config c;
    json g = j.value("general", json::object());
    c.general.firstRunDone = Get<bool>(g, "firstRunDone", false);
    c.general.hostMode = EnumFromStr(kHostModes, g, "hostMode", HostMode::OwnedByDesktop);
    c.general.showLooseIcons = Get<bool>(g, "showLooseIcons", true);
    c.general.language = GetW(g, "language");

    json ap = j.value("appearance", json::object());
    AppearanceSettings defAp;
    c.appearance.background = GetColor(ap, "background").value_or(defAp.background);
    c.appearance.darkenTitle = Get<bool>(ap, "darkenTitle", defAp.darkenTitle);
    c.appearance.cornerRadius = std::clamp(Get<int>(ap, "cornerRadius", defAp.cornerRadius), 0, 24);
    c.appearance.iconSize = std::clamp(Get<int>(ap, "iconSize", defAp.iconSize), 16, 256);
    c.appearance.titleFontSize = std::clamp(Get<int>(ap, "titleFontSize", defAp.titleFontSize), 8, 28);
    c.appearance.iconTint = GetColor(ap, "iconTint");
    c.appearance.wallpaperAware = Get<bool>(ap, "wallpaperAware", false);
    c.appearance.tabStyle = EnumFromStr(kTabStyles, ap, "tabStyle", TabStyle::Flat);
    c.appearance.tabColors = Get<bool>(ap, "tabColors", true);

    json ch = j.value("chameleon", json::object());
    c.chameleon.enabled = Get<bool>(ch, "enabled", false);
    c.chameleon.opacity = std::clamp(Get<int>(ch, "opacity", 35), 5, 100);
    c.chameleon.desaturate = Get<bool>(ch, "desaturate", true);

    json hd = j.value("hide", json::object());
    c.hide.enabled = Get<bool>(hd, "enabled", true);
    c.hide.hidden = Get<bool>(hd, "hidden", false);
    c.hide.autoHide = Get<bool>(hd, "autoHide", false);
    c.hide.excludeItems = GetWList(hd, "excludeItems");

    json ru = j.value("rollup", json::object());
    c.rollup.openMode = EnumFromStr(kRollupOpen, ru, "openMode", RollupOpen::Hover);
    c.rollup.hoverDelayMs = std::clamp(Get<int>(ru, "hoverDelayMs", 450), 0, 5000);
    c.rollup.autoRollupAtEdges = Get<bool>(ru, "autoRollupAtEdges", true);

    json pk = j.value("peek", json::object());
    c.peek.enabled = Get<bool>(pk, "enabled", true);
    c.peek.hotkey = GetW(pk, "hotkey");
    c.peek.mode = EnumFromStr(kTriggerModes, pk, "mode", TriggerMode::Toggle);
    c.peek.trayClick = Get<bool>(pk, "trayClick", true);
    c.peek.hotCornerEnabled = Get<bool>(pk, "hotCorner", false);
    c.peek.corner = EnumFromStr(kCorners, pk, "corner", HotCorner::TaskbarEnd);
    c.peek.dimPercent = std::clamp(Get<int>(pk, "dimPercent", 45), 0, 90);

    json dd = j.value("desktopDisplay", json::object());
    c.desktopDisplay.enabled = Get<bool>(dd, "enabled", false);
    c.desktopDisplay.interceptWinD = Get<bool>(dd, "interceptWinD", false);
    c.desktopDisplay.hotkey = GetW(dd, "hotkey");
    c.desktopDisplay.mode = EnumFromStr(kTriggerModes, dd, "mode", TriggerMode::Toggle);

    json pg = j.value("pages", json::object());
    c.pages.enabled = Get<bool>(pg, "enabled", true);
    c.pages.edgeSwipe = Get<bool>(pg, "edgeSwipe", true);

    json ly = j.value("layout", json::object());
    c.layout.snapToOthers = Get<bool>(ly, "snapToOthers", true);
    c.layout.snapToEdges = Get<bool>(ly, "snapToEdges", true);
    c.layout.snapToGrid = Get<bool>(ly, "snapToGrid", false);
    c.layout.gridSize = std::clamp(Get<int>(ly, "gridSize", 16), 4, 128);
    c.layout.spacing = std::clamp(Get<int>(ly, "spacing", 10), 0, 64);
    c.layout.perMonitorConfig = Get<bool>(ly, "perMonitorConfig", true);
    c.layout.dailyBackups = Get<bool>(ly, "dailyBackups", true);

    json lk = j.value("lock", json::object());
    c.lock.layoutLocked = Get<bool>(lk, "layoutLocked", false);
    c.lock.contentLocked = Get<bool>(lk, "contentLocked", false);

    json po = j.value("portals", json::object());
    c.portals.defaultView = EnumFromStr(kViewModes, po, "defaultView", ViewMode::Icons);
    c.portals.allowNavigation = Get<bool>(po, "allowNavigation", true);

    if (auto it = j.find("layouts"); it != j.end() && it->is_object())
        for (auto e = it->begin(); e != it->end(); ++e) c.layouts[e.key()] = LayoutFromJson(e.value());
    if (auto it = j.find("rules"); it != j.end() && it->is_array())
        for (const auto& r : *it) c.rules.push_back(RuleFromJson(r));
    return c;
}

}  // namespace

std::string SerializeConfig(const Config& config) {
    return ToJson(config).dump(2);
}

std::optional<Config> ParseConfig(std::string_view text, std::string* error) {
    json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        if (error) *error = "invalid JSON";
        return std::nullopt;
    }
    int version = Get<int>(j, "version", 0);
    if (version < 1) {
        // Version 0 had no "version" key and stored a single layout at the top level.
        if (j.contains("pages") && !j.contains("layouts")) {
            json migrated = json::object();
            migrated["layouts"] = {{"default", j}};
            j = migrated;
        }
    }
    try {
        Config c = ConfigFromJson(j);
        c.version = kConfigVersion;
        return c;
    } catch (const json::exception& e) {
        if (error) *error = e.what();
        return std::nullopt;
    }
}

bool WriteFileAtomic(const std::filesystem::path& path, std::string_view contents) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::filesystem::path tmp = path;
    tmp += L".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        out.flush();
        if (!out) return false;
    }
    if (std::filesystem::exists(path, ec)) {
        if (ReplaceFileW(path.c_str(), tmp.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) return true;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

std::optional<std::string> ReadFileText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool SaveConfigFile(const std::filesystem::path& path, const Config& config) {
    return WriteFileAtomic(path, SerializeConfig(config));
}

std::optional<Config> LoadConfigFile(const std::filesystem::path& path, std::string* error) {
    auto text = ReadFileText(path);
    if (!text) {
        if (error) *error = "cannot read file";
        return std::nullopt;
    }
    return ParseConfig(*text, error);
}

std::string SerializeLayout(const Layout& layout) {
    return ToJson(layout).dump(2);
}

std::optional<Layout> ParseLayout(std::string_view text) {
    json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    try {
        return LayoutFromJson(j);
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

}  // namespace of
