// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <windows.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "app/Worker.h"
#include "core/Hotkey.h"
#include "core/Model.h"
#include "core/MonitorConfig.h"
#include "core/Policy.h"
#include "core/Templates.h"
#include "desktop/DesktopHost.h"
#include "shell/ChangeWatcher.h"
#include "shell/IconCache.h"
#include "ui/ItemView.h"

namespace of {

class FenceWindow;
class DesktopCanvas;
class SettingsWindow;
class WelcomeWindow;
class Tray;

enum class PeekKind { None, Peek, DesktopDisplay };

// Live contents of one Folder Portal tab.
struct PortalModel {
    enum class State { Loading, Ready, Offline };
    std::string tabId;
    std::wstring rootPath;
    std::vector<SharedPidl> stack;  // navigation stack; back() is the folder shown
    std::vector<ShellItemPtr> items;
    State state = State::Loading;
    ChangeWatch watch;
    UINT watchMsg = 0;
    int retryDelayMs = 0;
    uint64_t requestId = 0;
    std::wstring folderName;
};

struct CommandLine {
    std::wstring verb;  // e.g. "settings", "toggle-hide"; empty = normal start
    std::wstring arg;
    bool autostart = false;
    bool verbose = false;
    bool noWelcome = false;  // --no-welcome: don't open the Welcome window automatically this session
    int restarts = 0;
    ULONGLONG firstRestartTick = 0;
};

// The application: owns the configuration, the desktop item model and every window.
class Controller {
public:
    static Controller& Get();
    int Run(const CommandLine& cmd);
    HWND MessageWindow() const { return m_msgWnd; }

    // ---- state ----
    Config& Cfg() { return m_config; }
    Layout& CurrentLayout();
    const std::vector<MonitorDesc>& Monitors() const { return m_monitors; }
    const MonitorDesc* MonitorFor(const Group& g) const;
    IconCache& Icons() { return *m_icons; }
    DesktopHost& Host() { return m_host; }
    const Policy& Pol() const { return m_policy; }
    std::wstring DesktopFolderPath() const { return m_desktopPath; }

    // ---- queries ----
    ShellItemPtr DesktopItem(const std::wstring& key) const;
    std::vector<ShellItemPtr> TabItems(const Tab& tab);
    PortalModel* Portal(const std::string& tabId);
    bool IsLayoutLocked(const Group* g) const;
    bool IsContentLocked(const Group* g) const;
    bool Hidden() const;
    PeekKind Peeking() const { return m_peek; }
    RectI GroupScreenRect(const Group& g) const;
    std::vector<RectI> GroupRectsOnMonitor(const std::wstring& monitorId, const std::string& exceptId);
    ItemViewStyle StyleFor(const Group* g, const Tab* t, float scale) const;
    Color BackgroundFor(const Group& g) const;
    FenceWindow* FenceAtTitle(POINT screen, const FenceWindow* except) const;
    void HighlightMergeTarget(const std::string& groupId);
    bool IsOurWindow(HWND hwnd) const;
    SnapOptions Snap(float scale) const;
    int PageCount();

    // ---- mutations ----
    void SaveSoon();
    void RefreshAll();
    void InvalidateAll();
    void SyncItems();
    Group* CreateGroup(const RectI& screen, bool portal, const std::vector<std::wstring>& keys = {}, bool rename = true);
    void DeleteGroup(const std::string& groupId, bool confirm);
    void SetGroupRect(const std::string& groupId, const RectI& screen);
    void AssignItems(const std::vector<std::wstring>& keys, const std::string& tabId, int insertIndex);
    void MakeLoose(const std::vector<std::wstring>& keys, const std::wstring& monitorId, PointI cell);
    void MergeGroups(const std::string& sourceId, const std::string& targetId);
    void DetachTab(const std::string& groupId, int tabIndex, const RectI& screen);
    void ReorderTab(const std::string& groupId, int from, int to);
    void ExpectDroppedFiles(const std::vector<std::wstring>& names, const std::string& tabId, const std::wstring& monitorId, PointI cell);
    void NoteRename(const std::wstring& oldKey, const std::wstring& newKey);
    void RenameItemInteractive(HWND owner, const ShellItemPtr& item, const RectI& labelScreenRect, float fontPx);

