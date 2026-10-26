// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "app/Controller.h"

#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <set>

#include "Version.h"
#include "app/CrashHandler.h"
#include "app/Ipc.h"
#include "app/Tray.h"
#include "app/Watchdog.h"
#include "core/Autostart.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Rules.h"
#include "core/Snapshots.h"
#include "core/Strings.h"
#include "desktop/DisplayManager.h"
#include "desktop/InputHooks.h"
#include "render/RenderDevice.h"
#include "shell/Enumerator.h"
#include "shell/FileOps.h"
#include "shell/Wallpaper.h"
#include "ui/DesktopCanvas.h"
#include "ui/FenceWindow.h"
#include "ui/InlineEditor.h"
#include "ui/settings/SettingsWindow.h"
#include "ui/welcome/WelcomeWindow.h"
#include "core/Welcome.h"

namespace of {

namespace {

std::wstring KnownFolderPath(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw))) out = raw;
    CoTaskMemFree(raw);
    return out;
}

std::wstring FileNameOf(const std::wstring& path) { return PathFindFileNameW(path.c_str()); }

bool ProcessIsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
    CloseHandle(token);
    return elevated;
}

std::wstring ParentDir(const std::wstring& path) {
    std::wstring p = path;
    PathRemoveFileSpecW(p.data());
    p.resize(wcslen(p.c_str()));
    return p;
}

}  // namespace

Controller& Controller::Get() {
    static Controller instance;
    return instance;
}

Layout& Controller::CurrentLayout() {
    Layout& l = m_config.layouts[m_layoutKey];
    EnsurePage(l);
    return l;
}

// ====================================================================== lifecycle

