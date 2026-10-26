// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

// Page builders and handlers for the settings window.
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>

#include "Version.h"
#include "app/Controller.h"
#include "core/Autostart.h"
#include "core/Config.h"
#include "core/Hotkey.h"
#include "core/Paths.h"
#include "core/Rules.h"
#include "core/Strings.h"
#include "core/Templates.h"
#include "desktop/InputHooks.h"
#include "shell/FileOps.h"
#include "ui/Help.h"
#include "ui/settings/RuleDialog.h"
#include "ui/settings/SettingsWindow.h"

namespace of {

namespace {

enum : int {
    // welcome
    kAutostartId = 100,
    kLooseIcons,
    kHostMode,
    kNewFenceBtn,
    kApplyRulesBtn,
    kAutoLayoutBtn,
    kTourBtn,
    kWhatsNewCheck,
    // chameleon
    kChamEnable = 200,
    kChamOpacity,
    kChamDesat,
    // personalization
    kBgColor = 300,
    kBgAlpha,
    kDarkTitle,
    kCorner,
    kIconSize,
    kTitleSize,
    kWallpaperAware,
    kTintEnable,
    kTintColor,
    // portals
    kNewPortalBtn = 400,
    kPortalView,
    kPortalNav,
    // peek
    kPeekEnable = 500,
    kPeekHotkey,
    kPeekMode,
    kPeekTray,
    kPeekCorner,
    kPeekCornerPos,
    kPeekDim,
    kDdEnable,
    kDdWinD,
    kDdHotkey,
    kDdMode,
    // hide
    kHideEnable = 600,
    kHideNow,
    kHideAuto,
    // rollup
    kRollMode = 700,
    kRollDelay,
    kRollEdges,
    // tabs
    kTabStyleId = 800,
    kTabColorsId,
    // pages
    kPagesEnable = 900,
    kPagesSwipe,
    kPagesList,
    kPageAdd,
    kPageRename,
    kPageDelete,
    kPageGo,
    // rules
    kRulesList = 1000,
    kRuleAdd,
    kRuleEdit,
    kRuleDelete,
    kRuleUp,
    kRuleDown,
    kRulesApply,
    // layout
    kSnapOthers = 1100,
    kSnapEdges,
    kSnapGrid,
    kGridSize,
    kSpacing,
    kPerMonitor,
    kDailyBackups,
    kSnapList,
    kSnapPreview,
    kSnapTake,
    kSnapRestore,
    kSnapDelete,
    kSnapFolder,
    kLockLayout,
    kLockContent,
    kAutoLayout2,
    // business
    kTemplateCombo = 1200,
    kTemplateApply,
    kExportBtn,
    kImportBtn,
    // about
    kOpenData = 1300,
    kOpenLogs,
    kOpenGuide,
    kIssuesBtn,
    kLicenseBtn,
    kExitBtn,
};

Controller& C() { return Controller::Get(); }
Config& Cfg() { return Controller::Get().Cfg(); }

const std::vector<std::wstring> kCorners = {L"End of the taskbar", L"Top-left", L"Top-right", L"Bottom-left", L"Bottom-right"};
const std::vector<std::wstring> kModes = {L"Toggle (press once to show, again to hide)", L"Hold (show while the keys are held)"};
const int kIconSizes[] = {32, 48, 64, 96};

std::optional<Color> AskColor(HWND owner, const Color& initial) {
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

// ====================================================================== Welcome

void SettingsWindow::BuildWelcome() {
    m_form.Heading(L"Welcome to openFences");
    m_form.Text(L"openFences groups your desktop icons into shaded, labeled areas called fences. Drag on an empty part of the desktop to create "
                L"one, drag icons into it, and double-click empty desktop to hide everything.");
    m_form.Check(kAutostartId, L"Start openFences when I sign in to Windows", autostart::Query().Enabled());
    m_form.Check(kLooseIcons, L"Show desktop icons that are not in a fence", Cfg().general.showLooseIcons);
    m_form.Combo(kHostMode, L"Desktop integration", {L"Owned by the desktop (recommended)", L"Child of the desktop window"},
                 Cfg().general.hostMode == HostMode::ChildOfDesktop ? 1 : 0);
    m_form.Check(kWhatsNewCheck, L"Show what's new after openFences is updated", Cfg().general.showWhatsNew);
    m_form.Gap();
    m_form.Buttons({{kTourBtn, L"Welcome to openFences"}, {kNewFenceBtn, L"Create a fence"}, {kApplyRulesBtn, L"Sort desktop with rules"},
                    {kAutoLayoutBtn, L"Automatic layout…"}});
    m_form.Heading(L"Quick tips");
    m_form.Text(L"• Drag on the desktop to create a fence or a Folder Portal.\n"
                L"• Double-click a fence's title bar to roll it up.\n"
                L"• Drag one fence's title onto another to combine them into tabs.\n"
                L"• Press the Peek shortcut to bring fences above your windows.\n"
                L"• Pull from the left or right screen edge to switch pages.",
                true);
}

void SettingsWindow::OnWelcome(int id, int code) {
    switch (id) {
        case kAutostartId:
            autostart::Set(Checked(kAutostartId), paths::ExePath().wstring());
            break;
        case kLooseIcons:
            Cfg().general.showLooseIcons = Checked(kLooseIcons);
            Changed();
            break;
        case kHostMode:
            if (code == CBN_SELCHANGE) {
                Cfg().general.hostMode = ComboIndex(kHostMode) == 1 ? HostMode::ChildOfDesktop : HostMode::OwnedByDesktop;
                Changed();
            }
            break;
        case kNewFenceBtn: C().ExecuteCommand(L"new-fence", L""); break;
        case kApplyRulesBtn: C().ApplyRulesNow(); break;
        case kAutoLayoutBtn: C().AutomaticLayout(true); break;
        case kTourBtn: C().OpenWelcome(); break;
        case kWhatsNewCheck:
            Cfg().general.showWhatsNew = Checked(kWhatsNewCheck);
            C().SaveSoon();
            break;
    }
}

// ====================================================================== Chameleon

void SettingsWindow::BuildChameleon() {
    m_form.Heading(L"Chameleon");
    m_form.Text(L"Blend desktop icons into the wallpaper. Icons stay usable and return to normal when you point at them.");
    m_form.Check(kChamEnable, L"Enable Chameleon", Cfg().chameleon.enabled);
    m_form.Slider(kChamOpacity, L"Icon visibility", 5, 100, Cfg().chameleon.opacity, L"%");
    m_form.Check(kChamDesat, L"Remove color from faded icons", Cfg().chameleon.desaturate);
}

void SettingsWindow::OnChameleon(int id, int) {
    if (id == kChamEnable) Cfg().chameleon.enabled = Checked(id);
    else if (id == kChamDesat) Cfg().chameleon.desaturate = Checked(id);
    else return;
    Changed();
}

// ====================================================================== Personalization

void SettingsWindow::BuildPersonalization() {
    const AppearanceSettings& ap = Cfg().appearance;
    m_form.Heading(L"Fence appearance");
    m_form.ColorButton(kBgColor, L"Background color", ap.background);
    m_form.Slider(kBgAlpha, L"Opacity", 0, 100, ap.background.a * 100 / 255, L"%");
    m_form.Check(kDarkTitle, L"Darken fence title bars", ap.darkenTitle);
    m_form.Slider(kCorner, L"Corner roundness", 0, 16, ap.cornerRadius, L" px");
    m_form.Slider(kTitleSize, L"Title text size", 9, 20, ap.titleFontSize, L" px");
    m_form.Check(kWallpaperAware, L"Match fence color to the wallpaper", ap.wallpaperAware);
    m_form.Heading(L"Icons");
    int sizeIdx = 1;
    for (int i = 0; i < 4; ++i)
        if (kIconSizes[i] == ap.iconSize) sizeIdx = i;
    m_form.Combo(kIconSize, L"Icon size", {L"Small", L"Medium", L"Large", L"Extra large"}, sizeIdx);
    m_form.Check(kTintEnable, L"Tint all desktop icons with one color", ap.iconTint.has_value());
    m_form.ColorButton(kTintColor, L"Tint color", ap.iconTint.value_or(Color{120, 170, 255, 255}));
    m_form.Text(L"Individual fences can have their own color and icon tint: open the fence's title menu.", true);
}

void SettingsWindow::OnPersonalization(int id, int code) {
    AppearanceSettings& ap = Cfg().appearance;
    switch (id) {
        case kBgColor:
            if (auto c = AskColor(m_hwnd, ap.background)) {
                ap.background = *c;
                m_form.SetColor(kBgColor, *c);
            }
            break;
        case kDarkTitle: ap.darkenTitle = Checked(id); break;
        case kWallpaperAware: ap.wallpaperAware = Checked(id); break;
        case kIconSize:
            if (code != CBN_SELCHANGE) return;
            ap.iconSize = kIconSizes[std::clamp(ComboIndex(id), 0, 3)];
            break;
        case kTintEnable:
            if (Checked(id)) ap.iconTint = m_form.ColorOf(kTintColor).value_or(Color{120, 170, 255, 255});
            else ap.iconTint.reset();
            break;
        case kTintColor:
            if (auto c = AskColor(m_hwnd, m_form.ColorOf(kTintColor).value_or(Color{120, 170, 255, 255}))) {
                m_form.SetColor(kTintColor, *c);
                if (ap.iconTint) ap.iconTint = *c;
            }
            break;
        default: return;
    }
    Changed();
}

void SettingsWindow::OnSliderChanged(int id, int pos) {
    AppearanceSettings& ap = Cfg().appearance;
    switch (id) {
        case kChamOpacity: Cfg().chameleon.opacity = pos; break;
        case kBgAlpha: ap.background.a = static_cast<uint8_t>(pos * 255 / 100); break;
        case kCorner: ap.cornerRadius = pos; break;
        case kTitleSize: ap.titleFontSize = pos; break;
        case kPeekDim: Cfg().peek.dimPercent = pos; break;
        case kRollDelay: Cfg().rollup.hoverDelayMs = pos; break;
        case kGridSize: Cfg().layout.gridSize = pos; break;
        case kSpacing: Cfg().layout.spacing = pos; break;
        default: return;
    }
    Changed();
}

// ====================================================================== Folder Portals

void SettingsWindow::BuildPortals() {
    m_form.Heading(L"Folder Portals");
    m_form.Text(L"A Folder Portal is a fence that shows the live contents of any folder: Downloads, Documents, a OneDrive or SharePoint "
                L"folder, or a network share. Files you drop on a portal are moved or copied into that folder.");
    m_form.Buttons({{kNewPortalBtn, L"Create a folder portal…"}});
    m_form.Combo(kPortalView, L"Default layout", {L"Icons", L"List", L"Horizontal list"}, static_cast<int>(Cfg().portals.defaultView));
    m_form.Check(kPortalNav, L"Open subfolders inside the portal (allow navigation)", Cfg().portals.allowNavigation);
    m_form.Text(L"If a network folder is unavailable, the portal says so and keeps retrying in the background. Hold Ctrl while double-clicking "
                L"a folder to open it in Explorer instead.",
                true);
}

void SettingsWindow::OnPortals(int id, int code) {
    switch (id) {
        case kNewPortalBtn: {
            const MonitorDesc* mon = PrimaryMonitor(C().Monitors());
            if (!mon) return;
            int w = MulDiv(320, static_cast<int>(mon->dpi), 96), h = MulDiv(260, static_cast<int>(mon->dpi), 96);
            C().CreateGroup(FindFreePlacement(w, h, C().GroupRectsOnMonitor(mon->id, {}), mon->work, MulDiv(10, static_cast<int>(mon->dpi), 96)), true);
            return;
        }
        case kPortalView:
            if (code != CBN_SELCHANGE) return;
            Cfg().portals.defaultView = static_cast<ViewMode>(std::clamp(ComboIndex(id), 0, 2));
            break;
        case kPortalNav: Cfg().portals.allowNavigation = Checked(id); break;
        default: return;
    }
    Changed();
}

// ====================================================================== Peek

void SettingsWindow::BuildPeek() {
    const PeekSettings& pk = Cfg().peek;
    m_form.Heading(L"Peek");
    m_form.Text(L"Bring your fences above open windows without minimizing anything. Press Esc or click empty space to put them back.");
    m_form.Check(kPeekEnable, L"Enable Peek", pk.enabled);
    Hotkey active = pk.hotkey.empty() && C().ActivePeekHotkey().Valid() ? C().ActivePeekHotkey() : DefaultPeekHotkey();
    std::wstring automatic = L"Automatic (" + FormatHotkey(active) + L")";
    m_form.HotkeyEdit(kPeekHotkey, L"Shortcut", pk.hotkey, automatic);
    m_form.Combo(kPeekMode, L"Mode", kModes, pk.mode == TriggerMode::Hold ? 1 : 0);
    m_form.Check(kPeekTray, L"Clicking the openFences tray icon toggles Peek", pk.trayClick);
    m_form.Check(kPeekCorner, L"Peek when the pointer rests in a screen corner", pk.hotCornerEnabled);
    m_form.Combo(kPeekCornerPos, L"Hot corner", kCorners, static_cast<int>(pk.corner));
    m_form.Slider(kPeekDim, L"Dim windows behind fences", 0, 90, pk.dimPercent, L"%");
    m_form.Text(L"Win+Space is also the Windows shortcut for switching keyboard languages. If you have more than one keyboard layout, "
                L"openFences uses Ctrl+Alt+Space instead, or Ctrl+Alt+Shift+Space if another program already owns that. The box above shows the "
                L"shortcut in use.",
                true);
    const DesktopDisplaySettings& dd = Cfg().desktopDisplay;
    m_form.Heading(L"Desktop Display");
    m_form.Text(L"Show the desktop instantly, without the minimize animation and without touching your windows.");
    m_form.Check(kDdEnable, L"Enable Desktop Display", dd.enabled);
    m_form.Check(kDdWinD, L"Use Win+D for Desktop Display", dd.interceptWinD, dd.enabled);
    m_form.HotkeyEdit(kDdHotkey, L"Additional shortcut", dd.hotkey, L"None");
    m_form.Combo(kDdMode, L"Mode", kModes, dd.mode == TriggerMode::Hold ? 1 : 0);
}

void SettingsWindow::OnPeek(int id, int code) {
    PeekSettings& pk = Cfg().peek;
    DesktopDisplaySettings& dd = Cfg().desktopDisplay;
    switch (id) {
        case kPeekEnable: pk.enabled = Checked(id); break;
        case kPeekHotkey:
            if (code != EN_CHANGE) return;
            pk.hotkey = TextOf(id);
            break;
        case kPeekMode:
            if (code != CBN_SELCHANGE) return;
            pk.mode = ComboIndex(id) == 1 ? TriggerMode::Hold : TriggerMode::Toggle;
            break;
        case kPeekTray: pk.trayClick = Checked(id); break;
        case kPeekCorner: pk.hotCornerEnabled = Checked(id); break;
        case kPeekCornerPos:
            if (code != CBN_SELCHANGE) return;
            pk.corner = static_cast<HotCorner>(std::clamp(ComboIndex(id), 0, 4));
            break;
        case kDdEnable:
            dd.enabled = Checked(id);
            EnableWindow(GetDlgItem(m_panel, kDdWinD), dd.enabled);
            break;
        case kDdWinD: dd.interceptWinD = Checked(id); break;
        case kDdHotkey:
            if (code != EN_CHANGE) return;
            dd.hotkey = TextOf(id);
            break;
        case kDdMode:
            if (code != CBN_SELCHANGE) return;
            dd.mode = ComboIndex(id) == 1 ? TriggerMode::Hold : TriggerMode::Toggle;
            break;
        default: return;
    }
    Changed();
}

// ====================================================================== Hide

void SettingsWindow::BuildHide() {
    m_form.Heading(L"Quick-Hide");
    m_form.Text(L"Double-click an empty part of the desktop to fade out all fences and icons; double-click again to bring them back.");
    m_form.Check(kHideEnable, L"Double-click the desktop to hide fences and icons", Cfg().hide.enabled);
    m_form.Check(kHideNow, L"Fences and icons are hidden now", Cfg().hide.hidden);
    m_form.Check(kHideAuto, L"Hide automatically while the desktop is not in use", Cfg().hide.autoHide);
    m_form.Text(L"To keep a fence visible while everything else hides, choose “Keep visible during Quick-Hide” in its title menu. "
                L"The same option is available on loose desktop icons.",
                true);
}

void SettingsWindow::OnHide(int id, int) {
    switch (id) {
        case kHideEnable: Cfg().hide.enabled = Checked(id); break;
        case kHideNow: C().SetHidden(Checked(id)); return;
        case kHideAuto: Cfg().hide.autoHide = Checked(id); break;
        default: return;
    }
    Changed();
}

// ====================================================================== Rollup

void SettingsWindow::BuildRollup() {
    m_form.Heading(L"Roll-up");
    m_form.Text(L"Double-click a fence's title bar to roll it up to just the title. Rolled-up fences open again when you point at them.");
    m_form.Combo(kRollMode, L"Open a rolled-up fence", {L"When the pointer moves over it", L"After the pointer rests on it", L"When I click its title"},
                 static_cast<int>(Cfg().rollup.openMode));
    m_form.Slider(kRollDelay, L"Resting delay", 100, 2000, Cfg().rollup.hoverDelayMs, L" ms");
    m_form.Check(kRollEdges, L"Roll up fences placed against the top or bottom screen edge", Cfg().rollup.autoRollupAtEdges);
}

void SettingsWindow::OnRollup(int id, int code) {
    if (id == kRollMode && code == CBN_SELCHANGE) Cfg().rollup.openMode = static_cast<RollupOpen>(std::clamp(ComboIndex(id), 0, 2));
    else if (id == kRollEdges) Cfg().rollup.autoRollupAtEdges = Checked(id);
    else return;
    Changed();
}

// ====================================================================== Tabs

void SettingsWindow::BuildTabs() {
    m_form.Heading(L"Tabs");
    m_form.Text(L"Drag a fence by its title onto another fence's title to combine them into one tabbed fence. Drag a tab away from the "
                L"title bar to make it a separate fence again.");
    m_form.Combo(kTabStyleId, L"Tab style", {L"Flat", L"Segmented", L"Rounded", L"Hidden (switch tabs from the menu)"},
                 static_cast<int>(Cfg().appearance.tabStyle));
    m_form.Check(kTabColorsId, L"Use tab colors", Cfg().appearance.tabColors);
}

void SettingsWindow::OnTabs(int id, int code) {
    if (id == kTabStyleId && code == CBN_SELCHANGE) Cfg().appearance.tabStyle = static_cast<TabStyle>(std::clamp(ComboIndex(id), 0, 3));
    else if (id == kTabColorsId) Cfg().appearance.tabColors = Checked(id);
    else return;
    Changed();
}

// ====================================================================== Pages

void SettingsWindow::BuildPages() {
    m_form.Heading(L"Desktop pages");
    m_form.Text(L"Keep several desktops of fences (for example Work, School and Play). Press and hold at the far left or right edge of "
                L"the screen and pull to switch pages.");
    m_form.Check(kPagesEnable, L"Enable pages", Cfg().pages.enabled);
    m_form.Check(kPagesSwipe, L"Switch pages by pulling from the screen edge", Cfg().pages.edgeSwipe);
    m_form.List(kPagesList, 140);
    m_form.Buttons({{kPageAdd, L"Add page"}, {kPageRename, L"Rename…"}, {kPageDelete, L"Delete"}, {kPageGo, L"Go to page"}});
    m_form.Text(L"A fence can appear on every page: choose “Show on all pages” in its title menu.", true);
    FillPages();
}

void SettingsWindow::FillPages() {
    HWND list = GetDlgItem(m_panel, kPagesList);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    Layout& l = C().CurrentLayout();
    for (size_t i = 0; i < l.pages.size(); ++i) {
        std::wstring text = l.pages[i].name + L"  (" + std::to_wstring(l.pages[i].groups.size()) + L" fences)";
        if (static_cast<int>(i) == l.currentPage) text += L"  ← current";
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
    SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(l.currentPage), 0);
}

void SettingsWindow::OnPages(int id, int code) {
    int sel = static_cast<int>(SendDlgItemMessageW(m_panel, kPagesList, LB_GETCURSEL, 0, 0));
    Layout& l = C().CurrentLayout();
    switch (id) {
        case kPagesEnable: Cfg().pages.enabled = Checked(id); Changed(); return;
        case kPagesSwipe: Cfg().pages.edgeSwipe = Checked(id); Changed(); return;
        case kPagesList:
            if (code == LBN_DBLCLK && sel >= 0) C().GoToPage(sel, sel > l.currentPage ? -1 : 1);
            else return;
            break;
        case kPageAdd: C().AddPage(); break;
        case kPageGo:
            if (sel >= 0) C().GoToPage(sel, sel > l.currentPage ? -1 : 1);
            break;
        case kPageRename:
            if (sel >= 0) {
                if (auto name = PromptText(m_hwnd, L"Rename page", L"Page name:", l.pages[static_cast<size_t>(sel)].name)) {
                    std::wstring n = Trim(*name);
                    if (!n.empty()) l.pages[static_cast<size_t>(sel)].name = n;
                    C().SaveSoon();
                }
            }
            break;
        case kPageDelete:
            if (sel >= 0) {
                if (sel != l.currentPage) C().GoToPage(sel, 0);
                C().DeleteCurrentPage();
            }
            break;
        default: return;
    }
    FillPages();
}

// ====================================================================== Rules

void SettingsWindow::BuildRules() {
    m_form.Heading(L"Rules");
    m_form.Text(L"Rules put new desktop items into the right fence automatically, by file type, name, shortcut target or date. They are "
                L"checked from top to bottom and the first match wins.");
    m_form.ListView(kRulesList, 220, {{L"Rule", 150}, {L"When", 260}, {L"Then", 170}}, true);
    m_form.Buttons({{kRuleAdd, L"Add…"}, {kRuleEdit, L"Edit…"}, {kRuleDelete, L"Delete"}, {kRuleUp, L"Move up"}, {kRuleDown, L"Move down"}});
    m_form.Buttons({{kRulesApply, L"Sort the desktop now"}});
    FillRules();
}

void SettingsWindow::FillRules() {
    HWND lv = GetDlgItem(m_panel, kRulesList);
    if (!lv) return;
    bool building = m_building;
    m_building = true;
    ListView_DeleteAllItems(lv);
    m_tabChoices = ListTabs(C().CurrentLayout());
    int i = 0;
    for (const Rule& r : Cfg().rules) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.pszText = const_cast<wchar_t*>(r.name.c_str());
        ListView_InsertItem(lv, &item);
        std::wstring when = DescribeCriteria(r);
        ListView_SetItemText(lv, i, 1, when.data());
        std::wstring then;
        if (r.action == RuleAction::MoveToFolder) {
            then = L"Move to " + r.targetFolder;
        } else {
            then = L"(missing fence)";
            for (const auto& [id, label] : m_tabChoices)
                if (id == r.targetTab) then = L"Fence: " + label;
        }
        ListView_SetItemText(lv, i, 2, then.data());
        ListView_SetCheckState(lv, i, r.enabled);
        ++i;
    }
    m_building = building;
}

void SettingsWindow::OnRules(int id, int) {
    HWND lv = GetDlgItem(m_panel, kRulesList);
    int sel = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
    auto& rules = Cfg().rules;
    switch (id) {
        case kRuleAdd: {
            Rule r;
            r.id = NewId("r");
            r.name = L"New rule";
            if (EditRule(m_hwnd, r, ListTabs(C().CurrentLayout()))) {
                rules.push_back(r);
                sel = static_cast<int>(rules.size()) - 1;
            }
            break;
        }
        case kRuleEdit:
            if (sel >= 0) EditRule(m_hwnd, rules[static_cast<size_t>(sel)], ListTabs(C().CurrentLayout()));
            break;
        case kRuleDelete:
            if (sel >= 0) rules.erase(rules.begin() + sel);
            sel = std::min(sel, static_cast<int>(rules.size()) - 1);
            break;
        case kRuleUp:
            if (sel > 0) {
                std::swap(rules[static_cast<size_t>(sel)], rules[static_cast<size_t>(sel - 1)]);
                --sel;
            }
            break;
        case kRuleDown:
            if (sel >= 0 && sel + 1 < static_cast<int>(rules.size())) {
                std::swap(rules[static_cast<size_t>(sel)], rules[static_cast<size_t>(sel + 1)]);
                ++sel;
            }
            break;
        case kRulesApply: C().ApplyRulesNow(); return;
        default: return;
    }
    C().SaveSoon();
    FillRules();
    if (sel >= 0) ListView_SetItemState(lv, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

LRESULT SettingsWindow::OnNotify(NMHDR* nm) {
    if (m_building) return 0;
    if (nm->idFrom == static_cast<UINT_PTR>(kRulesList)) {
        if (nm->code == LVN_ITEMCHANGED) {
            auto* lv = reinterpret_cast<NMLISTVIEW*>(nm);
            if ((lv->uChanged & LVIF_STATE) && ((lv->uNewState ^ lv->uOldState) & LVIS_STATEIMAGEMASK) && lv->iItem >= 0 &&
                lv->iItem < static_cast<int>(Cfg().rules.size())) {
                Cfg().rules[static_cast<size_t>(lv->iItem)].enabled = ListView_GetCheckState(nm->hwndFrom, lv->iItem) != 0;
                C().SaveSoon();
            }
        } else if (nm->code == NM_DBLCLK) {
            OnRules(kRuleEdit, 0);
        }
    }
    return 0;
}

// ====================================================================== Layout

void SettingsWindow::BuildLayout() {
    const LayoutSettings& ly = Cfg().layout;
    m_form.Heading(L"Layout and snapping");
    m_form.Check(kSnapOthers, L"Snap fences to each other", ly.snapToOthers);
    m_form.Check(kSnapEdges, L"Snap fences to screen edges", ly.snapToEdges);
    m_form.Check(kSnapGrid, L"Snap fences to a grid", ly.snapToGrid);
    m_form.Slider(kGridSize, L"Grid size", 4, 64, ly.gridSize, L" px");
    m_form.Slider(kSpacing, L"Space between snapped fences", 0, 40, ly.spacing, L" px");
    m_form.Check(kPerMonitor, L"Keep a separate layout for each monitor arrangement (docked, undocked, …)", ly.perMonitorConfig);
    m_form.Check(kLockLayout, L"Lock fences (no moving or resizing)", C().IsLayoutLocked(nullptr), !C().Pol().lockLayout.has_value());
    m_form.Check(kLockContent, L"Lock fence contents (no adding or removing icons)", C().IsContentLocked(nullptr), !C().Pol().lockContent.has_value());
    m_form.Buttons({{kAutoLayout2, L"Automatic layout…"}});
    m_form.Heading(L"Snapshots and backups");
    m_form.Check(kDailyBackups, L"Back up the layout every day (keeps the last 7 days)", ly.dailyBackups);
    m_form.List(kSnapList, 120);
    // The preview is a picture of the primary monitor: its height follows the width (scaled with the window, never
    // cropped), up to 360 DIP. DrawSnapshotPreview fits the whole monitor into it.
    double aspect = 16.0 / 9.0;
    if (const MonitorDesc* m = PrimaryMonitor(C().Monitors())) aspect = static_cast<double>(m->work.Width()) / std::max(1, m->work.Height());
    m_form.OwnerDrawAspect(kSnapPreview, aspect, 120, 360);
    m_form.Buttons({{kSnapTake, L"Take snapshot"}, {kSnapRestore, L"Restore…"}, {kSnapDelete, L"Delete"}, {kSnapFolder, L"Open folder"}});
    FillSnapshots();
}

void SettingsWindow::FillSnapshots() {
    m_snapshots = ListSnapshots(paths::BackupsDir());
    HWND list = GetDlgItem(m_panel, kSnapList);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& s : m_snapshots) {
        std::wstring text = FormatStamp(s.stamp) + L"   " + s.name;
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
    if (!m_snapshots.empty()) SendMessageW(list, LB_SETCURSEL, 0, 0);
    InvalidateRect(GetDlgItem(m_panel, kSnapPreview), nullptr, TRUE);
}

void SettingsWindow::DrawSnapshotPreview(DRAWITEMSTRUCT* dis) {
    if (dis->CtlID != static_cast<UINT>(kSnapPreview)) return;
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    HBRUSH bg = CreateSolidBrush(RGB(32, 40, 56));
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    int sel = static_cast<int>(SendDlgItemMessageW(m_panel, kSnapList, LB_GETCURSEL, 0, 0));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(220, 225, 235));
    HGDIOBJ oldFont = SelectObject(dc, m_font);
    if (sel < 0 || sel >= static_cast<int>(m_snapshots.size())) {
        DrawTextW(dc, L"No snapshot selected", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont);
        return;
    }
    auto snap = LoadSnapshot(m_snapshots[static_cast<size_t>(sel)].path);
    const Layout* layout = nullptr;
    if (snap) {
        auto it = snap->layouts.find(ComputeMonitorConfigHash(C().Monitors()));
        if (it == snap->layouts.end()) it = snap->layouts.find("default");
        if (it == snap->layouts.end() && !snap->layouts.empty()) it = snap->layouts.begin();
        if (it != snap->layouts.end()) layout = &it->second;
    }
    if (!layout) {
        DrawTextW(dc, L"This snapshot has no fences", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont);
        return;
    }
    // Draw the primary monitor's work area scaled into the preview, with each fence of the current page.
    int pad = MulDiv(8, static_cast<int>(m_dpi), 96);
    int availW = rc.right - rc.left - 2 * pad, availH = rc.bottom - rc.top - 2 * pad;
    double aspect = 16.0 / 9.0;
    if (const MonitorDesc* m = PrimaryMonitor(C().Monitors())) aspect = static_cast<double>(m->work.Width()) / std::max(1, m->work.Height());
    int w = availW, h = static_cast<int>(availW / aspect);
    if (h > availH) {
        h = availH;
        w = static_cast<int>(availH * aspect);
    }
    RECT screen{rc.left + (rc.right - rc.left - w) / 2, rc.top + (rc.bottom - rc.top - h) / 2, 0, 0};
    screen.right = screen.left + w;
    screen.bottom = screen.top + h;
    HBRUSH desk = CreateSolidBrush(RGB(58, 84, 120));
    FillRect(dc, &screen, desk);
    DeleteObject(desk);
    std::vector<const Group*> groups;
    for (const Group& g : layout->persistent) groups.push_back(&g);
    int page = std::clamp(layout->currentPage, 0, std::max(0, static_cast<int>(layout->pages.size()) - 1));
    if (!layout->pages.empty())
        for (const Group& g : layout->pages[static_cast<size_t>(page)].groups) groups.push_back(&g);
    HBRUSH fence = CreateSolidBrush(RGB(30, 34, 44));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(150, 170, 200));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, fence);
    for (const Group* g : groups) {
        RECT fr{screen.left + static_cast<LONG>(g->rect.x * w), screen.top + static_cast<LONG>(g->rect.y * h), 0, 0};
        fr.right = fr.left + std::max(4L, static_cast<LONG>(g->rect.w * w));
        fr.bottom = fr.top + std::max(4L, static_cast<LONG>(g->rect.h * h));
        RoundRect(dc, fr.left, fr.top, fr.right, fr.bottom, 6, 6);
        if (!g->tabs.empty()) {
            RECT tr = fr;
            InflateRect(&tr, -3, -2);
            DrawTextW(dc, g->tabs[0].title.c_str(), -1, &tr, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(fence);
    DeleteObject(pen);
    SelectObject(dc, oldFont);
}

void SettingsWindow::OnLayout(int id, int code) {
    LayoutSettings& ly = Cfg().layout;
    int sel = static_cast<int>(SendDlgItemMessageW(m_panel, kSnapList, LB_GETCURSEL, 0, 0));
    switch (id) {
        case kSnapOthers: ly.snapToOthers = Checked(id); break;
        case kSnapEdges: ly.snapToEdges = Checked(id); break;
        case kSnapGrid: ly.snapToGrid = Checked(id); break;
        case kPerMonitor: ly.perMonitorConfig = Checked(id); break;
        case kDailyBackups: ly.dailyBackups = Checked(id); break;
        case kLockLayout: Cfg().lock.layoutLocked = Checked(id); break;
        case kLockContent: Cfg().lock.contentLocked = Checked(id); break;
        case kAutoLayout2: C().AutomaticLayout(true); return;
        case kSnapList:
            if (code == LBN_SELCHANGE) InvalidateRect(GetDlgItem(m_panel, kSnapPreview), nullptr, TRUE);
            return;
        case kSnapTake:
            if (auto name = PromptText(m_hwnd, L"Take snapshot", L"Snapshot name:", L"My layout")) {
                C().TakeSnapshot(Trim(*name).empty() ? L"Snapshot" : Trim(*name));
                FillSnapshots();
            }
            return;
        case kSnapRestore:
            if (sel >= 0 && sel < static_cast<int>(m_snapshots.size())) {
                const auto& s = m_snapshots[static_cast<size_t>(sel)];
                std::wstring msg = L"Restore the snapshot \"" + s.name + L"\" from " + FormatStamp(s.stamp) +
                                   L"?\n\nYour current layout is saved as a snapshot first.";
                if (MessageBoxW(m_hwnd, msg.c_str(), L"Restore snapshot", MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
                    if (!C().RestoreSnapshot(s.path.wstring())) MessageBoxW(m_hwnd, L"The snapshot could not be read.", L"openFences", MB_ICONERROR);
                    RebuildPage();
                }
            }
            return;
        case kSnapDelete:
            if (sel >= 0 && sel < static_cast<int>(m_snapshots.size())) {
                std::error_code ec;
                std::filesystem::remove(m_snapshots[static_cast<size_t>(sel)].path, ec);
                FillSnapshots();
            }
            return;
        case kSnapFolder: {
            std::error_code ec;
            std::filesystem::create_directories(paths::BackupsDir(), ec);
            ShellExecuteW(m_hwnd, L"open", paths::BackupsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return;
        }
        default: return;
    }
    Changed();
}

// ====================================================================== Business

void SettingsWindow::BuildBusiness() {
    const Policy& p = C().Pol();
    m_form.Heading(L"Fences for business");
    std::wstring status;
    status += L"Configuration pushed by policy: " + (p.configPath.empty() ? std::wstring(L"none") : p.configPath) + L"\n";
    status += L"Layout lock: " + std::wstring(p.lockLayout ? (*p.lockLayout ? L"enforced on" : L"enforced off") : L"user choice") + L"\n";
    status += L"Content lock: " + std::wstring(p.lockContent ? (*p.lockContent ? L"enforced on" : L"enforced off") : L"user choice") + L"\n";
    status += L"Settings window: " + std::wstring(p.disableSettings ? L"disabled by policy" : L"available");
    m_form.Text(status);
    m_form.Heading(L"Templates");
    m_templates = ListTemplates(paths::TemplatesDir());
    std::vector<std::wstring> names;
    for (const auto& t : m_templates) names.push_back(t.name + (t.description.empty() ? L"" : L" — " + t.description));
    m_form.Combo(kTemplateCombo, L"Template", names, names.empty() ? -1 : 0, !names.empty());
    m_form.Buttons({{kTemplateApply, L"Apply template…"}}, !names.empty());
    m_form.Heading(L"Share layouts");
    m_form.Buttons({{kExportBtn, L"Export layout…"}, {kImportBtn, L"Import layout…"}});
    m_form.Heading(L"Deployment");
    m_form.Text(L"Administrators can manage openFences with these registry values (REG_DWORD unless noted):\n"
                L"HKLM\\SOFTWARE\\Policies\\openFences\n"
                L"    ConfigPath (REG_SZ): template or exported layout loaded at every sign-in, e.g. \\\\server\\share\\fences.json\n"
                L"    LockLayout, LockContent: 1 = locked, 0 = unlocked\n"
                L"    DisableSettings: 1 = hide this window\n"
                L"    Disabled: 1 = do not run\n"
                L"HKCU\\Software\\openFences\\Disabled: 1 = do not run for this user\n\n"
                L"Silent install: msiexec /i openFences.msi /qn AUTOSTART=1",
                true);
}

void SettingsWindow::OnBusiness(int id, int) {
    switch (id) {
        case kTemplateApply: {
            int sel = ComboIndex(kTemplateCombo);
            if (sel < 0 || sel >= static_cast<int>(m_templates.size())) return;
            const Template& t = m_templates[static_cast<size_t>(sel)];
            std::wstring msg = L"Apply the template \"" + t.name + L"\"? It replaces the fences and rules of the current layout (a snapshot is "
                               L"saved first).";
            if (MessageBoxW(m_hwnd, msg.c_str(), L"Apply template", MB_OKCANCEL | MB_ICONQUESTION) == IDOK) C().ApplyTemplateFile(t.path.wstring());
            return;
        }
        case kExportBtn: {
            std::wstring path = PickJsonFile(m_hwnd, true, L"Export layout", L"openFences-layout.json");
            if (!path.empty() && !C().ExportLayout(path)) MessageBoxW(m_hwnd, L"The layout could not be written.", L"openFences", MB_ICONERROR);
            return;
        }
        case kImportBtn: {
            std::wstring path = PickJsonFile(m_hwnd, false, L"Import layout");
            if (path.empty()) return;
            // A layout file can come from anyone. Its rules run as soon as it's applied, and a "move file to folder" rule
            // would move desktop files away (even to a network share), so say where before importing.
            if (auto tpl = LoadTemplate(path)) {
                auto dests = MoveRuleDestinations(tpl->rules);
                if (!dests.empty()) {
                    std::wstring text = L"This layout contains rules that move files from your desktop to:\n\n";
                    for (const auto& d : dests) text += L"    " + d + (d.rfind(L"\\\\", 0) == 0 ? L"   (network location)" : L"") + L"\n";
                    text += L"\nMatching files on your desktop will be moved there as soon as the layout is imported. Only continue if you "
                            L"trust where this file came from.\n\nImport it anyway?";
                    if (MessageBoxW(m_hwnd, text.c_str(), L"Import layout", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) return;
                }
            }
            if (!C().ImportLayout(path)) MessageBoxW(m_hwnd, L"That file is not an openFences layout.", L"openFences", MB_ICONERROR);
            return;
        }
    }
}

// ====================================================================== About

void SettingsWindow::BuildAbout() {
    m_form.Heading(std::wstring(L"openFences ") + OF_VERSION_WSTRING + (paths::IsPortable() ? L" (portable)" : L""));
    m_form.Text(L"A native, open-source desktop organizer for Windows 10 and Windows 11 (x64), modeled on Stardock Fences.");
    m_form.Text(std::wstring(L"Created by ") + OF_AUTHOR_WSTRING + L".\n" + OF_COPYRIGHT_WSTRING + L". Released under the " + OF_LICENSE_WSTRING + L".");
    if (paths::IsPortable())
        m_form.Text(L"Portable mode: settings, backups and logs are kept in the Data folder next to openFences.exe.", true);
    m_form.Text(L"Settings and layouts: " + paths::RoamingDir().wstring() + L"\nLogs and crash dumps: " + paths::LocalDir().wstring(), true);
    m_form.Buttons({{kOpenGuide, L"Open user guide"}, {kIssuesBtn, L"Report a bug or idea"}, {kLicenseBtn, L"View license"}});
    m_form.Buttons({{kOpenData, L"Open settings folder"}, {kOpenLogs, L"Open log folder"}});
    m_form.Gap(12);
    m_form.Buttons({{kExitBtn, L"Exit openFences"}});
}

void SettingsWindow::OnAbout(int id, int) {
    std::error_code ec;
    if (id == kOpenData) {
        std::filesystem::create_directories(paths::RoamingDir(), ec);
        ShellExecuteW(m_hwnd, L"open", paths::RoamingDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (id == kOpenLogs) {
        std::filesystem::create_directories(paths::LogsDir(), ec);
        ShellExecuteW(m_hwnd, L"open", paths::LogsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (id == kOpenGuide) {
        OpenUserGuide(m_hwnd);
    } else if (id == kIssuesBtn) {
        OpenIssuesPage(m_hwnd);
    } else if (id == kLicenseBtn) {
        OpenLicense(m_hwnd);
    } else if (id == kExitBtn) {
        C().Exit();
    }
}

}  // namespace of
