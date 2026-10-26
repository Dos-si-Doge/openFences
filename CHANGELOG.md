# Changelog

All notable changes to openFences are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Each release ships as an MSI installer and a portable zip (`out\openFences-<version>-x64.msi` and
`out\openFences-<version>-x64-portable.zip`).

## [1.0.3] - 2026-10-25

### Fixed
- The desktop right-click menu showed an "openFences ›" entry that did nothing when hovered or clicked. It's the
  installer's submenu for Explorer's own desktop menu. Windows fills that kind of registry-defined submenu only inside
  Explorer, so in openFences' menu it was always empty. openFences now leaves it out; all of its commands (Configure,
  New fence, Peek, Hide, Lock) are already direct entries in the menu. It still works in Explorer's menu while openFences
  isn't running.
- Settings now adapts to its window size, like the Welcome window's pictures:
  - The snapshot preview on the Layout page scales with the window and always shows the whole monitor in its real
    proportions (up to 360 px tall), instead of a fixed-height strip where the map stayed small.
  - Long labels and texts wrap instead of being cut off in a narrow window, including long folder paths on the About page.
  - Content stops widening at a readable width, so sliders and lists no longer stretch across a maximized window.
- "Configure openFences…" is the last item of the desktop menu again. Windows adds some of its own items (Display
  settings, Personalize, wallpaper slideshow commands) at the very bottom of any menu, which pushed it into the middle.

### Security
- **Commands from sandboxed processes are blocked.**
  - openFences used to lift Windows' message filter (UIPI) for its command window. Any process, even a low-integrity
    sandboxed one, could then send commands such as `export <path>` (overwrite a file) or `import <file>`.
  - Now the filter is lifted only when openFences runs elevated, so non-elevated command lines can still reach that
    instance.
  - Incoming command data is also validated (no missing buffer, size limit).
- **Import asks before moving files.** **Settings → Import layout…** now lists where a layout's "move file to folder"
  rules would move desktop files (flagging network locations) and asks before importing. A crafted layout file could
  otherwise move desktop files to a network share without warning.
- **Hardened exe.** It's built with Control Flow Guard, resolves its DLL imports from System32 only and never loads DLLs
  from the current directory. This blocks DLL planting next to the portable exe, for example in Downloads.

### Fixed (static analysis)
MSVC `/analyze` over the whole codebase is now clean. It found:
- A crash when the system is out of memory while building a context menu for several items (`ILCloneFull` returning
  null).
- Settings and Welcome continuing with a null window if creating the window failed.
- Missing checks and annotations in the wallpaper lookup, drag helper and `wWinMain`.

The PowerShell scripts were checked with PSScriptAnalyzer and the Python scripts with pyflakes. A `$null` comparison
in `WelcomeTests`, a silent `catch` and a helper name in `install-toolchain.ps1` were fixed.

### Changed
- `scripts/install-toolchain.ps1` sets up a development machine from a clean Windows 11 install:
  - It installs Git, Visual Studio 2022 Build Tools (C++ workload, Windows 11 SDK, CMake, Ninja), .NET 8 SDK, WiX 5.0.2
    with its extensions, and Python 3, plus VS Code with the C/C++ and CMake Tools extensions and GitHub CLI (skip these
    with `-Minimal`).
  - It works without the repository and can clone and build it (`-CloneTo`, `-Build`).
  - It skips anything already installed, adds missing C++ components to an existing Build Tools install, needs a single
    UAC prompt, and has a `-DryRun` mode.
  - It explains, at the top of the file and in the README, how to get past "running scripts is disabled on this
    system" on a new machine (`Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`, `Unblock-File`), and sets
    *RemoteSigned* for the current user itself when the policy is stricter.
- The user guide's picture of an icon's right-click menu ("Working with icons") is now a drawn illustration, like the
  guide's other pictures, instead of a screenshot. The screenshot showed shell extensions installed on the PC it was
  taken on.
- **Integration tests are safer on a machine that also runs openFences:**
  - `AutostartTests` sets aside an existing "start at sign-in" entry (for example from an installed copy) and puts it
    back afterwards. Before, it deleted it.
  - `AutostartTests` and `SettingsTests` refuse to start while openFences is running.
  - `InteractionTests` skips its drags, and reports them as SKIP, when the real mouse pointer rests on an openFences
    window. Virtual machines keep sending pointer events, so the drag followed the real pointer and could merge fences,
    which made later checks fail.

## [1.0.2] - 2026-10-25

