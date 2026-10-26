# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

openFences is a native C++20 / Win32 x64 reimplementation of Stardock Fences for Windows 10/11. `README.md` covers:
- user-facing behaviour, CLI verbs, registry policies and file locations
- the feature map against Fences versions
- how and why the design differs from Fences ("Background")

openFences is created by Dos-si-Doge under the MIT License (`LICENSE`). New `.cpp`/`.h`/`.ps1`/`.py` files start with
the same two-line header as the others (`Copyright (c) 2026 Dos-si-Doge` / `SPDX-License-Identifier: MIT`). The credits
the app shows come from `src/Version.h.in` (`OF_AUTHOR`, `OF_ISSUES_URL`, …); the MSI's Manufacturer and `ARPHELPLINK`
repeat them in `installer/Package.wxs`.

End-user documentation exists twice and both copies must be kept current:
- the built-in **Welcome to openFences** window (content in `src/core/Welcome.cpp`)
- the external HTML guide (`docs/user-guide/`)

## Build and test

The toolchain is VS 2022 Build Tools (MSVC v143, Windows SDK, CMake, Ninja), .NET 8 SDK, WiX 5.0.2, Git and Python 3.
Install it with `scripts\install-toolchain.ps1`.
- It works on a clean Windows 11 machine and on its own, without the repo (`-CloneTo <dir> -Build` clones and builds).
- It's idempotent; `-DryRun` lists what's missing without elevating.
- Machine-wide winget installs run in an elevated child (`-MachinePhase`). WiX, the VS Code extensions and the clone stay
  unelevated, so they belong to the user.
