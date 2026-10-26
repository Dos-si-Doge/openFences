// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Geometry.h"

namespace of {

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    bool operator==(const Color&) const = default;

    // "#RRGGBB" or "#RRGGBBAA".
    static std::optional<Color> Parse(std::string_view text);
    std::string ToString() const;
    uint32_t ToColorRef() const { return r | (g << 8) | (b << 16); }
    static Color FromColorRef(uint32_t cr, uint8_t alpha) {
        return {static_cast<uint8_t>(cr & 0xFF), static_cast<uint8_t>((cr >> 8) & 0xFF), static_cast<uint8_t>((cr >> 16) & 0xFF), alpha};
    }
};

enum class TabKind { Items, Portal };
enum class ViewMode { Icons, List, HList };
enum class SortMode { Manual, Name, Type, Date };

struct Tab {
    std::string id;
    std::wstring title;
    std::optional<Color> color;
    TabKind kind = TabKind::Items;
    std::vector<std::wstring> items;  // desktop item keys, in manual order (Items tabs)
    std::wstring path;                // folder shown by a Portal tab (may contain %VARS%)
    ViewMode view = ViewMode::Icons;
    SortMode sort = SortMode::Manual;
    int iconSize = 0;                 // 0 = use the global icon size
    bool allowNavigation = true;      // portals: open subfolders inside the portal
};

struct Group {
    std::string id;
    std::wstring monitor;  // monitor device id; empty = primary
    RectF rect;            // fraction of the monitor work area
    bool rolledUp = false;
    bool locked = false;         // position/size locked
    bool contentLocked = false;  // items cannot be added or removed
    bool hideExclude = false;    // stays visible during Quick-Hide
    int activeTab = 0;
    std::vector<Tab> tabs;
    std::optional<Color> background;
    std::optional<Color> iconTint;

    Tab* ActiveTab() { return tabs.empty() ? nullptr : &tabs[static_cast<size_t>(std::clamp(activeTab, 0, static_cast<int>(tabs.size()) - 1))]; }
    const Tab* ActiveTab() const { return const_cast<Group*>(this)->ActiveTab(); }
};

struct Page {
    std::string id;
    std::wstring name;
    std::vector<Group> groups;
};

struct LoosePos {
    std::wstring monitor;
    int col = 0;
    int row = 0;
    bool operator==(const LoosePos&) const = default;
};

// Everything that depends on one monitor arrangement.
struct Layout {
    std::vector<std::wstring> monitors;  // ids of the monitors this layout was built for
    std::vector<Page> pages;
    std::vector<Group> persistent;       // groups shown on every page
    int currentPage = 0;
    std::map<std::wstring, LoosePos> loose;  // manual desktop-grid positions of unfenced icons
};

// ---- rules ----
enum class MatchMode { All, Any };
enum class ItemKindFilter { Any, File, Folder, Shortcut };
enum class RuleAction { AssignToTab, MoveToFolder };

struct RuleCriteria {
    std::vector<std::wstring> extensions;  // ".png"
    std::wstring nameGlob;                 // "*invoice*"
    std::wstring nameRegex;                // ECMAScript, case-insensitive
    std::wstring targetPrefix;             // shortcut target path prefix
    ItemKindFilter kind = ItemKindFilter::Any;
    std::optional<int> modifiedOlderThanDays;
    std::optional<int> modifiedNewerThanDays;
    std::optional<int> createdOlderThanDays;
    std::optional<int> createdNewerThanDays;
};

struct Rule {
    std::string id;
    std::wstring name;
    bool enabled = true;
    MatchMode mode = MatchMode::All;
    RuleCriteria match;
    RuleAction action = RuleAction::AssignToTab;
    std::string targetTab;       // tab id
    std::wstring targetFolder;   // MoveToFolder destination (may contain %VARS%)
};

// ---- settings ----
enum class TabStyle { Flat, Segmented, Rounded, Hidden };
enum class RollupOpen { Hover, HoverDelay, Click };
enum class TriggerMode { Toggle, Hold };
enum class HotCorner { TaskbarEnd, TopLeft, TopRight, BottomLeft, BottomRight };
enum class HostMode { OwnedByDesktop, ChildOfDesktop };

struct GeneralSettings {
    bool firstRunDone = false;
    HostMode hostMode = HostMode::OwnedByDesktop;
    bool showLooseIcons = true;
    std::wstring language;  // empty = system
    std::wstring lastSeenVersion;  // version that last ran; a change opens "What's new"
    bool showWhatsNew = true;      // open the Welcome window on "What's new" after an update
};