### Added
- **Welcome to openFences**, built-in documentation modeled on PowerToys' "Welcome to PowerToys" window:
  - A sidebar of topics: Welcome, Fences, Rules, Folder Portals, Tabs, Roll-up, Quick-Hide, Peek, Pages, Appearance,
    Snapshots and What's new.
  - Each topic has a picture, a description and "How to use" rows with key chips. The Peek page shows the shortcut that's
    actually registered.
  - Pictures are always shown whole. They scale proportionally with the window and stop growing at their native size
    instead of zooming in and cropping.
  - **Open settings** jumps to the matching Settings page, and **Learn more** opens the HTML user guide at the matching
    section.
- **Ways to open it:** a button at the bottom of the Settings sidebar, Settings → Welcome, the tray menu, and
  `openFences.exe --welcome [topic]`.
- **Automatic opening:** on the first start (instead of the old balloon tip), and on **What's new** after an update.
  Turn the latter off with Settings → Welcome → "Show what's new after openFences is updated". `--no-welcome` skips
  automatic opening for one run, for scripted deployments.
- **New user guide content:** an Appearance illustration (fence colors, Icon Tint, Chameleon) and a section on the
  Welcome window.
- **Credits and license.** openFences is created by Dos-si-Doge and released under the MIT License (`LICENSE`).
  - Settings → About shows the author, copyright and license, with **Report a bug or idea** (GitHub Issues) and
    **View license** buttons. The Welcome topic ends with the same credits line and button.
  - The exe's file properties (CompanyName, LegalCopyright) and the MSI (Manufacturer, Settings → Apps support link)
    name Dos-si-Doge. `LICENSE.txt` is installed by the MSI and included in the portable zip.
  - Every source file carries a copyright / `SPDX-License-Identifier: MIT` header.
  - The user guide's footer, About row and Troubleshooting (how to report a bug) and the README cover the same.

### Fixed
- Peek could end up with no shortcut when both Win+Space and Ctrl+Alt+Space were already owned by other software.
  openFences now also tries Ctrl+Alt+Shift+Space and logs which shortcut it registered.

### Changed
- The tray menu has a "Welcome to openFences…" item.
- Opening the user guide goes through one helper. "Learn more" links open the guide in the default browser at the
  right section.

## [1.0.1] - 2026-10-25

### Fixed
- Double-clicking a desktop icon that stays visible during Quick-Hide now opens it and leaves Quick-Hide on.
  Previously openFences treated every loose icon as hidden while Quick-Hide was active, so the double-click also counted
  as a double-click on empty desktop and turned Quick-Hide off.
- The same icons can now also be selected, dragged, right-clicked and highlighted on hover during Quick-Hide.

### Added
- **Portable zip** (`openFences-<version>-x64-portable.zip`) built alongside the MSI. Unzip it anywhere and run
  `openFences.exe`; no installation needed.
- **Portable mode.** When an `openFences.portable` file sits next to `openFences.exe`, settings, backups and logs are kept
  in a `Data` folder beside it instead of `%APPDATA%` / `%LOCALAPPDATA%`. Settings → About shows when portable mode is on.
- The user guide is installed with openFences (and included in the zip), and **Settings → About → Open user guide** opens it.

## [1.0.0] - 2026-10-24

First release.

- Fences on the desktop:
  - drag-to-create
  - move and resize with snapping
  - rename, delete, scrolling
  - per-fence icon size, sort order, color and icon tint
- Shell-native icons: Explorer context menus, open, rename, delete, cut/copy/paste, drag and drop.
- Rules by file type, name (glob or regex), shortcut target and dates. They are ordered (first match wins) and can put
  an item in a fence or move the file to a folder.
- Folder Portals with navigation, list layouts, drop to move or copy, and offline retry.
- Tabs, roll-up, and Quick-Hide with exclusions and auto-hide.
- Peek (hotkey, tray icon, hot corner) and Desktop Display (instant "show desktop", optional Win+D).
- Pages with edge swipe and fences shown on every page.
- Chameleon, Icon Tint and wallpaper-aware colors.
- Snapshots, daily backups, layouts per monitor arrangement, and Per-Monitor-V2 DPI awareness.
- Locks, templates (Default, Work, Kiosk), export and import, and administrator policies including a network-pushed
  `ConfigPath`.
- Start at sign-in, with an installer option. Respects the Task Manager startup setting.
- Watchdog that restores Explorer's desktop icons if openFences stops unexpectedly.
- WiX MSI installer, HTML user guide, unit and integration tests.