- When a tool or version changes, update it there as well as in `build.ps1`.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1                     # user guide + build + ctest + MSI + portable zip (x64-release)
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Preset x64-debug -SkipPackages
```

- **Running CMake directly** needs the x64 MSVC environment. Call `vcvars64.bat` first, from
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\`. Then use
  `cmake --preset x64-debug` and `cmake --build build\x64-debug`.
  - Pass `-DOPENFENCES_BUILD_APP=OFF` to build only `ofcore` and the tests.
- **A single unit test:** `build\x64-debug\ofcore_tests.exe --gtest_filter=Rules.*`, or
  `ctest --test-dir build\x64-debug -R Rules.PriorityFirstMatchWins`.
- **Warnings:** builds use `/W4 /permissive-` and are expected to stay warning-free in project code.
- **Static analysis:** all clean as of 1.0.3; keep it that way.
  - C++: configure a separate folder with `/analyze` (needs the vcvars environment; filter the output to `src\` and
    `tests\`, since `_deps` is third-party):
    `cmake -S . -B build\analyze -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_RC_COMPILER=rc.exe "-DCMAKE_CXX_FLAGS=/analyze /analyze:external-"`,
    then `cmake --build build\analyze --clean-first`.
  - PowerShell: `Invoke-ScriptAnalyzer -Severity Error,Warning` (module `PSScriptAnalyzer`, CurrentUser). `Write-Host` in
    console scripts is intentional; `ShotDir` "unused" is a false positive (used by helper functions).
  - Python: `python -m pyflakes` (pip `--user`).
- **Security rules:**
  - Don't relax UIPI for `WM_COPYDATA` except when elevated (`ProcessIsElevated` in `Controller.cpp`).
  - Validate everything in `ipc::DecodeCommand`.
  - Anything that applies a template from a user-chosen file must show `MoveRuleDestinations` before running its rules.
  - Keep `/guard:cf` and `/DEPENDENTLOADFLAG:0x800` (`CMakeLists.txt`). Never `LoadLibrary` by bare name.
- **A `build\` folder copied from another machine doesn't configure** ("CMake Error at CMakeLists.txt:3 (project)"). The
  CMake cache stores 8.3 short paths to Ninja (`C:\PROGRA~2\MICROS~3\...`) that differ between machines. Delete
  `build\<preset>` (generated, gitignored) and build again.
- **MSI:** built from `installer\Package.wxs`. `wix` lives in `%USERPROFILE%\.dotnet\tools`, which is not always on PATH.
  - Package sources come from the repo-local `nuget.config`, because the user's global NuGet config has none.
  - Validate with `wix msi validate out\openFences-<ver>-x64.msi`; it is currently free of ICE errors and warnings.
- **Integration tests** (`tests\integration\*.ps1`) drive a real openFences on the live desktop through posted window
  messages, so the mouse is never moved. Pass `-Exe <path> -DataDir <empty folder>`.
  - Stop the user's copy first. If one is running, the test's commands (`--snapshot`, `--shutdown`, …) are forwarded to
    it and act on their real data. `SettingsTests` and `AutostartTests` refuse to start while openFences is running.
  - The user's copy may be the installed one (`C:\Program Files\openFences\openFences.exe`, started with no arguments
    or `--autostart`) rather than `build\x64-release`. Check `Win32_Process` for the path, stop it with its own
    `--shutdown`, and restart that same exe afterwards.
  - Tests that touch real per-user state must restore it. `AutostartTests` sets aside an existing `HKCU\...\Run\openFences`
    entry and its Task Manager mark, then puts them back.
  - Capture screenshots with `PrintWindow` (the test's own window only), not `CopyFromScreen`. The user may be working,
    and a screen copy picks up whatever is in front.
  - Synthetic drags only work while openFences' mouse capture is inert. Two things make it live, and Windows then feeds
    real-pointer moves into the drag:
    - an explorer.exe window (File Explorer, Control Panel) is in the foreground, because our Progman-owned windows share
      its input queue;
    - the real pointer rests over an openFences window (a fence or the empty desktop). VMs send pointer events even when
      the mouse is idle or outside the VM window.

    `InteractionTests` (`DragsUnreliableReason`) then doesn't drag at all, so fences can't merge at random, and reports
    the drag checks as SKIP with the reason. To get real results, the user parks the pointer over another window.
  - Launch with `--no-welcome` in suites that don't test the Welcome window: it would make openFences the foreground app.
  - Results depend on the machine. The `ShellTests` desktop-menu check only exercises the hidden installer submenu when
    the MSI has registered `HKCR\DesktopBackground\Shell\openFences`; its detail column says whether it is. Peek's
    registered shortcut depends on which hotkeys other software already holds (the log says which one won).
- **User guide:** edit `docs\user-guide\guide.src.html`, then run `python scripts\build_user_guide.py`.
  - Prefer drawn illustrations (`ill_*` in the script) to screenshots. Screenshots must show only openFences' own windows:
    no shell-extension menus, real file names, user paths or other personal details from the PC they were taken on.
  - The script generates `openFences-User-Guide.html`, with SVG illustrations and images inlined.
- **App icon:** regenerate `res\openFences.ico` with `python scripts\make_icon.py`.
- **Welcome window hero images:** regenerate `res\welcome\*.jpg` with `scripts\render_welcome_images.ps1`. It needs
  Python and Edge.
  - The images are the guide's SVG illustrations, rasterized through `build_user_guide.py --export-pages`, plus guide
    screenshots.
  - Rerun it whenever an illustration used as a hero changes, and list new heroes both in the script and in
    `res/openFences.rc`.

## Release routine (do this after every change, without being asked)

The user wants every change to end with fresh release artifacts and docs:

1. **Bump the version** in `project(openFences VERSION x.y.z)` in `CMakeLists.txt`. Use a patch bump for fixes and a
   minor bump for features. It feeds the exe, the About page and the MSI.
   - Bump only once per release. While the current version isn't released yet (no `vX.Y.Z` tag), keep it and add to its
     `CHANGELOG.md` entry. The user decides when a version is released.
2. **Add a `CHANGELOG.md` entry** with Fixed / Added / Changed sections.
3. **Update the docs that are affected:**
   - `README.md`
   - `CLAUDE.md`
   - **both** user docs: the HTML guide source `docs\user-guide\guide.src.html` (`build.ps1` regenerates the guide from it)
     and the built-in Welcome content in `src\core\Welcome.cpp`
   - refresh the "What's new" topic in `Welcome.cpp` for every release
   - `WelcomeTests` fail if a topic points at a missing guide anchor or hero image, or if the version is missing from
     "What's new" or `CHANGELOG.md`
4. **Run `scripts\build.ps1`.** It produces `out\openFences-<ver>-x64.msi` and `out\openFences-<ver>-x64-portable.zip`.
   - The release link fails if `build\x64-release\openFences.exe` is running (the user often runs that build). The user
     allows stopping it with `--shutdown` without asking; always restart it afterwards with `OPENFENCES_DATA_DIR` unset.
5. **Validate the MSI** with `wix msi validate`.

Commit only when asked.

**Never change or commit on `main`.** Work on a feature or version branch (for example `1.0.3`), and merge into `main`
only when the user asks. If the current branch is `main`, warn the user and switch to a branch before editing.
GitHub branch protection isn't available for this private repo, so this is enforced by convention.

## Git, GitHub and releases

- **Remote:** `origin` is the private repo `https://github.com/Dos-si-Doge/openFences`, owned by the user (GitHub
  `Dos-si-Doge`). `gh` must be signed in (`gh auth login`) for pushes, pull requests and releases.