    void PortalNavigate(const std::string& tabId, const ShellItemPtr& folder);
    void PortalBack(const std::string& tabId);
    void PortalRefresh(const std::string& tabId);

    // ---- commands (Commands.cpp) ----
    void ShowFenceMenu(FenceWindow* fence, POINT screen);
    void ShowItemMenu(HWND owner, const std::vector<ShellItemPtr>& items, POINT screen, const std::string& tabId, bool desktopItems,
                      const RectI& firstLabelScreen, float fontPx);
    void ShowDesktopMenu(DesktopCanvas* canvas, POINT screen);
    void ShowCreateMenu(DesktopCanvas* canvas, const RectI& screenBand, const std::vector<std::wstring>& selectedKeys, POINT screen);
    void ShowTrayMenu();
    void OnTrayClick();
    void ToggleHide();
    void SetHidden(bool hidden);
    void ToggleLock();
    void TogglePeek(PeekKind kind);
    void StartPeek(PeekKind kind, bool hold, unsigned holdVk);
    void EndPeek();
    void GoToPage(int index, int direction);
    void NextPage(int direction);
    void AddPage();
    void DeleteCurrentPage();
    void RenameCurrentPage(const std::wstring& name);
    void TogglePersistent(const std::string& groupId);
    void ApplyRulesNow();
    void AutomaticLayout(bool confirm);
    void TakeSnapshot(const std::wstring& name);
    bool RestoreSnapshot(const std::wstring& path);
    bool ExportLayout(const std::wstring& path);
    bool ImportLayout(const std::wstring& path);
    bool ApplyTemplateFile(const std::wstring& path);
    void OpenSettings(const std::wstring& page = {});
    // Built-in documentation ("Welcome to openFences"); page is a core/Welcome.h id, "" = overview.
    void OpenWelcome(const std::wstring& page = {});
    void SettingsChanged();
    void ExecuteCommand(const std::wstring& verb, const std::wstring& arg);
    void OnItemOpened();
    void Exit();

    // Page swipe gesture from a canvas (dx in pixels; positive = pulling right).
    void PageSwipeUpdate(int dx, int width);
    void PageSwipeEnd(int dx, int width);

    void RegisterHotkeys();
    void UnregisterHotkeys();
    const Hotkey& ActivePeekHotkey() const { return m_peekHotkey; }

private:
    friend class Tray;
    bool Init(const CommandLine& cmd);
    void Shutdown();
    static LRESULT CALLBACK MsgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

    void LoadConfig();
    void ApplyPolicyConfig();
    void SelectLayoutForMonitors();
    void OnDisplayChanged();
    void RebuildCanvases();
    void RestackAll();
    void LayoutLooseItems();

    void StartDesktopWatch();
    void RequestDesktopScan();
    void ApplyDesktopScan(std::vector<ShellItemPtr> items);
    void OnDesktopChange(WPARAM wp, LPARAM lp);
    void HandleNewItems(const std::vector<ShellItemPtr>& fresh);
    void PruneMissingMembers();
    void FirstRunSetup();

    void SyncPortals();
    void LoadPortal(PortalModel& model);
    void OnPortalChange(UINT msg, WPARAM wp, LPARAM lp);

    void UpdateHooks();
    void HotCornerTick();
    void UpdateAutoHide(HWND foreground);
    void PageAnimTick();
    void DailyBackupTick();
    void DoSave();
    void ApplyHiddenState(bool animate);
    void UpdateWallpaperColor();
    FenceWindow* FenceFor(const std::string& groupId) const;
    void SortItems(std::vector<ShellItemPtr>& items, SortMode mode, bool foldersFirst) const;
    std::vector<std::wstring> RulesTargetsApply(const std::vector<ShellItemPtr>& candidates);
    void FinishPageAnim();