struct AppearanceSettings {
    Color background{24, 24, 28, 120};
    bool darkenTitle = true;
    int cornerRadius = 6;
    int iconSize = 48;       // DIPs
    int titleFontSize = 12;  // DIPs
    std::optional<Color> iconTint;
    bool wallpaperAware = false;
    TabStyle tabStyle = TabStyle::Flat;
    bool tabColors = true;
};

struct ChameleonSettings {
    bool enabled = false;
    int opacity = 35;  // percent while not hovered
    bool desaturate = true;
};

struct HideSettings {
    bool enabled = true;   // double-click desktop toggles
    bool hidden = false;   // current state (persists across restarts)
    bool autoHide = false; // hide while the desktop is not in use
    std::vector<std::wstring> excludeItems;  // loose desktop items that stay visible
};

struct RollupSettings {
    RollupOpen openMode = RollupOpen::Hover;
    int hoverDelayMs = 450;
    bool autoRollupAtEdges = true;
};

struct PeekSettings {
    bool enabled = true;
    std::wstring hotkey;  // empty = automatic default
    TriggerMode mode = TriggerMode::Toggle;
    bool trayClick = true;
    bool hotCornerEnabled = false;
    HotCorner corner = HotCorner::TaskbarEnd;
    int dimPercent = 45;
};

struct DesktopDisplaySettings {
    bool enabled = false;
    bool interceptWinD = false;
    std::wstring hotkey;
    TriggerMode mode = TriggerMode::Toggle;
};

struct PagesSettings {
    bool enabled = true;
    bool edgeSwipe = true;
};

struct LayoutSettings {
    bool snapToOthers = true;
    bool snapToEdges = true;
    bool snapToGrid = false;
    int gridSize = 16;
    int spacing = 10;
    bool perMonitorConfig = true;
    bool dailyBackups = true;
};

struct LockSettings {
    bool layoutLocked = false;
    bool contentLocked = false;
};

struct PortalSettings {
    ViewMode defaultView = ViewMode::Icons;
    bool allowNavigation = true;
};

constexpr int kConfigVersion = 1;

struct Config {
    int version = kConfigVersion;
    GeneralSettings general;
    AppearanceSettings appearance;
    ChameleonSettings chameleon;
    HideSettings hide;
    RollupSettings rollup;
    PeekSettings peek;
    DesktopDisplaySettings desktopDisplay;
    PagesSettings pages;
    LayoutSettings layout;
    LockSettings lock;
    PortalSettings portals;
    std::map<std::string, Layout> layouts;  // keyed by monitor-configuration hash
    std::vector<Rule> rules;
};

// ---- helpers over the model ----
Page& EnsurePage(Layout& layout);
// Groups visible on the current page: persistent ones plus the page's own.
std::vector<Group*> VisibleGroups(Layout& layout);
std::vector<Group*> AllGroups(Layout& layout);
Group* FindGroup(Layout& layout, const std::string& groupId);
// Finds the group and tab holding a tab id anywhere in the layout.
std::pair<Group*, Tab*> FindTab(Layout& layout, const std::string& tabId);
// Finds the items tab that holds the given desktop item key (any page).
std::pair<Group*, Tab*> FindItemOwner(Layout& layout, const std::wstring& key);
// Removes the key from every items tab.
void RemoveItemEverywhere(Layout& layout, const std::wstring& key);
// Removes a group from the layout (pages or persistent list). Returns true if found.
bool RemoveGroup(Layout& layout, const std::string& groupId);
// Detaches a group (by value) from wherever it lives.
std::optional<Group> TakeGroup(Layout& layout, const std::string& groupId);
// All tab titles (for pickers), as pairs of (tab id, "Group / Tab").
std::vector<std::pair<std::string, std::wstring>> ListTabs(Layout& layout);

// Whether a loose (unfenced) desktop icon is shown and clickable. During Quick-Hide only icons excluded from
// hiding stay visible.
bool LooseItemVisible(bool showLooseIcons, bool quickHidden, const std::wstring& key, const std::vector<std::wstring>& excludeItems);

Group MakeGroup(const std::wstring& title, const RectF& rect, const std::wstring& monitor);
Group MakePortalGroup(const std::wstring& title, const std::wstring& path, const RectF& rect, const std::wstring& monitor, ViewMode view);

}  // namespace of
