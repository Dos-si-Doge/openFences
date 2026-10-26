# Changelog

All notable changes to openFences are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Each release ships as an MSI installer and a portable zip (`out\openFences-<version>-x64.msi` and
`out\openFences-<version>-x64-portable.zip`).

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
