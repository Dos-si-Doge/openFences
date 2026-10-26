# openFences

A native, open-source desktop organizer for **Windows 10 and Windows 11 (x64)**, modeled on Stardock Fences
(see [Background](#background-stardock-fences)). It groups desktop icons into translucent, labeled **fences**,
and adds Folder Portals, rules, Quick-Hide, roll-up, Peek, tabs, pages, snapshots and business deployment.
openFences is an independent project and isn't affiliated with Stardock.

- C++20, Win32, Direct2D / DirectComposition / DirectWrite, Windows Shell COM APIs
- One executable, no runtime dependencies (static CRT), Per-Monitor-V2 DPI aware
- MSI installer (WiX v5) with an optional **start at sign-in** setting

**User guide:** [`docs/user-guide/openFences-User-Guide.html`](docs/user-guide/openFences-User-Guide.html) is a single self-contained page. Edit `docs/user-guide/guide.src.html` and rebuild it with `python scripts/build_user_guide.py`.

## Features

The middle column shows which Fences version introduced each feature.

| Fences feature | In Fences since | openFences |
|---|---|---|
| Fences: create by dragging on the desktop, move, resize, rename, delete, scroll | 1.0 (2009) | ✓ drag a rectangle on empty desktop → *Create fence here*; title menu (≡) |
| Shell-native items | 1.0 | ✓ real Explorer context menu, open, rename (F2), delete, cut/copy/paste, drag in and out |
| Per-fence icon size, sort (custom/name/type/date) | 1.0 / 2.x | ✓ title menu → View |
| Personalization: color, transparency, title bar, corners, wallpaper-aware color | 1.0; corners 2.12; wallpaper 5.86 | ✓ Settings → Personalization; per-fence color from the title menu |
| Icon Tint, global or per fence | 6.0 (2025) | ✓ Settings → Personalization; title menu → Icon tint |
| Chameleon (fade icons into the wallpaper, highlight on hover) | 5.0 (2023) | ✓ Settings → Chameleon |
| Rules: file type, name (glob / regex), shortcut target, dates, any/all, priority | Pro / 2.x; target rules 3.10 | ✓ Settings → Rules; actions: put in fence, or move file to a folder |
| Folder Portals: live folder view, network/cloud folders, navigation, list views, drop to move/copy | 2.x; navigation 3.0 | ✓ offline portals retry with back-off |
| Tabs: merge by dropping a title on another fence, drag a tab out, 4 tab styles, tab colors | 6.0 | ✓ |
| Roll-up: double-click title, auto roll-up at screen edges | 3.0 (2015) | ✓ |
| Roll-up opens on hover, after a hover delay, or on click | 6.20 (2025) | ✓ |
| Quick-Hide: double-click desktop, exclusions, auto-hide, state persists | 1.0 | ✓ |
| Peek: hotkey (toggle or hold) | 4.0 (2021) | ✓ default Win+Space, or Ctrl+Alt+Space when Windows owns Win+Space |
| Peek: tray icon, hot corner | 5.0 | ✓ hot corner aware of the taskbar edge, including left/right taskbars (Fences 6.54) |
| Desktop Display (instant "show desktop", optional Win+D) | 4.0 | ✓ draws the wallpaper above windows; never minimizes anything |
| Pages: swipe from the screen edge | 2.01 | ✓ |
| Pages: fences shown on every page | 2.1 | ✓ |
| Snapshots: manual, preview before restore | 1.0 | ✓ Settings → Layout |
| Snapshots: daily backups (7 kept), first-run snapshot | 5.x | ✓ Settings → Layout |
| Per-monitor-configuration layouts | 5.5 (2024) | ✓ layouts keyed by a hash of monitor ids, resolutions and positions |
| High-DPI support | 3.0; 400% in 6.46 | ✓ Per-Monitor-V2 |
| Follow a moved or redirected Desktop folder | 6.20 | ✓ |
| Lock fences / lock contents | 5.0 | ✓ global and per fence |
| Business: templates, export/import, config pushed from a network path, locks, per-user disable | 5.0; templates 6.0 | ✓ see [Deployment](#deployment) |
| Desktop right-click menu entries | 1.0; Win11 modern menu 5.56 | ✓ in the classic desktop menu (installed by the MSI); see [Known limitations](#known-limitations) |

## Build

Prerequisites, all installed by [`scripts/install-toolchain.ps1`](scripts/install-toolchain.ps1) through winget:

- Visual Studio 2022 Build Tools with the C++ workload (MSVC v143, Windows 11 SDK, CMake, Ninja)
- .NET 8 SDK and the WiX Toolset 5.0.2 global tool, with the UI and Util extensions. These are needed only for the MSI.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\install-toolchain.ps1   # once
powershell -ExecutionPolicy Bypass -File scripts\build.ps1               # configure, build, unit tests, MSI
```

The script regenerates the user guide, then builds. The output is:
- `build\x64-release\openFences.exe`
- the installer, `out\openFences-<version>-x64.msi`
- the portable zip, `out\openFences-<version>-x64-portable.zip`

Useful options: `-Preset x64-debug`, `-SkipTests`, `-SkipPackages`. Release notes are in [CHANGELOG.md](CHANGELOG.md).
CMake presets (`x64-debug`, `x64-release`) also work from an "x64 Native Tools" prompt, in VS Code, or in Visual Studio.

`scripts/make_icon.py` regenerates `res/openFences.ico`.

## Install

Double-click the MSI. The wizard asks for an install folder and whether openFences should **start automatically when you sign in**, and can launch openFences when it finishes.

Silent install:

```powershell
msiexec /i openFences-1.0.1-x64.msi /qn AUTOSTART=1   # or AUTOSTART=0
```

What the MSI installs:

- `C:\Program Files\openFences\openFences.exe`
- `templates\*.json` and `openFences-User-Guide.html`
- a Start menu shortcut
- the desktop right-click submenu `HKLM\SOFTWARE\Classes\DesktopBackground\Shell\openFences`
- `HKLM\SOFTWARE\openFences\DefaultAutostart`

Uninstalling closes openFences and removes the current user's startup entry. openFences never moves Explorer's
icon positions, so the original desktop comes back as soon as openFences exits.

The MSI is not code-signed, so SmartScreen warns on first run until it is signed.

### Portable version

Unzip `openFences-<version>-x64-portable.zip` anywhere, for example onto a USB stick, and run `openFences.exe`. No
installation or administrator rights are needed.

- The zip contains an `openFences.portable` marker file. While it's next to the exe, settings, backups and logs live in a
  `Data` folder beside it instead of your user profile. Delete the marker to use `%APPDATA%` like the installed version.
- The portable version has no Start menu shortcut and no desktop right-click submenu. openFences' own desktop menu still
  works.
- "Start at sign-in" points at the portable copy. Turn it off before moving or deleting the folder.

### Start at sign-in

Run at sign-in is a per-user entry, `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\openFences`.

- The installer's checkbox sets the machine default. Each user gets it on their first run.
- After that, users toggle it in **Settings → Welcome** or from the tray menu.
- Disabling openFences in **Task Manager → Startup** is detected. Re-enabling it in openFences clears that mark.

## Using openFences

| Action | How |
|---|---|
| Create a fence or Folder Portal | Drag a rectangle on empty desktop, or right-click the desktop → *New fence here* |
| Add icons | Drag icons into a fence, or onto a tab header |
| Roll up / down | Double-click the fence title bar |
| Combine fences into tabs | Drag a fence by its title onto another fence's title |
| Separate a tab | Drag the tab down out of the title bar |
| Hide everything | Double-click empty desktop (again to show) |
| Peek | Win+Space (or Ctrl+Alt+Space), the tray icon, or the hot corner. Esc or a click on empty space ends it |
| Switch pages | Press and hold at the far left or right screen edge and pull, or right-click the desktop → *Pages* |
| Fence options | ≡ button in the title bar, or right-click empty space in the fence |
| Settings | Tray icon → *Configure openFences*, right-click the desktop, or `openFences.exe --settings` |

### Command line

```
openFences.exe [--settings [page]] [--toggle-hide | --hide | --show] [--toggle-lock] [--peek] [--desktop-display]
               [--next-page | --prev-page | --page N] [--new-fence] [--apply-rules] [--snapshot [name]]
               [--restore file] [--export file] [--import file] [--apply-template file] [--automatic-layout]
               [--refresh] [--shutdown] [--verbose]
```

Commands go to the running instance. Settings pages: `welcome chameleon personalization portals peek hide rollup tabs pages rules layout business about`.

### Files

| Path | Contents |
|---|---|
| `%APPDATA%\openFences\config.json` | Settings, rules and layouts (one per monitor arrangement). Written atomically |
| `%APPDATA%\openFences\Backups\` | Snapshots (`snap-*.json`) and daily backups (`auto-YYYYMMDD.json`, 7 kept) |
| `%LOCALAPPDATA%\openFences\logs\` | `openFences.log` (`--verbose` for details) |
| `%LOCALAPPDATA%\openFences\Crashes\` | Minidumps |

In portable mode all of these live under `<exe folder>\Data` (logs and crash dumps in `Data\local`). Set
`OPENFENCES_DATA_DIR` to keep them somewhere else, for example in tests; it takes precedence over portable mode.

## Deployment

| Registry value | Effect |
|---|---|
| `HKLM\SOFTWARE\Policies\openFences\ConfigPath` (REG_SZ) | Template or exported layout applied at every sign-in, for example `\\server\share\fences.json` |
| `HKLM\SOFTWARE\Policies\openFences\LockLayout` (DWORD) | 1 = fences locked, 0 = unlocked (users can't change it) |
| `HKLM\SOFTWARE\Policies\openFences\LockContent` (DWORD) | 1 = fence contents locked |
| `HKLM\SOFTWARE\Policies\openFences\DisableSettings` (DWORD) | 1 = hide the settings window |
| `HKLM\SOFTWARE\Policies\openFences\Disabled` (DWORD) | 1 = openFences does not run |
| `HKCU\Software\openFences\Disabled` (DWORD) | 1 = openFences does not run for this user |

Templates are config documents with a `"template"` header; see [`res/templates/`](res/templates).
The shipped templates are *Default*, *Work* (with a Downloads portal and an archive for old files) and *Kiosk* (one locked
fence; other files are moved off the desktop). **Settings → Business → Export layout** produces the same format.

## Architecture

openFences never injects code into explorer.exe. While it runs, it hides Explorer's desktop icon view
(`SysListView32`) and draws every icon itself:

- A **desktop canvas** per monitor is a transparent window that holds loose icons. It also handles empty-desktop
  gestures: drag-to-create, double-click hide, the desktop menu, page swipes, and the Peek backdrop.
- A **fence window** per fence.

Both are DirectComposition surfaces, owned by `Progman`, so they stay on the desktop layer and survive *Show desktop*.

```
src/core     model, JSON config + migration, rules, snapping/grid math, monitor hashing, snapshots,
             autostart, policy, templates                        (pure; unit tested)
src/shell    shell items, enumeration, icon/thumbnail cache, context menus, drag & drop, change
             notifications, file operations, wallpaper
src/desktop  Explorer desktop windows (Progman / DefView / SysListView32), monitors, hotkeys and hooks
src/render   Direct3D 11 / Direct2D / DirectComposition device, per-window surfaces, drawing helpers
src/ui       fence window, desktop canvas, item view, inline rename, settings window
src/app      controller, menus and commands, tray, single instance + IPC, watchdog, crash handler
```

How the main pieces work:

- **Items:** keyed by their in-folder parsing name, so fence membership survives restoring a backup on another PC.
  Membership also survives Office's delete-and-recreate save pattern.
- **Change notifications:** `SHChangeNotifyRegister`, debounced, followed by a rescan and a diff.
- **Threads:** desktop scans, portal enumeration and icon extraction each run on their own COM STA worker.
- **Watchdog:** a small watchdog process (`--watchdog <pid>`) restores Explorer's icons if openFences is killed. After a
  crash it restarts openFences, at most 3 times in 5 minutes.
- **Explorer restarts:** openFences listens for `TaskbarCreated` and re-attaches to the desktop.

## Tests

- **Unit tests** (GoogleTest, 50 tests): run by `scripts\build.ps1` or `ctest --preset x64-release`. They cover rules,
  config round-trip and migration, geometry and snapping, monitor hashing, snapshots, autostart parsing, hotkeys and the shipped templates.
- **Integration tests** in [`tests/integration/`](tests/integration) run against a real desktop session. They drive
  openFences with posted window messages, so the mouse is never moved:
  - `InteractionTests.ps1`: roll-up, move, resize, merge into tabs, detach a tab, Quick-Hide, CLI commands, templates,
    snapshots, and restoring Explorer's icons on exit
  - `AutostartTests.ps1`: run at sign-in, including the Task Manager "disabled" mark
  - `WatchdogTests.ps1`: icon restore after a kill or a crash
  - `QuickHideTests.ps1`: icons kept visible during Quick-Hide open on double-click without turning Quick-Hide off
  - `ShellTests.ps1`: Explorer restart, and the shell context menu inside a fence

  ```powershell
  powershell -ExecutionPolicy Bypass -File tests\integration\InteractionTests.ps1 -Exe build\x64-release\openFences.exe -DataDir $env:TEMP\of-test
  ```

## Known limitations

- On Windows 11 the desktop menu entries appear under *Show more options*. The modern menu needs a packaged
  `IExplorerCommand` extension.
- English UI only. Strings are kept in code, ready to move to a string table. Fences ships 11 languages.
- x64 only. Fences also runs on Windows on ARM.
- Uninstall removes the startup entry only for the user who uninstalls. Other users' entries point at the removed
  exe and do nothing.
- *Child of the desktop window* integration mode (Settings → Welcome) is offered as a fallback. The default,
  *owned by the desktop*, is the tested path.
- No built-in update checks or crash-report upload. Crashes produce local minidumps only.
- Fences settings (`HKCU\Software\Stardock\Fences`, `%APPDATA%\Stardock\Fences`) can't be imported.

## Background: Stardock Fences

[Fences](https://www.stardock.com/products/fences/) is a proprietary desktop organizer from Stardock, first released as
freeware in 2009. Each major version added one or two headline features:

| Version | Released | Headline features |
|---|---|---|
| 1.0 | 2009 | Fences, Quick-Hide, snapshots, color schemes |
| 2 | ~2011–2013 | Pages, Folder Portals, touch |
| 3 | ~2015 | Windows 10, roll-up, high-DPI, rules by shortcut target |
| 4 | Dec 2021 | Peek, Desktop Display |
| 5 | Nov 2023 | Chameleon, Peek tray icon and hot corner, business deployment, per-monitor-configuration layouts |
| 6 | May 2025 | Tabs, Icon Tint, templated business deployments |

The current release is 6.58 (October 2026). Its point releases mostly chase compatibility with Windows feature updates
(Creators Update, 24H2), multi-monitor setups, DPI, and sleep and resume. Those are the hardest areas for any desktop
overlay tool, and openFences' design aims at them.

### How openFences differs

Stardock doesn't publish how Fences works. Its observable behaviour points to tight Explorer integration:

- A shell extension, `FencesMenu64.dll`, is loaded into explorer.exe.
- Explorer's real desktop icon positions get moved, which is why Fences takes a pre-install snapshot to restore the
  desktop on uninstall.
- Its known bug classes follow from that design: Explorer crashes, icons scrambling after sleep or DPI changes,
  conflicts with DisplayFusion or nView, and Office files leaving their fence after a save.

openFences avoids that whole class of problems:

- **No code inside explorer.exe.** All desktop handling is isolated in `src/desktop/DesktopHost`.
- **It draws every icon itself.** Explorer's icon view is hidden while openFences runs and is never rearranged, so the
  original desktop returns as soon as openFences exits, without a restore step.
- **Membership is keyed by item name.** It survives Office's delete-and-recreate saves and backups restored on
  another PC.
- **Desktop Display paints the wallpaper above windows** instead of minimizing everything.
- **Folder Portals retry when offline.** Fences shows an empty portal when a network share isn't reachable at sign-in.

### Prior art and alternatives

- **Free and open source:** Desktop Fences+ (Windows), Tokri (cross-platform), and KDE's Folder View plasmoid (2008),
  which ZDNet readers compared with Fences in 2009.
- **Other Windows tools:** Portals, Nimi Places, Sideslide, iTop Easy Desktop, Coodesker, DeskFrame, PecoFence and ViPad.
- **macOS equivalents:** Floxtop, iCollections and DeskSweep.

### References

The feature set was researched from Stardock's own material, press coverage, and file metadata:

- **Stardock product pages:**
  - [product page](https://www.stardock.com/products/fences/)
  - [version history](https://www.stardock.com/products/fences/history)
  - [Fences 6 announcement](https://www.stardock.com/news/535945/stardock-releases-fences-6)
  - [Fences 6.20 release notes](https://forums.wincustomize.com/539869/releasing-fences-6-20-with-new-features-and-enhancements)
  - [Fences 4 guided tour](https://forums.wincustomize.com/508751/fences-4-a-guided-tour)
- **Stardock support:**
  - [Support FAQ](https://forums.wincustomize.com/486084/fences-support-faq)
  - [Support Hub](https://stardock.atlassian.net/wiki/spaces/SHC/pages/1326219282/Fences)
  - [backup and restore](https://stardock.atlassian.net/wiki/spaces/SHC/pages/1331331082)
  - [enabling or disabling Fences per user](https://stardock.atlassian.net/wiki/spaces/SHC/pages/1397358593/How+to+Enable+Disable+Fences+for+other+users)
- **Press:**
  - [Wikipedia](https://en.wikipedia.org/wiki/Fences_(software))
  - [BetaNews on 6.0](https://betanews.com/2025/06/02/fences-6-0-is-the-essential-desktop-upgrade-for-windows-10-and-11-users-get-it-today/)
  - [BetaNews on 6.20](https://betanews.com/2025/12/11/stardock-releases-fences-6-20-with-new-rollup-options-and-desktop-fixes/)
  - [Neowin on Fences 5](https://www.neowin.net/news/fences-5-exits-preview-available-now-on-windows-for-999/)
- **Third-party listings:**
  - [FencesMenu64.dll metadata (FreeFixer)](https://www.freefixer.com/library/file/FencesMenu64.dll-296440)
  - [AlternativeTo](https://alternativeto.net/software/fences)
