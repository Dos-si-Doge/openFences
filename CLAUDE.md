# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

openFences is a native C++20 / Win32 x64 reimplementation of Stardock Fences for Windows 10/11. `README.md` covers:
- user-facing behaviour, CLI verbs, registry policies and file locations
- the feature map against Fences versions
- how and why the design differs from Fences ("Background")

End-user documentation is the HTML guide in `docs/user-guide/`.

## Build and test

The toolchain is VS 2022 Build Tools (MSVC v143, Windows SDK, CMake, Ninja), .NET 8 SDK and WiX 5.0.2. Install it with
`scripts\install-toolchain.ps1`.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1                     # configure + build + ctest + MSI (x64-release)
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Preset x64-debug -SkipMsi
```

- **Running CMake directly** needs the x64 MSVC environment. Call `vcvars64.bat` first, from
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\`. Then use
  `cmake --preset x64-debug` and `cmake --build build\x64-debug`.
  - Pass `-DOPENFENCES_BUILD_APP=OFF` to build only `ofcore` and the tests.
- **A single unit test:** `build\x64-debug\ofcore_tests.exe --gtest_filter=Rules.*`, or
  `ctest --test-dir build\x64-debug -R Rules.PriorityFirstMatchWins`.
- **Warnings:** builds use `/W4 /permissive-` and are expected to stay warning-free in project code.
- **MSI:** built from `installer\Package.wxs`. `wix` lives in `%USERPROFILE%\.dotnet\tools`, which is not always on PATH.
  - Package sources come from the repo-local `nuget.config`, because the user's global NuGet config has none.
  - Validate with `wix msi validate out\openFences-1.0.0-x64.msi`; it is currently free of ICE errors and warnings.
- **Integration tests** (`tests\integration\*.ps1`) drive a real openFences on the live desktop through posted window
  messages, so the mouse is never moved. Pass `-Exe <path> -DataDir <empty folder>`.
- **User guide:** edit `docs\user-guide\guide.src.html`, then run `python scripts\build_user_guide.py`.
  - The script generates `openFences-User-Guide.html`, with SVG illustrations and images inlined.
- **App icon:** regenerate `res\openFences.ico` with `python scripts\make_icon.py`.

## Running the app safely during development

- **Single instance per session.** A mutex plus a hidden `openFences.Controller` window enforce it. Launching the exe while
  another copy runs only forwards a command (`--peek`, `--settings`, …) to it over `WM_COPYDATA`.
  - The user may have their own instance running. Check with `tasklist` before starting or `--shutdown`ing one, and never
    disturb theirs without asking.
- **Isolate test data.** Set `OPENFENCES_DATA_DIR` to a scratch folder so the user's real `%APPDATA%\openFences` config is
  never touched (`core/Paths.cpp`).
- **Side effects while running.** openFences hides Explorer's desktop `SysListView32` and draws every icon itself.
  - `--shutdown` exits cleanly and restores Explorer's icons.
  - If the process is killed, the watchdog child (`--watchdog <pid>`) restores them.
- **Seeing fences in screenshots.** Fences sit behind normal windows; use `--desktop-display` or `--peek`.

## Architecture

The layers are:
- `src/core`: no windows; unit tested.
- `src/shell`: Shell COM APIs.
- `src/desktop`: Explorer windows, monitors, input hooks.
- `src/render`: D3D11 / D2D / DirectComposition.
- `src/ui`: windows.
- `src/app`: the controller and process plumbing.

**Explorer desktop internals (why `DesktopHost` looks the way it does)**

```
Progman                    top-level desktop window (explorer.exe)
 └─ SHELLDLL_DefView       shell view of the merged Desktop folder (%USERPROFILE%\Desktop + %PUBLIC%\Desktop)
     └─ SysListView32      the ListView that draws Explorer's desktop icons
