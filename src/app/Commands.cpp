// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

// Menus and user-facing commands of the Controller.
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <set>

#include "app/Controller.h"
#include "app/Tray.h"
#include "core/Autostart.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Rules.h"
#include "core/Snapshots.h"
#include "core/Strings.h"
#include "core/Templates.h"
#include "shell/ContextMenu.h"
#include "shell/DragDrop.h"
#include "shell/FileOps.h"
#include "ui/DesktopCanvas.h"
#include "ui/FenceWindow.h"
#include "ui/InlineEditor.h"
#include "ui/settings/Form.h"
#include "ui/settings/SettingsWindow.h"
#include "ui/welcome/WelcomeWindow.h"

namespace of {

namespace {

enum : UINT {
    // fence menu
    kRenameFence = kFirstAppMenuId,
    kAddTab,
    kAddPortalTab,
    kRemoveTab,
    kIconDefault,
    kIcon32,
    kIcon48,
    kIcon64,
    kIcon96,
    kSortManual,
    kSortName,
    kSortType,
    kSortDate,
    kViewIcons,
    kViewList,
    kViewHList,
    kAllowNav,
    kOpenFolder,
    kRefreshPortal,
    kRollup,
    kColorDefault,
    kColorCustom,
    kTabColorNone,
    kTabColorCustom,
    kTintGlobal,
    kTintNone,
    kTintCustom,
    kHideExclude,
    kPersistent,
    kLockPosition,
    kLockContent,
    kPaste,
    kSettings,
    kDeleteFence,
    kSelectAll,
    kColorPreset = kFirstAppMenuId + 0x100,
    kTabColorPreset = kFirstAppMenuId + 0x120,
    // desktop menu
    kNewFence = kFirstAppMenuId + 0x200,
    kNewPortal,
    kRefresh,
    kArrange,
    kNewPage,
    kRenamePage,
    kDeletePage,
    kPeek,
    kDesktopDisplay,
    kHide,
    kLock,
    kApplyRules,
    kSnapshot,
    kAutostart,
    kWelcome,
    kExit,
    kPageBase = kFirstAppMenuId + 0x300,
    // item menu
    kRemoveFromFence = kFirstAppMenuId + 0x400,
    kExcludeItem,
    kReveal,
    kMoveToTab = kFirstAppMenuId + 0x500,
    // create menu
    kCreateFence = kFirstAppMenuId + 0x700,
    kCreateWithSelection,
    kCreatePortal,
};

struct Preset {
    const wchar_t* name;
    Color color;
};
constexpr Preset kPresets[] = {
    {L"Charcoal", {24, 24, 28, 130}}, {L"Ocean", {28, 70, 140, 140}},  {L"Teal", {18, 108, 110, 140}},  {L"Forest", {38, 104, 52, 140}},
    {L"Violet", {88, 50, 130, 140}},  {L"Crimson", {140, 36, 50, 140}}, {L"Amber", {170, 96, 20, 140}}, {L"Glass", {255, 255, 255, 36}},
};

class Menu {
public:
    Menu() : m_h(CreatePopupMenu()) {}
    ~Menu() {
        if (m_h && m_own) DestroyMenu(m_h);
    }
    HMENU Handle() const { return m_h; }
    HMENU Release() {
        m_own = false;
        return m_h;
    }
    void Add(UINT id, const std::wstring& text, bool checked = false, bool enabled = true) {
        AppendMenuW(m_h, MF_STRING | (checked ? MF_CHECKED : 0) | (enabled ? 0 : MF_GRAYED), id, text.c_str());
    }
    void Radio(UINT id, const std::wstring& text, bool checked, bool enabled = true) {
        Add(id, text, false, enabled);
        if (checked) CheckMenuRadioItem(m_h, id, id, id, MF_BYCOMMAND);
    }
    void Sep() { AppendMenuW(m_h, MF_SEPARATOR, 0, nullptr); }
    void Sub(const std::wstring& text, Menu& sub, bool enabled = true) {
        AppendMenuW(m_h, MF_POPUP | (enabled ? 0 : MF_GRAYED), reinterpret_cast<UINT_PTR>(sub.Release()), text.c_str());
    }
    int Count() const { return GetMenuItemCount(m_h); }

private:
    HMENU m_h;
    bool m_own = true;
};

std::optional<Color> PickColor(HWND owner, const Color& initial) {
    static COLORREF custom[16] = {};
    CHOOSECOLORW cc{sizeof(cc)};
    cc.hwndOwner = owner;
    cc.rgbResult = initial.ToColorRef();
    cc.lpCustColors = custom;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
    if (!ChooseColorW(&cc)) return std::nullopt;
    return Color::FromColorRef(cc.rgbResult, initial.a);
}

}  // namespace

// ====================================================================== fence menu

void Controller::ShowFenceMenu(FenceWindow* fence, POINT pt) {
    if (!fence) return;
    std::string groupId = fence->GroupId();
    Layout& layout = CurrentLayout();
    Group* g = FindGroup(layout, groupId);
    if (!g) return;
    Tab* t = g->ActiveTab();
    bool locked = IsLayoutLocked(g);
    bool portal = t && t->kind == TabKind::Portal;
    bool persistent = std::any_of(layout.persistent.begin(), layout.persistent.end(), [&](const Group& p) { return p.id == groupId; });

    Menu menu;
    menu.Add(kRenameFence, g->tabs.size() > 1 ? L"Rename tab" : L"Rename fence", false, !locked);
    Menu tabs;
    tabs.Add(kAddTab, L"Add tab", false, !locked);
    tabs.Add(kAddPortalTab, L"Add folder portal tab…", false, !locked);
    tabs.Add(kRemoveTab, L"Move this tab to its own fence", false, !locked && g->tabs.size() > 1);
    menu.Sub(L"Tabs", tabs);
    menu.Sep();

    Menu view;
    int icon = t ? t->iconSize : 0;
    view.Radio(kIconDefault, L"Default icon size", icon == 0);
    view.Radio(kIcon32, L"Small icons", icon == 32);
    view.Radio(kIcon48, L"Medium icons", icon == 48);
    view.Radio(kIcon64, L"Large icons", icon == 64);
    view.Radio(kIcon96, L"Extra large icons", icon == 96);
    view.Sep();
    SortMode sort = t ? t->sort : SortMode::Manual;
    view.Radio(kSortManual, L"Sort: custom order", sort == SortMode::Manual);
    view.Radio(kSortName, L"Sort by name", sort == SortMode::Name);
    view.Radio(kSortType, L"Sort by type", sort == SortMode::Type);
    view.Radio(kSortDate, L"Sort by date modified", sort == SortMode::Date);
    if (portal) {
        view.Sep();
        view.Radio(kViewIcons, L"Icons", t->view == ViewMode::Icons);
        view.Radio(kViewList, L"List", t->view == ViewMode::List);
        view.Radio(kViewHList, L"Horizontal list", t->view == ViewMode::HList);
    }
    menu.Sub(L"View", view);
    if (portal) {
        menu.Add(kAllowNav, L"Allow navigation", t->allowNavigation);
        menu.Add(kOpenFolder, L"Open folder in Explorer");
        menu.Add(kRefreshPortal, L"Refresh");
    }
    menu.Add(kRollup, L"Roll up", g->rolledUp);
    menu.Sep();

    Menu colors;
    colors.Radio(kColorDefault, L"Default", !g->background);
    for (UINT i = 0; i < std::size(kPresets); ++i) colors.Radio(kColorPreset + i, kPresets[i].name, g->background == kPresets[i].color);
    colors.Add(kColorCustom, L"Custom…");
    menu.Sub(L"Color", colors, !locked);
    if (g->tabs.size() > 1) {
        Menu tabColors;
        tabColors.Radio(kTabColorNone, L"None", !t->color);
        for (UINT i = 0; i < std::size(kPresets) - 1; ++i) {
            Color c = kPresets[i].color;
            c.a = 255;
            tabColors.Radio(kTabColorPreset + i, kPresets[i].name, t->color == c);
        }
        tabColors.Add(kTabColorCustom, L"Custom…");
        menu.Sub(L"Tab color", tabColors, !locked);
    }
    Menu tint;
    tint.Radio(kTintGlobal, L"Use global setting", !g->iconTint);
    tint.Add(kTintCustom, L"Tint icons in this fence…", g->iconTint.has_value());
    menu.Sub(L"Icon tint", tint);
    menu.Sep();
    menu.Add(kHideExclude, L"Keep visible during Quick-Hide", g->hideExclude);
    menu.Add(kPersistent, L"Show on all pages", persistent, m_config.pages.enabled);
    menu.Add(kLockPosition, L"Lock position and size", g->locked, !m_policy.lockLayout.has_value());
    menu.Add(kLockContent, L"Lock contents", g->contentLocked, !m_policy.lockContent.has_value());
    menu.Sep();
    menu.Add(kSelectAll, L"Select all");
    menu.Add(kPaste, L"Paste", false, IsClipboardFormatAvailable(CF_HDROP) && !IsContentLocked(g));
    menu.Add(kSettings, L"Configure openFences…");
    menu.Sep();
    menu.Add(kDeleteFence, L"Delete fence", false, !locked);

    UINT cmd = TrackAppMenu(fence->Hwnd(), menu.Handle(), pt);
    // The model may have changed while the menu was open.
    g = FindGroup(CurrentLayout(), groupId);
    if (!g || cmd == 0) return;
    t = g->ActiveTab();
    auto setIcon = [&](int size) {
        if (t) t->iconSize = size;
    };
    auto setSort = [&](SortMode m) {
        if (t) t->sort = m;
    };
    auto setView = [&](ViewMode m) {
        if (t) t->view = m;
    };
    switch (cmd) {
        case kRenameFence: fence->BeginTitleRename(); return;
        case kAddTab: {
            Tab nt;
            nt.id = NewId("t");
            nt.title = L"Tab " + std::to_wstring(g->tabs.size() + 1);
            g->tabs.push_back(nt);
            g->activeTab = static_cast<int>(g->tabs.size()) - 1;
            RefreshAll();
            if (FenceWindow* f = FenceFor(groupId)) f->BeginTitleRename();
            break;
        }
        case kAddPortalTab: {
            std::wstring path = PickFolder(m_msgWnd, L"Choose the folder this tab shows");
            if (path.empty()) return;
            Tab nt;
            nt.id = NewId("t");
            nt.kind = TabKind::Portal;
            nt.path = path;
            nt.title = PathFindFileNameW(path.c_str());
            nt.view = m_config.portals.defaultView;
            nt.allowNavigation = m_config.portals.allowNavigation;
            g->tabs.push_back(nt);
            g->activeTab = static_cast<int>(g->tabs.size()) - 1;
            RefreshAll();
            break;
        }
        case kRemoveTab: {
            RectI r = GroupScreenRect(*g).Offset(30, 30);
            DetachTab(groupId, g->activeTab, r);
            return;
        }
        case kIconDefault: setIcon(0); break;
        case kIcon32: setIcon(32); break;
        case kIcon48: setIcon(48); break;
        case kIcon64: setIcon(64); break;
        case kIcon96: setIcon(96); break;
        case kSortManual: setSort(SortMode::Manual); break;
        case kSortName: setSort(SortMode::Name); break;
        case kSortType: setSort(SortMode::Type); break;
        case kSortDate: setSort(SortMode::Date); break;
        case kViewIcons: setView(ViewMode::Icons); break;
        case kViewList: setView(ViewMode::List); break;
        case kViewHList: setView(ViewMode::HList); break;
        case kAllowNav:
            if (t) t->allowNavigation = !t->allowNavigation;
            break;
        case kOpenFolder:
            if (PortalModel* pm = t ? Portal(t->id) : nullptr; pm && !pm->stack.empty()) OpenInExplorer(pm->stack.back().get());
            return;
        case kRefreshPortal:
            if (t) PortalRefresh(t->id);
            return;
        case kRollup: g->rolledUp = !g->rolledUp; break;
        case kColorDefault: g->background.reset(); break;
        case kColorCustom:
            if (auto c = PickColor(m_msgWnd, g->background.value_or(m_config.appearance.background))) g->background = c;
            break;
        case kTabColorNone:
            if (t) t->color.reset();
            break;
        case kTabColorCustom:
            if (t)
                if (auto c = PickColor(m_msgWnd, t->color.value_or(Color{60, 120, 220, 255}))) t->color = c;
            break;
        case kTintGlobal: g->iconTint.reset(); break;
        case kTintCustom:
            if (auto c = PickColor(m_msgWnd, g->iconTint.value_or(Color{120, 170, 255, 255}))) g->iconTint = c;
            break;
        case kHideExclude: g->hideExclude = !g->hideExclude; break;
        case kPersistent: TogglePersistent(groupId); return;
        case kLockPosition: g->locked = !g->locked; break;
        case kLockContent: g->contentLocked = !g->contentLocked; break;
        case kSelectAll: SendMessageW(fence->Hwnd(), WM_KEYDOWN, 'A', 0); return;
        case kPaste: {
            ComPtr<IDataObject> data;
            if (t && t->kind == TabKind::Portal) {
                if (PortalModel* pm = Portal(t->id); pm && !pm->stack.empty()) PasteIntoFolder(fence->Hwnd(), pm->stack.back().get());
            } else if (t) {
                if (SUCCEEDED(OleGetClipboard(&data))) ExpectDroppedFiles(DroppedFileNames(data.Get()), t->id, L"", {});
                if (auto desk = ParsePath(m_desktopPath)) PasteIntoFolder(fence->Hwnd(), desk.get());
            }
            return;
        }
        case kSettings: OpenSettings(); return;
        case kDeleteFence: DeleteGroup(groupId, true); return;
        default:
            if (cmd >= kColorPreset && cmd < kColorPreset + std::size(kPresets)) {
                g->background = kPresets[cmd - kColorPreset].color;
            } else if (cmd >= kTabColorPreset && cmd < kTabColorPreset + std::size(kPresets) && t) {
                Color c = kPresets[cmd - kTabColorPreset].color;
                c.a = 255;
                t->color = c;
            }
            break;
    }
    if (FenceWindow* f = FenceFor(groupId)) f->SyncFromModel();
    LayoutLooseItems();
    SaveSoon();
}

// ====================================================================== item menu

void Controller::ShowItemMenu(HWND owner, const std::vector<ShellItemPtr>& items, POINT pt, const std::string& tabId, bool desktopItems,
                              const RectI& firstLabelScreen, float fontPx) {
    if (items.empty()) return;
    Layout& layout = CurrentLayout();
    auto [srcGroup, srcTab] = FindTab(layout, tabId);
    bool contentLocked = srcGroup && IsContentLocked(srcGroup);
    Menu menu;
    std::vector<std::pair<std::string, std::wstring>> tabs;
    if (desktopItems) {
        if (!tabId.empty()) menu.Add(kRemoveFromFence, L"Remove from fence", false, !contentLocked);
        tabs = ListTabs(layout);
        Menu moveTo;
        for (size_t i = 0; i < tabs.size() && i < 200; ++i)
            moveTo.Add(kMoveToTab + static_cast<UINT>(i), tabs[i].second, tabs[i].first == tabId, tabs[i].first != tabId && !contentLocked);
        if (!tabs.empty()) menu.Sub(L"Move to fence", moveTo);
        if (tabId.empty() && items.size() == 1) {
            auto& ex = m_config.hide.excludeItems;
            bool excluded = std::find(ex.begin(), ex.end(), items[0]->key) != ex.end();
            menu.Add(kExcludeItem, L"Keep visible during Quick-Hide", excluded);
        }
    } else {
        menu.Add(kReveal, L"Show in Explorer");
    }
    if (menu.Count() > 0) menu.Sep();
    ComPtr<IContextMenu> cm = ItemContextMenu(owner, items);
    ShellMenuResult r = TrackShellMenu(owner, cm.Get(), menu.Handle(), static_cast<UINT>(menu.Count()), pt, CMF_NORMAL | CMF_CANRENAME);
    if (r.renameRequested) {
        RenameItemInteractive(owner, items[0], firstLabelScreen, fontPx);
        return;
    }
    std::vector<std::wstring> keys;
    for (const auto& i : items) keys.push_back(i->key);
    switch (r.appId) {
        case 0: return;
        case kRemoveFromFence: MakeLoose(keys, L"", {}); return;
        case kReveal: RevealInExplorer(*items[0]); return;
        case kExcludeItem: {
            auto& ex = m_config.hide.excludeItems;
            auto it = std::find(ex.begin(), ex.end(), keys[0]);
            if (it != ex.end()) ex.erase(it);
            else ex.push_back(keys[0]);
            SaveSoon();
            InvalidateAll();
            return;
        }
        default:
            if (r.appId >= kMoveToTab && r.appId < kMoveToTab + tabs.size()) AssignItems(keys, tabs[r.appId - kMoveToTab].first, -1);
            return;
    }
}

// ====================================================================== desktop & create menus

void Controller::ShowDesktopMenu(DesktopCanvas* canvas, POINT pt) {
    if (!canvas) return;
    bool locked = IsLayoutLocked(nullptr);
    Layout& layout = CurrentLayout();
    Menu menu;
    menu.Add(kNewFence, L"New fence here", false, !locked);
    menu.Add(kNewPortal, L"New folder portal here…", false, !locked);
    menu.Sep();
    menu.Add(kPaste, L"Paste", false, IsClipboardFormatAvailable(CF_HDROP));
    menu.Add(kRefresh, L"Refresh");
    menu.Add(kArrange, L"Arrange desktop icons");
    if (m_config.pages.enabled) {
        Menu pages;
        for (size_t i = 0; i < layout.pages.size() && i < 64; ++i)
            pages.Radio(kPageBase + static_cast<UINT>(i), layout.pages[i].name, static_cast<int>(i) == layout.currentPage);
        pages.Sep();
        pages.Add(kNewPage, L"New page");
        pages.Add(kRenamePage, L"Rename this page…");
        pages.Add(kDeletePage, L"Delete this page", false, layout.pages.size() > 1);
        menu.Sub(L"Pages", pages);
    }
    menu.Sep();
    menu.Add(kPeek, L"Peek (show fences above windows)", false, m_config.peek.enabled);
    menu.Add(kHide, L"Hide fences and icons", m_config.hide.hidden);
    menu.Add(kLock, L"Lock fences", IsLayoutLocked(nullptr), !m_policy.lockLayout.has_value());
    menu.Add(kApplyRules, L"Sort desktop with rules now", false, !m_config.rules.empty());
    menu.Sep();
    UINT shellPos = static_cast<UINT>(menu.Count());
    menu.Sep();
    menu.Add(kSettings, L"Configure openFences…", false, !m_policy.disableSettings);

    ITEMIDLIST root{};
    ComPtr<IContextMenu> cm = BackgroundContextMenu(canvas->Hwnd(), &root);
    ShellMenuResult r = TrackShellMenu(canvas->Hwnd(), cm.Get(), menu.Handle(), shellPos, pt, CMF_NORMAL);
    switch (r.appId) {
        case 0: return;
        case kNewFence:
        case kNewPortal: {
            const MonitorDesc* mon = MonitorForRect(m_monitors, RectI::FromXYWH(pt.x, pt.y, 1, 1));
            int dpi = mon ? static_cast<int>(mon->dpi) : 96;
            RectI rect = RectI::FromXYWH(pt.x, pt.y, MulDiv(300, dpi, 96), MulDiv(220, dpi, 96));
            CreateGroup(rect, r.appId == kNewPortal);
            return;
        }
        case kPaste:
            if (auto desk = ParsePath(m_desktopPath)) {
                ComPtr<IDataObject> data;
                if (SUCCEEDED(OleGetClipboard(&data))) ExpectDroppedFiles(DroppedFileNames(data.Get()), {}, canvas->Monitor().id, canvas->CellAtScreen(pt));
                PasteIntoFolder(canvas->Hwnd(), desk.get());
            }
            return;
        case kRefresh: ExecuteCommand(L"refresh", L""); return;
        case kArrange:
            CurrentLayout().loose.clear();
            LayoutLooseItems();
            SaveSoon();
            return;
        case kNewPage: AddPage(); return;
        case kRenamePage: {
            Layout& l = CurrentLayout();
            if (auto name = PromptText(m_msgWnd, L"Rename page", L"Page name:", l.pages[static_cast<size_t>(l.currentPage)].name))
                RenameCurrentPage(*name);
            return;
        }
        case kDeletePage: DeleteCurrentPage(); return;
        case kPeek: TogglePeek(PeekKind::Peek); return;
        case kHide: ToggleHide(); return;
        case kLock: ToggleLock(); return;
        case kApplyRules: ApplyRulesNow(); return;
        case kSettings: OpenSettings(); return;
        default:
            if (r.appId >= kPageBase && r.appId < kPageBase + 64) {
                int idx = static_cast<int>(r.appId - kPageBase);
                GoToPage(idx, idx > CurrentLayout().currentPage ? -1 : 1);
            }
            return;
    }
}

void Controller::ShowCreateMenu(DesktopCanvas* canvas, const RectI& band, const std::vector<std::wstring>& selectedKeys, POINT pt) {
    Menu menu;
    menu.Add(kCreateFence, L"Create fence here");
    if (!selectedKeys.empty())
        menu.Add(kCreateWithSelection, L"Create fence with the " + std::to_wstring(selectedKeys.size()) + L" selected icon(s)");
    menu.Add(kCreatePortal, L"Create folder portal here…");
    menu.Sep();
    menu.Add(0xFFFF, L"Cancel");
    UINT cmd = TrackAppMenu(canvas->Hwnd(), menu.Handle(), pt);
    if (cmd == kCreateFence) CreateGroup(band, false);
    else if (cmd == kCreateWithSelection) CreateGroup(band, false, selectedKeys);
    else if (cmd == kCreatePortal) CreateGroup(band, true);
}

// ====================================================================== tray

void Controller::OnTrayClick() {
    if (m_config.peek.enabled && m_config.peek.trayClick) TogglePeek(PeekKind::Peek);
    else OpenSettings();
}

void Controller::ShowTrayMenu() {
    Layout& layout = CurrentLayout();
    Menu menu;
    menu.Add(kPeek, L"Peek", m_peek == PeekKind::Peek, m_config.peek.enabled);
    menu.Add(kDesktopDisplay, L"Show desktop", m_peek == PeekKind::DesktopDisplay);
    menu.Add(kHide, L"Hide fences and icons", m_config.hide.hidden);
    if (m_config.pages.enabled && layout.pages.size() > 1) {
        Menu pages;
        for (size_t i = 0; i < layout.pages.size() && i < 64; ++i)
            pages.Radio(kPageBase + static_cast<UINT>(i), layout.pages[i].name, static_cast<int>(i) == layout.currentPage);
        menu.Sub(L"Pages", pages);
    }
    menu.Sep();
    menu.Add(kLock, L"Lock fences", IsLayoutLocked(nullptr), !m_policy.lockLayout.has_value());
    menu.Add(kSnapshot, L"Take snapshot");
    menu.Add(kWelcome, L"Welcome to openFences…");
    menu.Add(kSettings, L"Configure openFences…", false, !m_policy.disableSettings);
    menu.Add(kAutostart, L"Start openFences when I sign in", autostart::Query().Enabled());
    menu.Sep();
    menu.Add(kExit, L"Exit");
    POINT pt;
    GetCursorPos(&pt);
    UINT cmd = TrackAppMenu(m_msgWnd, menu.Handle(), pt);
    switch (cmd) {
        case kPeek: TogglePeek(PeekKind::Peek); break;
        case kDesktopDisplay: TogglePeek(PeekKind::DesktopDisplay); break;
        case kHide: ToggleHide(); break;
        case kLock: ToggleLock(); break;
        case kSnapshot: TakeSnapshot(L"Snapshot"); break;
        case kSettings: OpenSettings(); break;
        case kWelcome: OpenWelcome(); break;
        case kAutostart: autostart::Set(!autostart::Query().Enabled(), paths::ExePath().wstring()); break;
        case kExit: Exit(); break;
        default:
            if (cmd >= kPageBase && cmd < kPageBase + 64) {
                int idx = static_cast<int>(cmd - kPageBase);
                GoToPage(idx, idx > CurrentLayout().currentPage ? -1 : 1);
            }
            break;
    }
}

// ====================================================================== hide / lock / peek

void Controller::ToggleHide() { SetHidden(!m_config.hide.hidden); }

void Controller::SetHidden(bool hidden) {
    if (m_peek != PeekKind::None) EndPeek();
    m_config.hide.hidden = hidden;
    m_autoHidden = false;
    ApplyHiddenState(true);
    SaveSoon();
}

void Controller::ToggleLock() {
    if (m_policy.lockLayout.has_value()) {
        MessageBoxW(m_msgWnd, L"Fence locking is managed by your administrator.", L"openFences", MB_ICONINFORMATION | MB_SETFOREGROUND);
        return;
    }
    m_config.lock.layoutLocked = !m_config.lock.layoutLocked;
    InvalidateAll();
    SaveSoon();
}

void Controller::TogglePeek(PeekKind kind) {
    if (m_peek == kind) EndPeek();
    else StartPeek(kind, false, 0);
}

void Controller::StartPeek(PeekKind kind, bool hold, unsigned holdVk) {
    if (m_peek != PeekKind::None) EndPeek();
    inline_editor::Cancel();
    m_peek = kind;
    m_peekHold = hold;
    m_holdVk = holdVk;
    for (auto& c : m_canvases) {
        c->AllowZOrder(true);
        m_host.RaiseTopmost(c->Hwnd());
        c->AllowZOrder(false);
        c->SetPeek(kind);
        c->SetItemsShown(true, false);
    }
    for (auto& f : m_fences) {
        f->AllowZOrder(true);
        f->SetShown(true, false);
        f->ApplyGeometry();
        m_host.RaiseTopmost(f->Hwnd());
        f->AllowZOrder(false);
        f->Invalidate();
    }
    if (const MonitorDesc* primary = PrimaryMonitor(m_monitors)) {
        for (auto& c : m_canvases)
            if (EqualsI(c->Monitor().id, primary->id)) SetForegroundWindow(c->Hwnd());
    }
    if (hold) SetTimer(m_msgWnd, kTimerHold, 30, nullptr);
}

void Controller::EndPeek() {
    if (m_peek == PeekKind::None) return;
    m_peek = PeekKind::None;
    KillTimer(m_msgWnd, kTimerHold);
    for (auto& c : m_canvases) {
        c->AllowZOrder(true);
        m_host.Lower(c->Hwnd());
        c->AllowZOrder(false);
        c->SetPeek(PeekKind::None);
    }
    for (auto& f : m_fences) {
        f->AllowZOrder(true);
        m_host.Lower(f->Hwnd());
        f->AllowZOrder(false);
    }
    RefreshAll();
}

void Controller::OnItemOpened() {
    if (m_peek != PeekKind::None) EndPeek();
}

// ====================================================================== pages

void Controller::FinishPageAnim() {
    if (!m_pageAnim.active && m_pageAnim.outgoing.empty()) return;
    KillTimer(m_msgWnd, kTimerPageAnim);
    m_pageAnim.active = false;
    m_pageAnim.outgoing.clear();
    for (FenceWindow* f : m_pageAnim.incoming) f->ApplyGeometry(0);
    m_pageAnim.incoming.clear();
}

void Controller::GoToPage(int index, int direction) {
    Layout& layout = CurrentLayout();
    if (index < 0 || index >= static_cast<int>(layout.pages.size()) || index == layout.currentPage) {
        for (auto& c : m_canvases) c->SetPageIndicator(false, 0);
        return;
    }
    if (m_peek != PeekKind::None) EndPeek();
    FinishPageAnim();
    std::set<std::string> persistentIds;
    for (const Group& g : layout.persistent) persistentIds.insert(g.id);
    for (auto it = m_fences.begin(); it != m_fences.end();) {
        if (!persistentIds.count((*it)->GroupId())) {
            m_pageAnim.outgoing.push_back(std::move(*it));
            it = m_fences.erase(it);
        } else {
            ++it;
        }
    }
    layout.currentPage = index;
    RefreshAll();
    for (auto& f : m_fences)
        if (!persistentIds.count(f->GroupId())) m_pageAnim.incoming.push_back(f.get());
    for (auto& c : m_canvases) c->SetPageIndicator(true, 0);
    SetTimer(m_msgWnd, kTimerPageIndicator, 1400, nullptr);
    SaveSoon();
    if (direction == 0) {
        FinishPageAnim();
        return;
    }
    m_pageAnim.active = true;
    m_pageAnim.start = GetTickCount64();
    m_pageAnim.direction = direction;
    PageAnimTick();
    SetTimer(m_msgWnd, kTimerPageAnim, 15, nullptr);
}

void Controller::PageAnimTick() {
    if (!m_pageAnim.active) {
        KillTimer(m_msgWnd, kTimerPageAnim);
        return;
    }
    double t = std::min(1.0, (GetTickCount64() - m_pageAnim.start) / 240.0);
    double e = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
    const MonitorDesc* primary = PrimaryMonitor(m_monitors);
    int width = primary ? primary->work.Width() : 1920;
    int d = m_pageAnim.direction;
    for (auto& f : m_pageAnim.outgoing) f->ApplyGeometry(static_cast<int>(d * width * e));
    for (FenceWindow* f : m_pageAnim.incoming) f->ApplyGeometry(static_cast<int>(-d * width * (1.0 - e)));
    if (t >= 1.0) FinishPageAnim();
}

void Controller::NextPage(int delta) {
    GoToPage(CurrentLayout().currentPage + delta, -delta);
}

void Controller::PageSwipeUpdate(int dx, int width) {
    for (auto& c : m_canvases) c->SetPageIndicator(true, static_cast<float>(dx) / std::max(1, width));
}

void Controller::PageSwipeEnd(int dx, int width) {
    int threshold = std::max(40, width * 15 / 100);
    if (dx > threshold) NextPage(-1);       // pulled right from the left edge: previous page
    else if (dx < -threshold) NextPage(1);  // pulled left from the right edge: next page
    SetTimer(m_msgWnd, kTimerPageIndicator, 900, nullptr);
    for (auto& c : m_canvases) c->SetPageIndicator(true, 0);
}

void Controller::AddPage() {
    Layout& layout = CurrentLayout();
    layout.pages.push_back(Page{NewId("p"), L"Page " + std::to_wstring(layout.pages.size() + 1), {}});
    GoToPage(static_cast<int>(layout.pages.size()) - 1, -1);
    SaveSoon();
}

void Controller::DeleteCurrentPage() {
    Layout& layout = CurrentLayout();
    if (layout.pages.size() <= 1) return;
    Page& page = layout.pages[static_cast<size_t>(layout.currentPage)];
    std::wstring msg = L"Delete the page \"" + page.name + L"\" and its " + std::to_wstring(page.groups.size()) +
                       L" fence(s)?\n\nIcons from those fences stay on the desktop.";
    if (MessageBoxW(m_msgWnd, msg.c_str(), L"openFences", MB_OKCANCEL | MB_ICONQUESTION | MB_SETFOREGROUND) != IDOK) return;
    layout.pages.erase(layout.pages.begin() + layout.currentPage);
    layout.currentPage = std::clamp(layout.currentPage - 1, 0, static_cast<int>(layout.pages.size()) - 1);
    RefreshAll();
    SaveSoon();
}

void Controller::RenameCurrentPage(const std::wstring& name) {
    std::wstring n = Trim(name);
    if (n.empty()) return;
    Layout& layout = CurrentLayout();
    layout.pages[static_cast<size_t>(layout.currentPage)].name = n;
    SaveSoon();
}

void Controller::TogglePersistent(const std::string& groupId) {
    Layout& layout = CurrentLayout();
    bool persistent = std::any_of(layout.persistent.begin(), layout.persistent.end(), [&](const Group& g) { return g.id == groupId; });
    std::optional<Group> g = TakeGroup(layout, groupId);
    if (!g) return;
    if (persistent) EnsurePage(layout).groups.push_back(std::move(*g));
    else layout.persistent.push_back(std::move(*g));
    RefreshAll();
    SaveSoon();
}

// ====================================================================== rules, layout, snapshots

void Controller::ApplyRulesNow() {
    std::vector<ShellItemPtr> candidates;
    for (const auto& key : m_itemOrder) candidates.push_back(m_items[key]);
    auto assigned = RulesTargetsApply(candidates);
    log::Info(L"Rules applied: {} item(s) sorted", assigned.size());
    SyncItems();
}

void Controller::AutomaticLayout(bool confirm) {
    if (IsLayoutLocked(nullptr)) return;
    if (confirm &&
        MessageBoxW(m_msgWnd,
                    L"Replace the current layout with the default fences (Programs, Folders, Files) and sort the desktop into them?\n\n"
                    L"A snapshot of the current layout is saved first.",
                    L"openFences", MB_OKCANCEL | MB_ICONQUESTION | MB_SETFOREGROUND) != IDOK)
        return;
    TakeSnapshot(L"Before automatic layout");
    Layout& layout = CurrentLayout();
    std::vector<std::wstring> monitors = layout.monitors;
    layout = Layout{};
    layout.monitors = monitors;
    const MonitorDesc* primary = PrimaryMonitor(m_monitors);
    BuildDefaultLayout(m_config, m_layoutKey, primary ? primary->id : L"");
    FinishPageAnim();
    m_fences.clear();
    ApplyRulesNow();
    RefreshAll();
    SaveSoon();
}

void Controller::TakeSnapshot(const std::wstring& name) {
    auto path = SaveSnapshot(paths::BackupsDir(), m_config, name, CurrentStamp());
    log::Info(L"Snapshot \"{}\" {}", name, path ? path->wstring() : std::wstring(L"failed"));
}

bool Controller::RestoreSnapshot(const std::wstring& path) {
    auto snap = LoadSnapshot(path);
    if (!snap) return false;
    TakeSnapshot(L"Before restore");
    GeneralSettings general = m_config.general;
    m_config = std::move(*snap);
    m_config.general = general;
    FinishPageAnim();
    m_fences.clear();
    SelectLayoutForMonitors();
    RebuildCanvases();
    RefreshAll();
    SettingsChanged();
    DoSave();
    return true;
}

bool Controller::ExportLayout(const std::wstring& path) {
    Config c;
    c.layouts["template"] = CurrentLayout();
    c.layouts["template"].monitors.clear();
    c.rules = m_config.rules;
    c.appearance = m_config.appearance;
    auto j = nlohmann::json::parse(SerializeConfig(c));
    j["template"] = {{"name", "Exported layout"}, {"description", ToUtf8(L"Exported on " + FormatStamp(CurrentStamp()))}};
    return WriteFileAtomic(path, j.dump(2));
}

bool Controller::ImportLayout(const std::wstring& path) { return ApplyTemplateFile(path); }

bool Controller::ApplyTemplateFile(const std::wstring& path) {
    auto tpl = LoadTemplate(path);
    if (!tpl) {
        log::Info(L"Template {} could not be read", path);
        return false;
    }
    TakeSnapshot(L"Before applying " + tpl->name);
    FinishPageAnim();
    m_fences.clear();
    ApplyTemplate(m_config, m_layoutKey, *tpl);
    ApplyRulesNow();
    RefreshAll();
    SettingsChanged();
    return true;
}

// ====================================================================== settings & commands

void Controller::OpenSettings(const std::wstring& page) {
    if (m_policy.disableSettings) {
        MessageBoxW(m_msgWnd, L"openFences settings are disabled by your administrator.", L"openFences", MB_ICONINFORMATION | MB_SETFOREGROUND);
        return;
    }
    if (m_peek != PeekKind::None) EndPeek();
    if (!m_settings) m_settings = std::make_unique<SettingsWindow>();
    m_settings->Show(page);
}

void Controller::OpenWelcome(const std::wstring& page) {
    if (m_peek != PeekKind::None) EndPeek();
    if (!m_welcome) m_welcome = std::make_unique<WelcomeWindow>();
    m_welcome->Show(page);
}

void Controller::SettingsChanged() {
    if (m_host.Mode() != m_config.general.hostMode) {
        m_host.SetMode(m_config.general.hostMode);
        FinishPageAnim();
        m_fences.clear();
        m_canvases.clear();
    }
    std::string oldKey = m_layoutKey;
    SelectLayoutForMonitors();
    if (oldKey != m_layoutKey) {
        FinishPageAnim();
        m_fences.clear();
    }
    RegisterHotkeys();
    UpdateHooks();
    UpdateWallpaperColor();
    RebuildCanvases();
    for (auto& c : m_canvases) c->RefreshStyle();
    if (!m_config.hide.autoHide) m_autoHidden = false;
    RefreshAll();
    SaveSoon();
}

void Controller::ExecuteCommand(const std::wstring& verb, const std::wstring& arg) {
    log::Debug(L"Command: {} {}", verb, arg);
    if (verb == L"settings" || verb == L"activate") OpenSettings(arg);
    else if (verb == L"welcome") OpenWelcome(arg);
    else if (verb == L"toggle-hide") ToggleHide();
    else if (verb == L"hide") SetHidden(true);
    else if (verb == L"show") SetHidden(false);
    else if (verb == L"toggle-lock") ToggleLock();
    else if (verb == L"peek") TogglePeek(PeekKind::Peek);
    else if (verb == L"desktop-display") TogglePeek(PeekKind::DesktopDisplay);
    else if (verb == L"next-page") NextPage(1);
    else if (verb == L"prev-page") NextPage(-1);
    else if (verb == L"page") GoToPage(_wtoi(arg.c_str()) - 1, 0);
    else if (verb == L"apply-rules") ApplyRulesNow();
    else if (verb == L"snapshot") TakeSnapshot(arg.empty() ? L"Snapshot" : arg);
    else if (verb == L"restore") RestoreSnapshot(arg);
    else if (verb == L"export") ExportLayout(arg);
    else if (verb == L"import" || verb == L"apply-template") ApplyTemplateFile(arg);
    else if (verb == L"automatic-layout") AutomaticLayout(true);
    else if (verb == L"shutdown" || verb == L"exit") Exit();
    else if (verb == L"new-fence") {
        POINT pt;
        GetCursorPos(&pt);
        const MonitorDesc* mon = MonitorForRect(m_monitors, RectI::FromXYWH(pt.x, pt.y, 1, 1));
        if (mon) {
            int w = MulDiv(300, static_cast<int>(mon->dpi), 96), h = MulDiv(220, static_cast<int>(mon->dpi), 96);
            CreateGroup(FindFreePlacement(w, h, GroupRectsOnMonitor(mon->id, {}), mon->work, MulDiv(m_config.layout.spacing, static_cast<int>(mon->dpi), 96)),
                        false);
        }
    } else if (verb == L"refresh") {
        m_icons->MarkAllStale();
        RequestDesktopScan();
        for (auto& [id, pm] : m_portals) LoadPortal(*pm);
    } else if (verb == L"desktop-activity") {
        if (m_config.hide.autoHide && m_autoHidden) {
            m_autoHidden = false;
            ApplyHiddenState(true);
        }
        if (m_config.hide.autoHide) SetTimer(m_msgWnd, kTimerAutoHide, 2500, nullptr);
    }
}

}  // namespace of