    Config m_config;
    std::string m_layoutKey;
    Policy m_policy;
    std::vector<MonitorDesc> m_monitors;
    DesktopHost m_host;
    HWND m_msgWnd = nullptr;
    UINT m_taskbarCreatedMsg = 0;
    std::unique_ptr<Worker> m_enumWorker;
    std::unique_ptr<Worker> m_iconWorker;
    std::unique_ptr<Worker> m_portalWorker;
    std::unique_ptr<IconCache> m_icons;
    std::unique_ptr<Tray> m_tray;
    std::unique_ptr<SettingsWindow> m_settings;
    std::unique_ptr<WelcomeWindow> m_welcome;

    std::vector<std::unique_ptr<FenceWindow>> m_fences;
    std::vector<std::unique_ptr<DesktopCanvas>> m_canvases;
    std::map<std::string, std::unique_ptr<PortalModel>> m_portals;
    UINT m_nextPortalMsg = 0;

    // Desktop item model.
    std::map<std::wstring, ShellItemPtr> m_items;
    std::vector<std::wstring> m_itemOrder;  // enumeration order
    bool m_haveScan = false;
    bool m_scanPending = false;
    bool m_scanQueued = false;
    std::wstring m_desktopPath;
    std::wstring m_publicDesktopPath;
    ChangeWatch m_userDesktopWatch;
    ChangeWatch m_publicDesktopWatch;
    ChangeWatch m_rootWatch;
    std::map<std::wstring, std::wstring> m_renames;
    struct PendingDrop {
        std::wstring name;
        std::string tabId;
        std::wstring monitorId;
        PointI cell;
        ULONGLONG expires = 0;
    };
    std::vector<PendingDrop> m_pendingDrops;

    // Peek / hide / pages.
    PeekKind m_peek = PeekKind::None;
    bool m_peekHold = false;
    unsigned m_holdVk = 0;
    bool m_autoHidden = false;
    int m_hotCornerDwell = 0;
    bool m_hotCornerArmed = true;
    struct PageAnim {
        bool active = false;
        ULONGLONG start = 0;
        int direction = 0;
        std::vector<std::unique_ptr<FenceWindow>> outgoing;
        std::vector<FenceWindow*> incoming;
    } m_pageAnim;
    int m_swipeDx = 0;

    Hotkey m_peekHotkey;
    Hotkey m_ddHotkey;
    std::optional<Color> m_wallpaperColor;
    std::optional<Template> m_policyTemplate;
    std::set<std::string> m_policyAppliedLayouts;
    bool m_exiting = false;
    bool m_noAutoWelcome = false;
    std::wstring m_lastBackupDate;
};

// Timer and message ids used on the controller's message window.
constexpr UINT WM_APP_DESKTOP_CHANGE = WM_APP + 10;
constexpr UINT WM_APP_TRAY = WM_APP + 11;
constexpr UINT WM_APP_COMMAND = WM_APP + 12;
constexpr UINT WM_APP_PORTAL_FIRST = WM_APP + 0x100;
constexpr UINT WM_APP_PORTAL_LAST = WM_APP + 0x1FF;

constexpr UINT_PTR kTimerSave = 1;
constexpr UINT_PTR kTimerScan = 2;
constexpr UINT_PTR kTimerDisplay = 3;
constexpr UINT_PTR kTimerHold = 4;
constexpr UINT_PTR kTimerHotCorner = 5;
constexpr UINT_PTR kTimerDaily = 6;
constexpr UINT_PTR kTimerIcons = 7;
constexpr UINT_PTR kTimerPageAnim = 8;
constexpr UINT_PTR kTimerPrune = 9;
constexpr UINT_PTR kTimerAutoHide = 10;
constexpr UINT_PTR kTimerPageIndicator = 11;
constexpr UINT_PTR kTimerPortalRetryBase = 0x100;

constexpr int kHotkeyPeek = 1;
constexpr int kHotkeyDesktopDisplay = 2;
constexpr int kHotkeyNextPage = 3;
constexpr int kHotkeyPrevPage = 4;

}  // namespace of