- **Branches:** each version is developed on a branch named after it (`1.0.2`, `1.0.3`, …), created from an up-to-date
  `main`. It reaches `main` through a GitHub pull request; the user merged 1.0.2 as PR #1. Don't merge, push or open pull
  requests unless asked.
- **The user often stages changes themselves.** Commit what's staged plus what they asked for, and check `git status`
  first.
- **Commit messages** follow the earlier release commits (`git log`): a title `openFences x.y.z: <summary>`, then
  Fixed / Added / Changed paragraphs, a short Docs line, and the attribution trailer.
- **Releasing** (only when asked):
  1. Commit, then tag `git tag -a vX.Y.Z -m "openFences X.Y.Z"`.
  2. Push the branch (`git push -u origin <branch>`) and the tag.
  3. Run `gh release create vX.Y.Z --verify-tag --title "<commit title>" --notes-file <notes>`, attaching
     `out\openFences-X.Y.Z-x64.msi` and `out\openFences-X.Y.Z-x64-portable.zip`.
  4. Build the notes from the version's `CHANGELOG.md` entry, with a short **Downloads** section (MSI vs portable zip)
     at the top and the Issues link at the end.
  5. Check the assets with `gh release view`.
- **Never publish personal data.** Screenshots in docs and releases come from test data folders and show only
  openFences' own windows, with no names, paths, installed software or files from the PC they were taken on.

## Running the app safely during development

- **Single instance per session.** A mutex plus a hidden `openFences.Controller` window enforce it. Launching the exe while
  another copy runs only forwards a command (`--peek`, `--settings`, …) to it over `WM_COPYDATA`.
  - The user may have their own instance running. Check with `tasklist` before starting or `--shutdown`ing one. Stopping
    theirs for a build or test is allowed, but always restart it afterwards.
- **Isolate test data.** Set `OPENFENCES_DATA_DIR` to a scratch folder so the user's real `%APPDATA%\openFences` config is
  never touched (`core/Paths.cpp`).
  - The variable takes precedence over portable mode: an `openFences.portable` file next to the exe sends data to
    `<exe dir>\Data`.
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
  - The page is rebuilt on every resize. Content width is capped at 860 DIP. `Form` measures wrapped text, so labels,
    checkboxes and texts grow taller instead of clipping; keep new row types doing the same.
  - Pictures (owner-drawn areas) size themselves from the width with `OwnerDrawAspect` and fit their content whole, like
    the Welcome heroes (`FitHero`). `SettingsTests.ps1` checks this on the Layout page.