int Controller::Run(const CommandLine& cmd) {
    if (!Init(cmd)) return m_policy.disabled ? 0 : 1;
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (m_settings && m_settings->TranslateDialogMessage(&msg)) continue;
        if (m_welcome && m_welcome->TranslateDialogMessage(&msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    Shutdown();
    return 0;
}

bool Controller::Init(const CommandLine& cmd) {
    log::Init(cmd.verbose);
    log::Info(L"openFences {} starting (autostart={}, restarts={})", OF_VERSION_WSTRING, cmd.autostart, cmd.restarts);
    InstallCrashHandler();
    m_noAutoWelcome = cmd.noWelcome;

    m_policy = ReadPolicy();
    if (m_policy.disabled) {
        log::Info(L"Disabled by policy/registry; exiting");
        return false;
    }
    if (!Gfx().Init()) {
        MessageBoxW(nullptr, L"openFences could not initialise Direct2D/Direct3D.", L"openFences", MB_ICONERROR);
        return false;
    }

    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = MsgProc;
    wc.hInstance = inst;
    wc.lpszClassName = ipc::kMessageWindowClass;
    RegisterClassExW(&wc);
    // A hidden top-level window (not HWND_MESSAGE) so broadcasts like TaskbarCreated and WM_DISPLAYCHANGE arrive.
    m_msgWnd = CreateWindowExW(WS_EX_TOOLWINDOW, ipc::kMessageWindowClass, L"openFences", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
    if (!m_msgWnd) return false;
    // Commands arrive as WM_COPYDATA. Normally UIPI already lets same-user, same-integrity processes send them and blocks
    // sandboxed (low-integrity) ones. Only an elevated instance needs the filter relaxed, so that the CLI, the desktop
    // menu and the Start menu (not elevated) can still reach it. Relaxing it always would let any low-integrity process
    // send "export <path>", "import <file>" and similar commands.
    if (ProcessIsElevated()) ChangeWindowMessageFilterEx(m_msgWnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
    InitUiDispatcher(m_msgWnd);
    m_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    m_enumWorker = std::make_unique<Worker>(L"openFences desktop scan");
    m_iconWorker = std::make_unique<Worker>(L"openFences icons");
    m_portalWorker = std::make_unique<Worker>(L"openFences portals");
    m_icons = std::make_unique<IconCache>(*m_iconWorker);
    m_icons->onLoaded = [this] { SetTimer(m_msgWnd, kTimerIcons, 30, nullptr); };

    m_desktopPath = KnownFolderPath(FOLDERID_Desktop);
    m_publicDesktopPath = KnownFolderPath(FOLDERID_PublicDesktop);
    LoadConfig();
    ApplyPolicyConfig();
    autostart::ApplyInstallerDefaultOnce(paths::ExePath().wstring());

    m_monitors = EnumerateMonitors();
    SelectLayoutForMonitors();
    UpdateWallpaperColor();

    m_host.SetMode(m_config.general.hostMode);
    m_host.Refresh();
    m_host.SetIconsHidden(true);

    RebuildCanvases();
    StartDesktopWatch();
    RequestDesktopScan();
    RefreshAll();

    m_tray = std::make_unique<Tray>(m_msgWnd);
    m_tray->Add();
    RegisterHotkeys();
    UpdateHooks();
    foreground::Install(m_msgWnd);

    // After an update, open "What's new" (PowerToys' SCOOBE). The first start opens the overview from FirstRunSetup.
    bool updated = m_config.general.firstRunDone && m_config.general.lastSeenVersion != OF_VERSION_WSTRING;
    if (m_config.general.lastSeenVersion != OF_VERSION_WSTRING) {
        log::Info(L"Version changed: {} -> {}", m_config.general.lastSeenVersion, OF_VERSION_WSTRING);
        m_config.general.lastSeenVersion = OF_VERSION_WSTRING;
        SaveSoon();
    }
    if (updated && m_config.general.showWhatsNew && !m_noAutoWelcome) PostToUi([this] { OpenWelcome(kWhatsNewPageId); });

    DailyBackupTick();
    SetTimer(m_msgWnd, kTimerDaily, 60 * 60 * 1000, nullptr);
    SetTimer(m_msgWnd, kTimerPrune, 3 * 60 * 1000, nullptr);
    watchdog::Spawn(cmd.restarts, cmd.firstRestartTick);

    if (!cmd.verb.empty()) {
        std::wstring verb = cmd.verb, arg = cmd.arg;
        PostToUi([this, verb, arg] { ExecuteCommand(verb, arg); });
    }
    return true;
}

void Controller::Shutdown() {
    log::Info(L"Shutting down");
    KillTimer(m_msgWnd, kTimerSave);
    DoSave();
    windhook::Uninstall();
    foreground::Uninstall();
    inline_editor::Cancel();
    m_settings.reset();
    m_welcome.reset();
    FinishPageAnim();
    m_fences.clear();
    m_canvases.clear();
    m_portals.clear();
    m_host.SetIconsHidden(false);
    if (m_tray) m_tray->Remove();
    m_tray.reset();
    m_userDesktopWatch.Stop();
    m_publicDesktopWatch.Stop();
    m_rootWatch.Stop();
    m_enumWorker.reset();
    m_portalWorker.reset();
    m_iconWorker.reset();
    if (m_msgWnd) DestroyWindow(m_msgWnd);
    m_msgWnd = nullptr;
}

void Controller::Exit() {
    m_exiting = true;
    PostQuitMessage(0);
}

void Controller::LoadConfig() {
    auto path = paths::ConfigFile();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        log::Info(L"No configuration yet; first run");
        return;
    }
    std::string err;
    if (auto c = LoadConfigFile(path, &err)) {
        m_config = std::move(*c);
        return;
    }
    // Keep the unreadable file for inspection and start fresh.
    auto broken = path;
    broken += L".broken-" + CurrentStamp();
    std::filesystem::rename(path, broken, ec);
    log::Info(L"Configuration unreadable ({}); moved aside", FromUtf8(err));
}

void Controller::ApplyPolicyConfig() {
    if (m_policy.configPath.empty()) return;
    if (auto t = LoadTemplate(m_policy.configPath)) {
        log::Info(L"Loaded policy configuration from {}", m_policy.configPath);
        m_policyTemplate = std::move(*t);
        m_config.general.firstRunDone = true;
    } else {
        log::Info(L"Policy configuration {} unavailable", m_policy.configPath);
    }
}

void Controller::DoSave() {
    if (!SaveConfigFile(paths::ConfigFile(), m_config)) log::Info(L"Saving configuration failed");
}

void Controller::SaveSoon() { SetTimer(m_msgWnd, kTimerSave, 1000, nullptr); }

void Controller::DailyBackupTick() {
    if (!m_config.layout.dailyBackups) return;
    std::wstring today = CurrentDateYmd();
    if (today == m_lastBackupDate) return;
    EnsureDailyBackup(paths::BackupsDir(), m_config, today, 7);
    m_lastBackupDate = today;
}

// ====================================================================== monitors & layouts

void Controller::SelectLayoutForMonitors() {
    std::string key = m_config.layout.perMonitorConfig ? ComputeMonitorConfigHash(m_monitors) : std::string("default");
    if (!m_config.layouts.count(key)) {
        std::string closest = FindClosestLayout(m_config.layouts, m_monitors);
        Layout fresh;
        if (!closest.empty()) {
            fresh = m_config.layouts[closest];
            log::Info(L"New monitor configuration; starting from layout {}", FromUtf8(closest));
        }
        m_config.layouts[key] = std::move(fresh);
    }
    m_layoutKey = key;
    Layout& l = CurrentLayout();
    l.monitors.clear();
    for (const auto& m : m_monitors) l.monitors.push_back(m.id);
    // A policy-pushed configuration is applied once per layout per session (at sign-in and when the
    // monitor arrangement changes), not on every settings change.
    if (m_policyTemplate && m_policyAppliedLayouts.insert(m_layoutKey).second) ApplyTemplate(m_config, m_layoutKey, *m_policyTemplate);
}

const MonitorDesc* Controller::MonitorFor(const Group& g) const {
    if (const MonitorDesc* m = FindMonitor(m_monitors, g.monitor)) return m;
    return PrimaryMonitor(m_monitors);
}

RectI Controller::GroupScreenRect(const Group& g) const {
    const MonitorDesc* m = MonitorFor(g);
    if (!m) return RectI::FromXYWH(100, 100, 300, 240);
    RectI r = DenormalizeRect(g.rect, m->work);
    int minW = MulDiv(120, static_cast<int>(m->dpi), 96), minH = MulDiv(60, static_cast<int>(m->dpi), 96);
    if (r.Width() < minW) r.right = r.left + minW;
    if (r.Height() < minH) r.bottom = r.top + minH;
    return ClampInside(r, m->work);
}

std::vector<RectI> Controller::GroupRectsOnMonitor(const std::wstring& monitorId, const std::string& exceptId) {
    std::vector<RectI> out;
    for (Group* g : VisibleGroups(CurrentLayout())) {
        if (g->id == exceptId) continue;
        const MonitorDesc* m = MonitorFor(*g);
        if (m && EqualsI(m->id, monitorId)) out.push_back(GroupScreenRect(*g));
    }
    return out;
}

SnapOptions Controller::Snap(float scale) const {
    SnapOptions o;
    o.toOthers = m_config.layout.snapToOthers;
    o.toEdges = m_config.layout.snapToEdges;
    o.toGrid = m_config.layout.snapToGrid;
    o.gridSize = std::max(4, static_cast<int>(m_config.layout.gridSize * scale));
    o.threshold = static_cast<int>(10 * scale);
    o.spacing = static_cast<int>(m_config.layout.spacing * scale);
    return o;
}

void Controller::OnDisplayChanged() {
    auto monitors = EnumerateMonitors();
    if (monitors.empty()) return;
    m_monitors = std::move(monitors);
    std::string oldKey = m_layoutKey;
    DoSave();
    SelectLayoutForMonitors();
    m_host.Refresh();
    m_host.SetIconsHidden(true);
    if (oldKey != m_layoutKey) {
        log::Info(L"Monitor configuration changed: {} -> {}", FromUtf8(oldKey), FromUtf8(m_layoutKey));
        FinishPageAnim();
        m_fences.clear();
    }
    RebuildCanvases();
    RefreshAll();
    SaveSoon();
}

void Controller::RebuildCanvases() {
    std::vector<std::unique_ptr<DesktopCanvas>> next;
    for (const auto& m : m_monitors) {
        auto it = std::find_if(m_canvases.begin(), m_canvases.end(), [&](const auto& c) { return c && EqualsI(c->Monitor().id, m.id); });
        if (it != m_canvases.end()) {
            (*it)->SetMonitor(m);
            next.push_back(std::move(*it));
        } else {
            auto canvas = std::make_unique<DesktopCanvas>(m);
            if (canvas->Create()) next.push_back(std::move(canvas));
        }
    }
    m_canvases = std::move(next);
    LayoutLooseItems();
}

void Controller::RestackAll() {
    std::vector<HWND> topFirst;
    std::vector<WindowBase*> all;
    for (auto& f : m_fences) all.push_back(f.get());
    for (auto& f : m_pageAnim.outgoing) all.push_back(f.get());
    for (auto& c : m_canvases) all.push_back(c.get());
    for (WindowBase* w : all) {
        w->AllowZOrder(true);
        topFirst.push_back(w->Hwnd());
    }
    m_host.Restack(topFirst);
    for (WindowBase* w : all) w->AllowZOrder(false);
}

// ====================================================================== windows

FenceWindow* Controller::FenceFor(const std::string& groupId) const {
    for (auto& f : m_fences)
        if (f->GroupId() == groupId) return f.get();
    return nullptr;
}

void Controller::RefreshAll() {
    if (m_peek != PeekKind::None) {
        // Rebuilding while raised would drop windows back to the desktop layer mid-Peek.
        for (auto& f : m_fences) f->SyncFromModel();
    }
    SyncPortals();
    Layout& layout = CurrentLayout();
    std::set<std::string> visible;
    for (Group* g : VisibleGroups(layout)) {
        visible.insert(g->id);
        if (FenceWindow* f = FenceFor(g->id)) {
            f->SyncFromModel();
        } else {
            auto fence = std::make_unique<FenceWindow>(g->id);
            if (fence->Create()) m_fences.push_back(std::move(fence));
        }
    }
    std::erase_if(m_fences, [&](const auto& f) { return !visible.count(f->GroupId()); });
    ApplyHiddenState(false);
    LayoutLooseItems();
    if (m_peek == PeekKind::None) RestackAll();
    InvalidateAll();
}

void Controller::SyncItems() {
    for (auto& f : m_fences) f->SyncFromModel();
    LayoutLooseItems();
}

void Controller::InvalidateAll() {
    for (auto& f : m_fences) f->Invalidate();
    for (auto& c : m_canvases) c->Invalidate();
}

void Controller::ApplyHiddenState(bool animate) {
    bool hidden = Hidden();
    Layout& layout = CurrentLayout();
    for (auto& f : m_fences) {
        Group* g = FindGroup(layout, f->GroupId());
        bool show = !hidden || (g && g->hideExclude);
        f->SetShown(show, animate);
    }
    for (auto& c : m_canvases) c->SetItemsShown(!hidden, animate);
}

bool Controller::Hidden() const {
    return (m_config.hide.hidden || m_autoHidden) && m_peek == PeekKind::None;
}

FenceWindow* Controller::FenceAtTitle(POINT screen, const FenceWindow* except) const {
    for (auto& f : m_fences)
        if (f.get() != except && f->TitleContains(screen)) return f.get();
    return nullptr;
}

void Controller::HighlightMergeTarget(const std::string& groupId) {
    for (auto& f : m_fences) f->SetMergeHighlight(!groupId.empty() && f->GroupId() == groupId);
}

bool Controller::IsOurWindow(HWND hwnd) const {
    if (!hwnd) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid == GetCurrentProcessId();
}

int Controller::PageCount() { return static_cast<int>(CurrentLayout().pages.size()); }

bool Controller::IsLayoutLocked(const Group* g) const {
    bool global = m_policy.lockLayout.value_or(m_config.lock.layoutLocked);
    return global || (g && g->locked);
}

bool Controller::IsContentLocked(const Group* g) const {
    bool global = m_policy.lockContent.value_or(m_config.lock.contentLocked);
    return global || (g && g->contentLocked);
}

// ====================================================================== styling

ItemViewStyle Controller::StyleFor(const Group* g, const Tab* t, float scale) const {
    const AppearanceSettings& ap = m_config.appearance;
    ItemViewStyle s;
    int dip = (t && t->iconSize) ? t->iconSize : ap.iconSize;
    s.iconPx = static_cast<int>(dip * scale);
    // List layouts are a Folder Portal feature.
    s.view = (t && t->kind == TabKind::Portal) ? t->view : ViewMode::Icons;
    s.scale = scale;
    s.fontPx = 12.f * scale;
    s.tint = (g && g->iconTint) ? g->iconTint : ap.iconTint;
    s.chameleon = m_config.chameleon.enabled;
    s.chameleonOpacity = m_config.chameleon.opacity / 100.f;
    s.desaturate = m_config.chameleon.desaturate;
    return s;
}

Color Controller::BackgroundFor(const Group& g) const {
    const AppearanceSettings& ap = m_config.appearance;
    Color c = g.background.value_or(ap.background);
    if (!g.background && ap.wallpaperAware && m_wallpaperColor) {
        c.r = static_cast<uint8_t>(m_wallpaperColor->r * 0.55);
        c.g = static_cast<uint8_t>(m_wallpaperColor->g * 0.55);
        c.b = static_cast<uint8_t>(m_wallpaperColor->b * 0.55);
    }
    if (ap.tabColors) {
        if (const Tab* t = g.ActiveTab(); t && t->color) {
            auto mix = [](uint8_t a, uint8_t b) { return static_cast<uint8_t>((a * 65 + b * 35) / 100); };
            c = {mix(c.r, t->color->r), mix(c.g, t->color->g), mix(c.b, t->color->b), c.a};
        }
    }
    return c;
}

void Controller::UpdateWallpaperColor() {
    m_wallpaperColor.reset();
    if (!m_config.appearance.wallpaperAware) return;
    const MonitorDesc* m = PrimaryMonitor(m_monitors);
    if (!m) return;
    WallpaperInfo info = GetWallpaperFor(m->bounds);
    m_wallpaperColor = AverageImageColor(info.path);
    if (!m_wallpaperColor) m_wallpaperColor = info.background;
}

// ====================================================================== desktop items

ShellItemPtr Controller::DesktopItem(const std::wstring& key) const {
    auto it = m_items.find(key);
    return it == m_items.end() ? nullptr : it->second;
}

void Controller::SortItems(std::vector<ShellItemPtr>& items, SortMode mode, bool foldersFirst) const {
    auto byName = [](const ShellItemPtr& a, const ShellItemPtr& b) { return StrCmpLogicalW(a->displayName.c_str(), b->displayName.c_str()) < 0; };
    auto folderRank = [&](const ShellItemPtr& a) { return foldersFirst && a->isFolder ? 0 : 1; };
    switch (mode) {
        case SortMode::Manual:
            if (foldersFirst) {
                std::stable_sort(items.begin(), items.end(), [&](const auto& a, const auto& b) {
                    if (folderRank(a) != folderRank(b)) return folderRank(a) < folderRank(b);
                    return byName(a, b);
                });
            }
            break;
        case SortMode::Name:
            std::stable_sort(items.begin(), items.end(), [&](const auto& a, const auto& b) {
                if (folderRank(a) != folderRank(b)) return folderRank(a) < folderRank(b);
                return byName(a, b);
            });
            break;
        case SortMode::Type:
            std::stable_sort(items.begin(), items.end(), [&](const auto& a, const auto& b) {
                if (a->isFolder != b->isFolder) return a->isFolder;
                int c = _wcsicmp(a->typeName.c_str(), b->typeName.c_str());
                return c != 0 ? c < 0 : byName(a, b);
            });
            break;
        case SortMode::Date:
            std::stable_sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a->modified > b->modified; });
            break;
    }
}

std::vector<ShellItemPtr> Controller::TabItems(const Tab& tab) {
    std::vector<ShellItemPtr> out;
    if (tab.kind == TabKind::Portal) {
        if (PortalModel* pm = Portal(tab.id)) out = pm->items;
        SortItems(out, tab.sort, true);
        return out;
    }
    for (const auto& key : tab.items)
        if (auto item = DesktopItem(key)) out.push_back(item);
    SortItems(out, tab.sort, false);
    return out;
}

void Controller::StartDesktopWatch() {
    m_userDesktopWatch.Stop();
    m_publicDesktopWatch.Stop();
    m_rootWatch.Stop();
    if (auto p = ParsePath(m_desktopPath)) m_userDesktopWatch.Start(m_msgWnd, WM_APP_DESKTOP_CHANGE, p.get(), false);
    if (auto p = ParsePath(m_publicDesktopPath)) m_publicDesktopWatch.Start(m_msgWnd, WM_APP_DESKTOP_CHANGE, p.get(), false);
    ITEMIDLIST root{};
    m_rootWatch.Start(m_msgWnd, WM_APP_DESKTOP_CHANGE, &root, false);
}

void Controller::RequestDesktopScan() {
    // Follow a redirected/moved Desktop folder (OneDrive backup, folder relocation).
    std::wstring desktop = KnownFolderPath(FOLDERID_Desktop);
    if (!desktop.empty() && !EqualsI(desktop, m_desktopPath)) {
        log::Info(L"Desktop folder moved: {} -> {}", m_desktopPath, desktop);
        m_desktopPath = desktop;
        StartDesktopWatch();
    }
    if (m_scanPending) {
        m_scanQueued = true;
        return;
    }
    m_scanPending = true;
    HiddenSettings hidden = ReadHiddenSettings();
    m_enumWorker->Post([hidden] {
        EnumResult r = EnumerateDesktop(hidden);
        if (FAILED(r.hr)) log::Info(L"Desktop enumeration failed 0x{:08x}", static_cast<unsigned>(r.hr));
        auto items = std::move(r.items);
        PostToUi([items] { Controller::Get().ApplyDesktopScan(items); });
    });
}

void Controller::NoteRename(const std::wstring& oldKey, const std::wstring& newKey) {
    if (oldKey == newKey) return;
    Layout& layout = CurrentLayout();
    for (auto& [k, l] : m_config.layouts) {
        for (Group* g : AllGroups(l))
            for (Tab& t : g->tabs) std::replace(t.items.begin(), t.items.end(), oldKey, newKey);
        if (auto it = l.loose.find(oldKey); it != l.loose.end()) {
            LoosePos pos = it->second;
            l.loose.erase(it);
            l.loose[newKey] = pos;
        }
    }
    std::replace(m_config.hide.excludeItems.begin(), m_config.hide.excludeItems.end(), oldKey, newKey);
    (void)layout;
    SaveSoon();
}

void Controller::ApplyDesktopScan(std::vector<ShellItemPtr> items) {
    m_scanPending = false;
    for (const auto& [oldKey, newKey] : m_renames) NoteRename(oldKey, newKey);
    m_renames.clear();

    std::map<std::wstring, ShellItemPtr> next;
    std::vector<std::wstring> order;
    std::vector<ShellItemPtr> fresh;
    for (auto& item : items) {
        if (next.count(item->key)) continue;
        if (m_haveScan && !m_items.count(item->key)) fresh.push_back(item);
        order.push_back(item->key);
        next[item->key] = item;
    }
    m_items = std::move(next);
    m_itemOrder = std::move(order);
    bool first = !m_haveScan;
    m_haveScan = true;
    log::Debug(L"Desktop scan: {} items, {} new", m_items.size(), fresh.size());

    if (first && !m_config.general.firstRunDone) FirstRunSetup();
    else if (!fresh.empty()) HandleNewItems(fresh);
    SyncItems();
    if (m_scanQueued) {
        m_scanQueued = false;
        RequestDesktopScan();
    }
}

void Controller::OnDesktopChange(WPARAM wp, LPARAM lp) {
    bool iconsChanged = false;
    DecodeChangeNotification(wp, lp, [&](LONG event, PCIDLIST_ABSOLUTE a, PCIDLIST_ABSOLUTE b) {
        if ((event & (SHCNE_RENAMEITEM | SHCNE_RENAMEFOLDER)) && a && b) {
            std::wstring from = FileSystemPath(a), to = FileSystemPath(b);
            auto onDesktop = [&](const std::wstring& p) { return EqualsI(ParentDir(p), m_desktopPath) || EqualsI(ParentDir(p), m_publicDesktopPath); };
            if (!from.empty() && !to.empty() && onDesktop(from) && onDesktop(to)) m_renames[FileNameOf(from)] = FileNameOf(to);
        }
        if ((event & SHCNE_UPDATEITEM) && a) m_icons->Invalidate(ParsingName(a));
        if (event & (SHCNE_UPDATEIMAGE | SHCNE_ASSOCCHANGED)) iconsChanged = true;
    });
    if (iconsChanged) {
        m_icons->MarkAllStale();
        InvalidateAll();
    }
    SetTimer(m_msgWnd, kTimerScan, 400, nullptr);
}

std::vector<std::wstring> Controller::RulesTargetsApply(const std::vector<ShellItemPtr>& candidates) {
    std::vector<std::wstring> assigned;
    Layout& layout = CurrentLayout();
    std::map<std::wstring, std::vector<ShellItemPtr>> moves;
    uint64_t now = NowTicks();
    for (const auto& item : candidates) {
        if (FindItemOwner(layout, item->key).first) continue;
        const Rule* rule = EvaluateRules(m_config.rules, ToRuleInfo(*item), now);
        if (!rule) continue;
        if (rule->action == RuleAction::MoveToFolder) {
            if (!rule->targetFolder.empty() && item->isFileSystem) moves[rule->targetFolder].push_back(item);
            continue;
        }
        // Rules are set up by the user or administrator, so they may fill content-locked fences.
        auto [g, t] = FindTab(layout, rule->targetTab);
        if (!t || t->kind != TabKind::Items) continue;
        t->items.push_back(item->key);
        layout.loose.erase(item->key);
        assigned.push_back(item->key);
    }
    for (auto& [folder, list] : moves) {
        log::Info(L"Rules: moving {} item(s) to {}", list.size(), folder);
        MoveItemsToFolder(m_msgWnd, list, folder);
    }
    if (!assigned.empty() || !moves.empty()) SaveSoon();
    return assigned;
}

void Controller::HandleNewItems(const std::vector<ShellItemPtr>& fresh) {
    ULONGLONG now = GetTickCount64();
    std::erase_if(m_pendingDrops, [&](const PendingDrop& p) { return p.expires < now; });
    Layout& layout = CurrentLayout();
    std::vector<ShellItemPtr> forRules;
    for (const auto& item : fresh) {
        if (FindItemOwner(layout, item->key).first) continue;  // e.g. Office re-creating a file during save
        std::wstring name = item->isFileSystem ? FileNameOf(item->parsingName) : item->key;
        auto it = std::find_if(m_pendingDrops.begin(), m_pendingDrops.end(), [&](const PendingDrop& p) { return EqualsI(p.name, name); });
        if (it != m_pendingDrops.end()) {
            if (!it->tabId.empty()) {
                auto [g, t] = FindTab(layout, it->tabId);
                if (t && t->kind == TabKind::Items) t->items.push_back(item->key);
            } else {
                layout.loose[item->key] = LoosePos{it->monitorId, it->cell.x, it->cell.y};
            }
            m_pendingDrops.erase(it);
            SaveSoon();
            continue;
        }
        if (layout.loose.count(item->key)) continue;
        forRules.push_back(item);
    }
    RulesTargetsApply(forRules);
}

void Controller::ExpectDroppedFiles(const std::vector<std::wstring>& names, const std::string& tabId, const std::wstring& monitorId, PointI cell) {
    ULONGLONG expires = GetTickCount64() + 60'000;
    std::vector<std::wstring> already;
    int i = 0;
    for (const auto& name : names) {
        // Already on the desktop (e.g. dragged from an Explorer window showing the Desktop folder).
        if (DesktopItem(name)) {
            already.push_back(name);
            continue;
        }
        m_pendingDrops.push_back(PendingDrop{name, tabId, monitorId, PointI{cell.x, cell.y + i++}, expires});
    }
    if (!already.empty()) {
        if (!tabId.empty()) AssignItems(already, tabId, -1);
        else MakeLoose(already, monitorId, cell);
    }
}

void Controller::FirstRunSetup() {
    log::Info(L"First run: building the default layout");
    TakeSnapshot(L"Before openFences");
    Layout& layout = CurrentLayout();
    bool empty = layout.persistent.empty();
    for (const Page& p : layout.pages) empty = empty && p.groups.empty();
    if (empty) {
        const MonitorDesc* primary = PrimaryMonitor(m_monitors);
        BuildDefaultLayout(m_config, m_layoutKey, primary ? primary->id : L"");
    }
    std::vector<ShellItemPtr> all;
    for (const auto& key : m_itemOrder) all.push_back(m_items[key]);
    RulesTargetsApply(all);
    m_config.general.firstRunDone = true;
    RefreshAll();
    SaveSoon();
    // First start: show the built-in tour, like PowerToys' OOBE.
    if (!m_noAutoWelcome) PostToUi([this] { OpenWelcome(L"welcome"); });
}

void Controller::PruneMissingMembers() {
    if (!m_haveScan) return;
    bool changed = false;
    for (auto& [key, layout] : m_config.layouts) {
        if (key != m_layoutKey) continue;
        for (Group* g : AllGroups(layout)) {
            for (Tab& t : g->tabs) {
                size_t before = t.items.size();
                std::erase_if(t.items, [&](const std::wstring& k) { return !m_items.count(k); });
                changed = changed || before != t.items.size();
            }
        }
        for (auto it = layout.loose.begin(); it != layout.loose.end();) {
            if (!m_items.count(it->first)) {
                it = layout.loose.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
    }
    if (changed) SaveSoon();
}

void Controller::LayoutLooseItems() {
    if (m_canvases.empty()) return;
    Layout& layout = CurrentLayout();
    std::set<std::wstring> owned;
    for (Group* g : AllGroups(layout))
        for (const Tab& t : g->tabs)
            if (t.kind == TabKind::Items) owned.insert(t.items.begin(), t.items.end());

    struct CanvasState {
        DesktopCanvas* canvas;
        std::set<std::pair<int, int>> occupied;
        std::vector<ShellItemPtr> items;
        std::vector<PointI> cells;
        int rows = 1;
    };
    std::vector<CanvasState> states;
    for (auto& c : m_canvases) {
        CanvasState s{c.get(), {}, {}, {}, 1};
        ItemView& view = c->View();
        GridSpec spec = view.CellSpec();
        RectI client = c->ClientRect();
        s.rows = std::max(1, (client.Height() - 2 * spec.padding) / std::max(1, spec.cellH));
        int cols = std::max(1, (client.Width() - 2 * spec.padding) / std::max(1, spec.cellW));
        RectI screen = c->ScreenRect();
        // Cells covered by fences are unavailable for loose icons.
        for (Group* g : VisibleGroups(layout)) {
            RectI r = GroupScreenRect(*g).Offset(-screen.left, -screen.top);
            if (!r.Intersects(client)) continue;
            for (int col = 0; col < cols; ++col)
                for (int row = 0; row < s.rows; ++row)
                    if (view.CellRect({col, row}).Intersects(r)) s.occupied.insert({col, row});
        }
        states.push_back(std::move(s));
    }
    auto stateFor = [&](const std::wstring& monitorId) -> CanvasState* {
        for (auto& s : states)
            if (EqualsI(s.canvas->Monitor().id, monitorId)) return &s;
        return nullptr;
    };
    CanvasState* primary = nullptr;
    for (auto& s : states)
        if (s.canvas->Monitor().primary) primary = &s;
    if (!primary) primary = &states.front();

    std::vector<ShellItemPtr> unplaced;
    for (const auto& key : m_itemOrder) {
        if (owned.count(key)) continue;
        ShellItemPtr item = m_items[key];
        auto pos = layout.loose.find(key);
        if (pos != layout.loose.end()) {
            CanvasState* s = stateFor(pos->second.monitor);
            if (s && pos->second.row < s->rows && !s->occupied.count({pos->second.col, pos->second.row})) {
                s->occupied.insert({pos->second.col, pos->second.row});
                s->items.push_back(item);
                s->cells.push_back({pos->second.col, pos->second.row});
                continue;
            }
        }
        unplaced.push_back(item);
    }
    // Auto-arrange the rest column by column from the top-left of the primary monitor, like Explorer.
    int col = 0, row = 0;
    for (const auto& item : unplaced) {
        while (primary->occupied.count({col, row})) {
            if (++row >= primary->rows) {
                row = 0;
                ++col;
            }
        }
        primary->occupied.insert({col, row});
        primary->items.push_back(item);
        primary->cells.push_back({col, row});
    }
    for (auto& s : states) s.canvas->SetItems(std::move(s.items), std::move(s.cells));
}

// ====================================================================== membership

Group* Controller::CreateGroup(const RectI& screen, bool portal, const std::vector<std::wstring>& keys, bool rename) {
    if (IsLayoutLocked(nullptr)) return nullptr;
    const MonitorDesc* mon = MonitorForRect(m_monitors, screen);
    if (!mon) return nullptr;
    RectI r = ClampInside(screen, mon->work);
    RectF f = NormalizeRect(r, mon->work);
    Layout& layout = CurrentLayout();
    Group g;
    if (portal) {
        std::wstring path = PickFolder(m_msgWnd, L"Choose the folder this portal shows");
        if (path.empty()) return nullptr;
        std::wstring title = FileNameOf(path);
        if (title.empty()) title = path;
        g = MakePortalGroup(title, path, f, mon->id, m_config.portals.defaultView);
        g.tabs[0].allowNavigation = m_config.portals.allowNavigation;
    } else {
        for (const auto& k : keys) {
            RemoveItemEverywhere(layout, k);
            layout.loose.erase(k);
        }
        g = MakeGroup(L"New Fence", f, mon->id);
        g.tabs[0].items = keys;
    }
    std::string id = g.id;
    EnsurePage(layout).groups.push_back(std::move(g));
    RefreshAll();
    SaveSoon();
    if (rename && !portal)
        if (FenceWindow* w = FenceFor(id)) w->BeginTitleRename();
    return FindGroup(CurrentLayout(), id);
}

void Controller::DeleteGroup(const std::string& groupId, bool confirm) {
    Layout& layout = CurrentLayout();
    Group* g = FindGroup(layout, groupId);
    if (!g || IsLayoutLocked(g)) return;
    if (confirm) {
        bool portal = g->ActiveTab() && g->ActiveTab()->kind == TabKind::Portal;
        std::wstring msg = L"Delete the fence \"" + (g->tabs.empty() ? std::wstring() : g->tabs[0].title) + L"\"?\n\n" +
                           (portal ? L"The folder and its files are not affected." : L"Its icons stay on the desktop.");
        if (MessageBoxW(m_msgWnd, msg.c_str(), L"openFences", MB_OKCANCEL | MB_ICONQUESTION | MB_SETFOREGROUND) != IDOK) return;
    }
    RemoveGroup(layout, groupId);
    RefreshAll();
    SaveSoon();
}

void Controller::SetGroupRect(const std::string& groupId, const RectI& screen) {
    Group* g = FindGroup(CurrentLayout(), groupId);
    if (!g) return;
    const MonitorDesc* mon = MonitorForRect(m_monitors, screen);
    if (!mon) return;
    g->monitor = mon->id;
    g->rect = NormalizeRect(ClampInside(screen, mon->work), mon->work);
    if (FenceWindow* f = FenceFor(groupId)) f->ApplyGeometry();
    LayoutLooseItems();
    SaveSoon();
}

void Controller::AssignItems(const std::vector<std::wstring>& keys, const std::string& tabId, int insertIndex) {
    Layout& layout = CurrentLayout();
    auto [g, t] = FindTab(layout, tabId);
    if (!t || t->kind != TabKind::Items || keys.empty()) return;
    // Anchor the insertion on the first displayed item that is not being moved.
    std::wstring anchor;
    if (insertIndex >= 0) {
        auto shown = TabItems(*t);
        for (int i = insertIndex; i < static_cast<int>(shown.size()); ++i) {
            if (std::find(keys.begin(), keys.end(), shown[static_cast<size_t>(i)]->key) == keys.end()) {
                anchor = shown[static_cast<size_t>(i)]->key;
                break;
            }
        }
    }
    for (const auto& k : keys) {
        RemoveItemEverywhere(layout, k);
        layout.loose.erase(k);
    }
    auto pos = anchor.empty() ? t->items.end() : std::find(t->items.begin(), t->items.end(), anchor);
    t->items.insert(pos, keys.begin(), keys.end());
    if (insertIndex >= 0) t->sort = SortMode::Manual;
    SyncItems();
    if (FenceWindow* f = FenceFor(g->id)) f->SelectKeys(keys);
    SaveSoon();
}

void Controller::MakeLoose(const std::vector<std::wstring>& keys, const std::wstring& monitorId, PointI cell) {
    Layout& layout = CurrentLayout();
    int i = 0;
    for (const auto& k : keys) {
        RemoveItemEverywhere(layout, k);
        if (monitorId.empty()) layout.loose.erase(k);
        else layout.loose[k] = LoosePos{monitorId, cell.x, cell.y + i++};
    }
    SyncItems();
    for (auto& c : m_canvases)
        if (EqualsI(c->Monitor().id, monitorId)) c->SelectKeys(keys);
    SaveSoon();
}

void Controller::MergeGroups(const std::string& sourceId, const std::string& targetId) {
    Layout& layout = CurrentLayout();
    Group* target = FindGroup(layout, targetId);
    Group* source = FindGroup(layout, sourceId);
    if (!target || !source || IsLayoutLocked(target) || IsLayoutLocked(source)) {
        RefreshAll();
        return;
    }
    std::optional<Group> src = TakeGroup(layout, sourceId);
    target = FindGroup(layout, targetId);
    int first = static_cast<int>(target->tabs.size());
    for (Tab& t : src->tabs) target->tabs.push_back(std::move(t));
    target->activeTab = first;
    RefreshAll();
    SaveSoon();
}

void Controller::DetachTab(const std::string& groupId, int tabIndex, const RectI& screen) {
    Layout& layout = CurrentLayout();
    Group* g = FindGroup(layout, groupId);
    if (!g || tabIndex < 0 || tabIndex >= static_cast<int>(g->tabs.size())) return;
    if (g->tabs.size() <= 1 || IsLayoutLocked(g)) {
        SetGroupRect(groupId, screen);
        return;
    }
    Tab tab = std::move(g->tabs[static_cast<size_t>(tabIndex)]);
    g->tabs.erase(g->tabs.begin() + tabIndex);
    g->activeTab = std::clamp(g->activeTab >= tabIndex ? g->activeTab - 1 : g->activeTab, 0, static_cast<int>(g->tabs.size()) - 1);
    bool persistent = std::any_of(layout.persistent.begin(), layout.persistent.end(), [&](const Group& p) { return p.id == groupId; });
    const MonitorDesc* mon = MonitorForRect(m_monitors, screen);
    Group fresh;
    fresh.id = NewId("g");
    fresh.monitor = mon ? mon->id : L"";
    fresh.rect = mon ? NormalizeRect(ClampInside(screen, mon->work), mon->work) : g->rect;
    fresh.tabs.push_back(std::move(tab));
    if (persistent) layout.persistent.push_back(std::move(fresh));
    else EnsurePage(layout).groups.push_back(std::move(fresh));
    RefreshAll();
    SaveSoon();
}

void Controller::ReorderTab(const std::string& groupId, int from, int to) {
    Group* g = FindGroup(CurrentLayout(), groupId);
    if (!g || from == to || from < 0 || to < 0 || from >= static_cast<int>(g->tabs.size()) || to >= static_cast<int>(g->tabs.size())) return;
    Tab moved = std::move(g->tabs[static_cast<size_t>(from)]);
    g->tabs.erase(g->tabs.begin() + from);
    g->tabs.insert(g->tabs.begin() + to, std::move(moved));
    g->activeTab = to;
    if (FenceWindow* f = FenceFor(groupId)) f->SyncFromModel();
    SaveSoon();
}

void Controller::RenameItemInteractive(HWND owner, const ShellItemPtr& item, const RectI& label, float fontPx) {
    if (!item) return;
    bool desktop = DesktopItem(item->key) == item;
    int h = static_cast<int>(fontPx * 1.8f) + 4;
    RectI r = RectI::FromXYWH(label.left - 10, label.top, label.Width() + 20, h);
    inline_editor::Show(owner, r, item->editName, fontPx, !item->isFolder, [this, owner, item, desktop](const std::wstring& text) {
        std::wstring name = Trim(text);
        if (name.empty() || name == item->editName) return;
        auto newKey = RenameItem(owner, *item, name, desktop);
        if (!newKey) return;
        if (desktop) {
            NoteRename(item->key, *newKey);
            RequestDesktopScan();
        } else {
            for (auto& [id, pm] : m_portals)
                if (!pm->stack.empty()) LoadPortal(*pm);
        }
    });
}

// ====================================================================== portals

PortalModel* Controller::Portal(const std::string& tabId) {
    auto it = m_portals.find(tabId);
    return it == m_portals.end() ? nullptr : it->second.get();
}

void Controller::SyncPortals() {
    std::set<std::string> wanted;
    for (Group* g : VisibleGroups(CurrentLayout())) {
        for (const Tab& t : g->tabs) {
            if (t.kind != TabKind::Portal) continue;
            wanted.insert(t.id);
            auto& slot = m_portals[t.id];
            if (slot && slot->rootPath == t.path) continue;
            slot = std::make_unique<PortalModel>();
            slot->tabId = t.id;
            slot->rootPath = t.path;
            // Pick a free notification message for this portal.
            std::set<UINT> used;
            for (auto& [id, pm] : m_portals)
                if (pm && pm.get() != slot.get()) used.insert(pm->watchMsg);
            for (UINT m = WM_APP_PORTAL_FIRST; m <= WM_APP_PORTAL_LAST; ++m)
                if (!used.count(m)) {
                    slot->watchMsg = m;
                    break;
                }
            LoadPortal(*slot);
        }
    }
    for (auto it = m_portals.begin(); it != m_portals.end();) {
        if (!wanted.count(it->first)) {
            KillTimer(m_msgWnd, kTimerPortalRetryBase + (it->second->watchMsg - WM_APP_PORTAL_FIRST));
            it = m_portals.erase(it);
        } else {
            ++it;
        }
    }
}

void Controller::LoadPortal(PortalModel& model) {
    if (model.stack.empty()) {
        if (auto root = ParsePath(model.rootPath)) model.stack.push_back(root);
    }
    UINT_PTR timer = kTimerPortalRetryBase + (model.watchMsg - WM_APP_PORTAL_FIRST);
    KillTimer(m_msgWnd, timer);
    if (model.stack.empty()) {
        model.state = PortalModel::State::Offline;
        model.retryDelayMs = std::clamp(model.retryDelayMs ? model.retryDelayMs * 2 : 5000, 5000, 300000);
        SetTimer(m_msgWnd, timer, static_cast<UINT>(model.retryDelayMs), nullptr);
        SyncItems();
        return;
    }
    if (model.items.empty()) model.state = PortalModel::State::Loading;
    SharedPidl folder = model.stack.back();
    uint64_t request = ++model.requestId;
    std::string tabId = model.tabId;
    HiddenSettings hidden = ReadHiddenSettings();
    m_portalWorker->Post([folder, request, tabId, hidden] {
        EnumResult r = EnumerateFolder(folder.get(), hidden);
        PWSTR name = nullptr;
        std::wstring folderName;
        if (SUCCEEDED(SHGetNameFromIDList(folder.get(), SIGDN_NORMALDISPLAY, &name))) folderName = name;
        CoTaskMemFree(name);
        HRESULT hr = r.hr;
        auto items = std::move(r.items);
        PostToUi([folder, request, tabId, hr, items, folderName] {
            Controller& c = Controller::Get();
            PortalModel* pm = c.Portal(tabId);
            if (!pm || pm->requestId != request) return;
            UINT_PTR timer = kTimerPortalRetryBase + (pm->watchMsg - WM_APP_PORTAL_FIRST);
            if (FAILED(hr)) {
                pm->state = PortalModel::State::Offline;
                pm->items.clear();
                pm->retryDelayMs = std::clamp(pm->retryDelayMs ? pm->retryDelayMs * 2 : 5000, 5000, 300000);
                SetTimer(c.m_msgWnd, timer, static_cast<UINT>(pm->retryDelayMs), nullptr);
                log::Debug(L"Portal {} offline (0x{:08x})", pm->rootPath, static_cast<unsigned>(hr));
            } else {
                pm->state = PortalModel::State::Ready;
                pm->items = items;
                pm->folderName = folderName;
                pm->retryDelayMs = 0;
                pm->watch.Start(c.m_msgWnd, pm->watchMsg, folder.get(), false);
            }
            c.SyncItems();
        });
    });
}

void Controller::OnPortalChange(UINT msg, WPARAM wp, LPARAM lp) {
    DecodeChangeNotification(wp, lp, [](LONG, PCIDLIST_ABSOLUTE, PCIDLIST_ABSOLUTE) {});
    for (auto& [id, pm] : m_portals) {
        if (pm->watchMsg == msg) {
            // Debounce bursts (copying many files) into one refresh.
            SetTimer(m_msgWnd, kTimerPortalRetryBase + (msg - WM_APP_PORTAL_FIRST), 300, nullptr);
            pm->retryDelayMs = 0;
        }
    }
}

void Controller::PortalNavigate(const std::string& tabId, const ShellItemPtr& folder) {
    PortalModel* pm = Portal(tabId);
    if (!pm || !folder) return;
    pm->stack.push_back(folder->pidl);
    pm->items.clear();
    pm->watch.Stop();
    LoadPortal(*pm);
    SyncItems();
}

void Controller::PortalBack(const std::string& tabId) {
    PortalModel* pm = Portal(tabId);
    if (!pm || pm->stack.size() <= 1) return;
    pm->stack.pop_back();
    pm->items.clear();
    pm->watch.Stop();
    LoadPortal(*pm);
    SyncItems();
}

void Controller::PortalRefresh(const std::string& tabId) {
    if (PortalModel* pm = Portal(tabId)) LoadPortal(*pm);
}

// ====================================================================== hotkeys & hooks

void Controller::UnregisterHotkeys() {
    for (int id : {kHotkeyPeek, kHotkeyDesktopDisplay, kHotkeyNextPage, kHotkeyPrevPage}) UnregisterHotKey(m_msgWnd, id);
}

void Controller::RegisterHotkeys() {
    UnregisterHotkeys();
    m_peekHotkey = {};
    m_ddHotkey = {};
    if (m_config.peek.enabled) {
        // A user-chosen shortcut is used as is; "automatic" walks the candidates until one is free.
        std::optional<Hotkey> chosen = ParseHotkey(m_config.peek.hotkey);
        std::vector<Hotkey> candidates = chosen ? std::vector<Hotkey>{*chosen} : PeekHotkeyCandidates();
        for (const Hotkey& key : candidates) {
            if (RegisterHotKey(m_msgWnd, kHotkeyPeek, key.modifiers | MOD_NOREPEAT, key.vk)) {
                m_peekHotkey = key;
                break;
            }
            log::Info(L"Peek hotkey {} unavailable", FormatHotkey(key));
        }
        if (m_peekHotkey.Valid()) log::Info(L"Peek hotkey: {}", FormatHotkey(m_peekHotkey));
    }
    if (m_config.desktopDisplay.enabled) {
        if (auto hk = ParseHotkey(m_config.desktopDisplay.hotkey)) {
            if (RegisterHotKey(m_msgWnd, kHotkeyDesktopDisplay, hk->modifiers | MOD_NOREPEAT, hk->vk)) m_ddHotkey = *hk;
            else log::Info(L"Desktop Display hotkey {} unavailable", FormatHotkey(*hk));
        }
    }
}

void Controller::UpdateHooks() {
    if (m_config.desktopDisplay.enabled && m_config.desktopDisplay.interceptWinD) windhook::Install(m_msgWnd);
    else windhook::Uninstall();
    if (m_config.peek.enabled && m_config.peek.hotCornerEnabled) SetTimer(m_msgWnd, kTimerHotCorner, 100, nullptr);
    else KillTimer(m_msgWnd, kTimerHotCorner);
}

void Controller::HotCornerTick() {
    POINT pt;
    if (!GetCursorPos(&pt)) return;
    const MonitorDesc* primary = PrimaryMonitor(m_monitors);
    int size = primary ? MulDiv(3, static_cast<int>(primary->dpi), 96) : 3;
    RectI corner = HotCornerRect(m_config.peek.corner, m_monitors, size);
    bool inside = corner.Contains({pt.x, pt.y});
    if (!inside) {
        m_hotCornerDwell = 0;
        m_hotCornerArmed = true;
        return;
    }
    if (m_hotCornerArmed && ++m_hotCornerDwell >= 3) {
        m_hotCornerArmed = false;
        TogglePeek(PeekKind::Peek);
    }
}

void Controller::UpdateAutoHide(HWND fg) {
    if (!m_config.hide.autoHide) {
        if (m_autoHidden) {
            m_autoHidden = false;
            ApplyHiddenState(true);
        }
        return;
    }
    bool desktopActive = !fg || m_host.IsDesktopWindow(fg) || IsOurWindow(fg);
    bool hide = !desktopActive;
    if (hide != m_autoHidden) {
        m_autoHidden = hide;
        ApplyHiddenState(true);
    }
}

// ====================================================================== message window

LRESULT CALLBACK Controller::MsgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Controller& c = Get();
    if (c.m_msgWnd == hwnd) return c.HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT Controller::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    if (HandleUiInvoke(msg, wp, lp)) return 0;
    if (msg == m_taskbarCreatedMsg && msg != 0) {
        log::Info(L"Explorer restarted; re-attaching to the desktop");
        m_host.Refresh();
        m_host.SetIconsHidden(true);
        FinishPageAnim();
        m_fences.clear();
        m_canvases.clear();
        RebuildCanvases();
        RefreshAll();
        if (m_tray) m_tray->Add();
        StartDesktopWatch();
        RequestDesktopScan();
        return 0;
    }
    if (msg >= WM_APP_PORTAL_FIRST && msg <= WM_APP_PORTAL_LAST) {
        OnPortalChange(msg, wp, lp);
        return 0;
    }
    switch (msg) {
        case WM_APP_DESKTOP_CHANGE:
            OnDesktopChange(wp, lp);
            return 0;
        case WM_APP_TRAY:
            switch (LOWORD(lp)) {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                    OnTrayClick();
                    break;
                case WM_CONTEXTMENU:
                    ShowTrayMenu();
                    break;
            }
            return 0;
        case WM_HOTKEY: {
            int id = static_cast<int>(wp);
            if (id == kHotkeyPeek || id == kHotkeyDesktopDisplay) {
                PeekKind kind = id == kHotkeyPeek ? PeekKind::Peek : PeekKind::DesktopDisplay;
                TriggerMode mode = id == kHotkeyPeek ? m_config.peek.mode : m_config.desktopDisplay.mode;
                const Hotkey& hk = id == kHotkeyPeek ? m_peekHotkey : m_ddHotkey;
                if (mode == TriggerMode::Hold) {
                    if (m_peek == PeekKind::None) StartPeek(kind, true, hk.vk);
                } else {
                    TogglePeek(kind);
                }
            } else if (id == kHotkeyNextPage) {
                NextPage(1);
            } else if (id == kHotkeyPrevPage) {
                NextPage(-1);
            }
            return 0;
        }
        case WM_APP_WIN_D:
            if (m_config.desktopDisplay.mode == TriggerMode::Hold) {
                if (m_peek == PeekKind::None) StartPeek(PeekKind::DesktopDisplay, true, 'D');
            } else {
                TogglePeek(PeekKind::DesktopDisplay);
            }
            return 0;
        case WM_APP_FOREGROUND: {
            HWND fg = reinterpret_cast<HWND>(wp);
            if (m_peek != PeekKind::None && fg && !IsOurWindow(fg) && !m_host.IsDesktopWindow(fg)) EndPeek();
            UpdateAutoHide(fg);
            return 0;
        }
        case WM_TIMER:
            switch (wp) {
                case kTimerSave:
                    KillTimer(m_msgWnd, kTimerSave);
                    DoSave();
                    return 0;
                case kTimerScan:
                    KillTimer(m_msgWnd, kTimerScan);
                    RequestDesktopScan();
                    return 0;
                case kTimerDisplay:
                    KillTimer(m_msgWnd, kTimerDisplay);
                    OnDisplayChanged();
                    return 0;
                case kTimerHold:
                    if (!(GetAsyncKeyState(static_cast<int>(m_holdVk)) & 0x8000)) EndPeek();
                    return 0;
                case kTimerHotCorner:
                    HotCornerTick();
                    return 0;
                case kTimerDaily:
                    DailyBackupTick();
                    return 0;
                case kTimerIcons:
                    KillTimer(m_msgWnd, kTimerIcons);
                    InvalidateAll();
                    return 0;
                case kTimerPageAnim:
                    PageAnimTick();
                    return 0;
                case kTimerPageIndicator:
                    KillTimer(m_msgWnd, kTimerPageIndicator);
                    for (auto& cv : m_canvases) cv->SetPageIndicator(false, 0);
                    return 0;
                case kTimerPrune:
                    KillTimer(m_msgWnd, kTimerPrune);
                    PruneMissingMembers();
                    return 0;
                case kTimerAutoHide: {
                    KillTimer(m_msgWnd, kTimerAutoHide);
                    POINT pt;
                    GetCursorPos(&pt);
                    HWND under = WindowFromPoint(pt);
                    if (!IsOurWindow(under)) UpdateAutoHide(GetForegroundWindow());
                    else SetTimer(m_msgWnd, kTimerAutoHide, 2500, nullptr);
                    return 0;
                }
            }
            if (wp >= kTimerPortalRetryBase && wp < kTimerPortalRetryBase + 0x100) {
                KillTimer(m_msgWnd, wp);
                UINT portalMsg = WM_APP_PORTAL_FIRST + static_cast<UINT>(wp - kTimerPortalRetryBase);
                for (auto& [id, pm] : m_portals)
                    if (pm->watchMsg == portalMsg) LoadPortal(*pm);
                return 0;
            }
            break;
        case WM_DISPLAYCHANGE:
            SetTimer(m_msgWnd, kTimerDisplay, 800, nullptr);
            return 0;
        case WM_SETTINGCHANGE:
            if (wp == SPI_SETWORKAREA || wp == SPI_SETDESKWALLPAPER || wp == SPI_SETICONMETRICS) SetTimer(m_msgWnd, kTimerDisplay, 800, nullptr);
            if (wp == SPI_SETDESKWALLPAPER) UpdateWallpaperColor();
            // Hidden-file settings and Desktop folder relocation.
            SetTimer(m_msgWnd, kTimerScan, 400, nullptr);
            return 0;
        case WM_POWERBROADCAST:
            if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) SetTimer(m_msgWnd, kTimerDisplay, 1500, nullptr);
            return TRUE;
        case WM_COPYDATA: {
            std::wstring verb, arg;
            if (ipc::DecodeCommand(lp, verb, arg)) {
                PostToUi([this, verb, arg] { ExecuteCommand(verb, arg); });
                return TRUE;
            }
            return FALSE;
        }
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wp) {
                DoSave();
                m_host.SetIconsHidden(false);
            }
            return 0;
        case WM_CLOSE:
            Exit();
            return 0;
    }
    return DefWindowProcW(m_msgWnd, msg, wp, lp);
}

}  // namespace of