WorkerW (0..n)             siblings; one paints the wallpaper. With slideshows, DefView may live under a WorkerW
```

- Explorer stores icon positions under `HKCU\Software\Microsoft\Windows\Shell\Bags\1\Desktop`. openFences deliberately
  never moves Explorer's icons (no `LVM_SETITEMPOSITION`, no code injection). It only shows or hides the ListView.
  - Fences does inject and move icons, and its long-running bugs come from that: Explorer crashes, icons scrambling after
    sleep or DPI changes, conflicts with DisplayFusion or nView. Don't reintroduce either technique.
- Every Windows feature update can break desktop internals; Fences needed fixes for the Creators Update, 24H2 and the
  Windows 11 context menu. Keep undocumented behaviour, such as the `0x052C` WorkerW trick or window-class lookups,
  confined to `DesktopHost`.
- The riskiest areas are multi-monitor, docking, sleep and resume, and DPI changes. Check them when touching layout or
  window code (`OnDisplayChanged`, `WM_DPICHANGED`, `PBT_APMRESUMEAUTOMATIC`, `TaskbarCreated`).

**Model (`core/Model.h`)**
- `Config` holds global settings, `rules`, and `layouts` keyed by a monitor-configuration hash
  (`core/MonitorConfig.cpp`, or `"default"` when per-monitor layouts are off).
- A `Layout` holds `pages` of `Group`s, plus `persistent` groups shown on every page and `loose` grid positions for
  unfenced icons.
- A `Group` (a fence) has `Tab`s. A tab is either `Items`, which lists desktop item keys, or `Portal`, which shows a
  folder path.
- Group rects are fractions of their monitor's work area.
- Desktop items are identified by their **in-folder parsing name** ("Report.docx", or "::{GUID}" for virtual items), not
  by absolute path.
- `core/Config.cpp` is the only JSON (nlohmann) serializer. Unknown fields are ignored, values are clamped, and
  `ParseConfig` handles version migration.

**Controller (`app/Controller.*`, `app/Commands.cpp`)**
- A singleton that owns the config, the desktop item model (`m_items`), all windows, portals, the tray, hotkeys and
  timers.
- Windows never cache `Group*`: config vectors get mutated. They hold a group id and re-look-up through
  `FindGroup(CurrentLayout(), id)`.
- Mutations follow a fixed pattern: change `m_config`, then call `RefreshAll()` (reconciles fence windows with the
  visible groups) or `SyncItems()` (re-pushes items), then `SaveSoon()` (debounced atomic write).
- `Commands.cpp` holds the menus, Peek, Quick-Hide, pages, snapshots, templates and the `ExecuteCommand` verb dispatcher
  used by the CLI.

**Desktop items**
- A desktop scan runs on a worker thread (`app/Worker`, a COM STA) and is triggered by debounced
  `SHChangeNotifyRegister` events. Results come back through `PostToUi`.
- `ApplyDesktopScan` diffs the new scan against the old one:
  - new items go through pending-drop matching, then through the rules (`RulesTargetsApply`)
  - renames are carried over through `m_renames` / `NoteRename`
- Membership keys of missing items are kept (Office delete-and-recreate saves) and pruned later.
- Portals (`PortalModel`) enumerate on their own worker and retry with back-off when offline.

**Windows (`ui/`)**
- All windows derive from `WindowBase`: a `WS_EX_NOREDIRECTIONBITMAP` popup drawn through a DirectComposition swap chain
  (`render/Surface`).
- `WM_WINDOWPOSCHANGING` strips z-order changes unless `AllowZOrder(true)` is set. That is how fences stay on the
  desktop layer.
- `DesktopHost` (`desktop/`) holds all Explorer-specific and undocumented behaviour:
  - it makes our windows owned by `Progman`
  - it hides and shows the desktop icon view
  - it handles Peek raising (topmost)
  - it handles the "child of desktop" fallback mode
- `DesktopCanvas` is one per monitor, sized to the work area. It draws loose icons and handles empty-desktop gestures:
  drag-to-create, double-click Quick-Hide, desktop menu, edge swipe for pages, and the Peek/Desktop Display backdrop.
- `FenceWindow` is one per visible group. It handles the title bar and tabs, move/resize with `core/Geometry` snapping,
  merge and detach of tabs, roll-up, and drop handling.
- `ItemView` is the shared layout, paint and hit-test component for both canvases and fences.
- Shell integration is native:
  - context menus come from `IContextMenu`; every wndproc first calls `HandleShellMenuMessage`
  - drops are forwarded to the target folder's own `IDropTarget` (`shell/DragDrop.cpp`)
  - in-process drags are recognised through `CurrentDrag()`, so moving an item between fences is just a membership change
- Input handlers derive screen points from message coordinates (`ClientToScreen`), not from `GetCursorPos`. The
  integration tests rely on this by posting synthetic mouse messages.
- Settings (`ui/settings`) are built in code with the `Form` builder, not dialog resources. Each page has a
  `Build*`/`On*` pair, and changes call `Controller::SettingsChanged()` immediately.

**Process-level pieces (`app/`)**
- `main.cpp`: CLI parsing and IPC forwarding.
- `Watchdog`: restores Explorer's icons. It restarts the app only after crash exit codes (`0xC...`), at most 3 times in
  5 minutes.
- `CrashHandler`: minidumps.
- `core/Autostart`: the HKCU Run key, including the Task Manager `StartupApproved` mark.
- `core/Policy` and `core/Templates`: admin policy under `HKLM\SOFTWARE\Policies\openFences`. A `ConfigPath` template is
  applied once per layout per session.

## Gotchas

- **Rules vs. content lock:** rules may fill content-locked fences (the Kiosk template depends on this). Only manual
  edits honour `IsContentLocked`.
- **Shipped templates:** they live in `res/templates/*.json`, are copied next to the exe after build, and are installed by
  the MSI. `Templates.ShippedTemplatesParse` validates them.
- **Bash heredocs collapse doubled backslashes** in this environment. Write files containing Windows paths, JSON escapes or
  regexes with the Write/Edit tools, not `cat <<EOF` or inline Python.
- **PowerShell test scripts:**
  - `$null` passed to a P/Invoke string parameter becomes `""`; use `[NullString]::Value`.
  - Avoid helper names that are built-in aliases (`LP`, `Kill`) and the automatic variable `$pid`.
  - Wrap single results in `@()` before `.Count`.