**Process-level pieces (`app/`)**
- `main.cpp`: CLI parsing and IPC forwarding.
- `Watchdog`: restores Explorer's icons. It restarts the app only after crash exit codes (`0xC...`), at most 3 times in
  5 minutes.
- `CrashHandler`: minidumps.
- `core/Autostart`: the HKCU Run key, including the Task Manager `StartupApproved` mark.
- `core/Policy` and `core/Templates`: admin policy under `HKLM\SOFTWARE\Policies\openFences`. A `ConfigPath` template is
  applied once per layout per session.

## Gotchas

- **Registry cascades in hosted shell menus:** submenus defined in the registry (`SubCommands=""` + `shell` subkeys, or
  `ExtendedSubCommandsKey`) come back empty from `IContextMenu` outside Explorer: after `WM_INITMENUPOPUP` they have 0
  items, while handler-based submenus like *New* fill normally. `ShowDesktopMenu` therefore passes the installer's
  `openFences` verb to `TrackShellMenu`'s `hiddenVerbs`. `ShellTests` checks the real desktop menu.
  - Windows appends `Position=Bottom` verbs (Display settings, Personalize, …) to the very end of the menu, below anything
    already there. Items that must stay last go in `TrackShellMenu`'s `appendLast` callback.
- **Rules vs. content lock:** rules may fill content-locked fences (the Kiosk template depends on this). Only manual
  edits honour `IsContentLocked`.
- **Quick-Hide hit-testing:** canvas input must go through `DesktopCanvas::VisibleItemAt` (`LooseItemVisible` in core).
  Icons excluded from Quick-Hide stay clickable, and a double-click toggles Quick-Hide only on genuinely empty desktop.
- **Welcome window (`ui/welcome/WelcomeWindow`):** a Win32 counterpart of PowerToys' `OobeWindow`/`OOBEPageControl`.
  - Topics come from `core/Welcome.cpp`.
  - Hero images are RCDATA resources named `WELCOME_<HERO>`, decoded and scaled with WIC.
  - They are never cropped or enlarged: `FitHero` (core, unit tested) sizes the band to the whole image, at most native
    size. In a wider band the image is centered on a neutral fill, which also frames screenshots (`heroFit`).
  - "Learn more" goes through `ui/Help.cpp`, which hands `file:///...#anchor` to the default browser, because the
    `.html` association drops fragments.
  - `general.lastSeenVersion` drives the automatic "What's new" after an update.
- **Assets next to the exe:** templates, `openFences-User-Guide.html` and `LICENSE.txt` are copied there by the always-run
  `openFencesAssets` target. The MSI and the zip package them from `build\<preset>`.
- **Shipped templates:** they live in `res/templates/*.json`, are copied next to the exe after build, and are installed by
  the MSI. `Templates.ShippedTemplatesParse` validates them.
- **Bash heredocs collapse doubled backslashes** in this environment. Write files containing Windows paths, JSON escapes or
  regexes with the Write/Edit tools, not `cat <<EOF` or inline Python.
- **PowerShell test scripts:**
  - `$null` passed to a P/Invoke string parameter becomes `""`; use `[NullString]::Value`.
  - Avoid helper names that are built-in aliases (`LP`, `Kill`) and the automatic variable `$pid`.
  - Wrap single results in `@()` before `.Count`.
  - Keep `.ps1` files ASCII, or save them as UTF-8 with a BOM. Windows PowerShell reads BOM-less files as ANSI, so a
    literal like `…` never matches; use `-like` wildcards instead.
  - Don't pipe or capture `winget` (or any console-drawing tool). PowerShell decodes its output with the OEM code page,
    which garbles the progress bar and puts each spinner frame on a new line. Use `Start-Process -NoNewWindow -PassThru`,
    touch `$p.Handle`, then `WaitForExit()`/`ExitCode`.
  - `Start-Process -Verb RunAs -Wait` can return while the elevated process is still running. Poll for a result file
    and for the process ID instead (`install-toolchain.ps1`).
