// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "core/Welcome.h"

#include <algorithm>

#include "Version.h"

namespace of {

const std::vector<WelcomePage>& WelcomePages() {
    static const std::vector<WelcomePage> pages = {
        {L"welcome", L"Welcome", L"Welcome to openFences",
         L"openFences organizes your desktop into fences: labeled, see-through areas that hold your icons. Your files stay "
         L"where they are, and closing openFences brings back the normal Windows desktop. Pick a topic on the left to see how "
         L"each feature works.",
         {{{L"Drag"}, L"Draw a rectangle on empty desktop to create a fence, then drag icons into it."},
          {{L"Double-click"}, L"Double-click empty desktop to hide all fences and icons; double-click again to bring them back."},
          {{L"{peek}"}, L"Bring your fences above open windows (Peek)."},
          {{}, L"Right-click empty desktop or the openFences tray icon for more options."}},
         L"welcome", L"welcome", L"hero", false, true},

        {L"fences", L"Fences", L"Fences",
         L"A fence is a panel on the desktop that holds a group of icons. Icons inside work exactly like desktop icons: open "
         L"them, right-click them for the Windows menu, rename or delete them.",
         {{{L"Drag"}, L"Drag a fence by its title bar to move it. It snaps into line with other fences and the screen edges."},
          {{L"Drag"}, L"Drag any edge or corner to resize it."},
          {{L"≡"}, L"Click the menu button in the title bar for view, sort, color, tab and lock options."},
          {{L"F2"}, L"Rename the selected icon, or the fence when no icon is selected."},
          {{L"×"}, L"Delete a fence with the × button. Its icons go back to the desktop; nothing is deleted."}},
         L"layout", L"fences", L"anatomy"},

        {L"rules", L"Rules", L"Sorting automatically with rules",
         L"Rules put new desktop items into the right fence for you, by file type, name, shortcut target or date. A rule can "
         L"also move matching files off the desktop into a folder.",
         {{{}, L"Rules are checked from top to bottom and the first match wins."},
          {{}, L"They act on new items. Use “Sort desktop with rules now” to sort what is already on the desktop."},
          {{}, L"Rules never take an icon out of a fence you put it in yourself."}},
         L"rules", L"rules", L"settings-rules", true},

        {L"portals", L"Folder Portals", L"Folder Portals",
         L"A Folder Portal is a fence that shows the live contents of any folder, such as Downloads, a OneDrive folder or a "
         L"network share. Files you drop on it are moved or copied into that folder.",
         {{{L"Drag"}, L"Draw a rectangle on the desktop and choose “Create folder portal here”."},
          {{L"Double-click"}, L"Open a subfolder inside the portal; click the ‹ bar to go back."},
          {{L"Ctrl", L"Double-click"}, L"Open the folder in File Explorer instead."},
          {{L"F5"}, L"Refresh. Unavailable network folders are retried automatically."}},
         L"portals", L"portals", L"portal"},

        {L"tabs", L"Tabs", L"Tabs",
         L"Tabs stack several fences into one panel to save space. A tabbed fence can mix ordinary fences and Folder Portals.",
         {{{L"Drag"}, L"Drop one fence's title onto another fence's title to combine them."},
          {{L"Drag"}, L"Drag a tab down out of the title bar to make it a separate fence again."},
          {{}, L"Choose Flat, Segmented, Rounded or Hidden tabs in Settings, and color tabs from the fence menu."}},
         L"tabs", L"tabs", L"tabs"},

        {L"rollup", L"Roll-up", L"Rolling fences up",
         L"Roll a fence up to just its title bar to keep the desktop tidy. It opens again when you point at it.",
         {{{L"Double-click"}, L"Double-click a title bar to roll the fence up or down."},
          {{}, L"Choose whether rolled-up fences open on hover, after a short pause, or only on click."},
          {{}, L"Fences against the top or bottom screen edge roll up automatically (you can turn this off)."}},
         L"rollup", L"rollup", L"rollup"},

        {L"hide", L"Quick-Hide", L"Quick-Hide",
         L"Fade out every fence and icon for a clean wallpaper, then bring them back just as quickly. openFences remembers "
         L"the choice, even after a restart.",
         {{{L"Double-click"}, L"Double-click empty desktop to hide or show everything."},
          {{}, L"Keep a fence or icon visible with “Keep visible during Quick-Hide”. Visible icons still open normally."},
          {{}, L"Optionally hide icons automatically while you work in other apps."}},
         L"hide", L"hide", L"quickhide"},

        {L"peek", L"Peek", L"Peek and Desktop Display",
         L"Peek brings your fences above open windows without minimizing anything. Desktop Display shows the whole desktop "
         L"instantly, without the minimize animation.",
         {{{L"{peek}"}, L"Start or end Peek. Choose toggle or hold-to-peek in Settings."},
          {{L"Esc"}, L"End Peek; clicking empty space or opening an item also ends it."},
          {{}, L"Click the tray icon, or rest the pointer in the hot corner, to Peek."},
          {{L"Win", L"D"}, L"Optionally use Win+D for Desktop Display (Settings → Peek)."}},
         L"peek", L"peek", L"peek"},

        {L"pages", L"Pages", L"Pages",
         L"Keep several desktops of fences, for example Work, Play and School, and flick between them.",
         {{{L"Drag"}, L"Press and hold at the far left or right edge of the screen and pull toward the middle."},
          {{}, L"Add, rename and delete pages from the desktop right-click menu → Pages."},
          {{}, L"Show a fence on every page with “Show on all pages” in its menu."}},
         L"pages", L"pages", L"pages"},

        {L"appearance", L"Appearance", L"Appearance",
         L"Change how fences and icons look: fence colors and transparency, Icon Tint to color icons, and Chameleon to fade "
         L"icons into the wallpaper until you point at them.",
         {{{}, L"Set the default fence color, opacity, corners and icon size in Settings → Personalization."},
          {{}, L"Give a single fence its own color or icon tint from its menu."},
          {{}, L"Turn on Chameleon in Settings → Chameleon."}},
         L"personalization", L"appearance", L"appearance"},

        {L"snapshots", L"Snapshots", L"Snapshots, backups and monitors",
         L"Save your whole layout and restore it whenever you like. openFences also backs up daily and keeps a separate layout "
         L"for each monitor arrangement, so docking and undocking a laptop restores the right one.",
         {{{}, L"Take, preview and restore snapshots in Settings → Layout."},
          {{}, L"Daily backups keep the last 7 days. “Before openFences” holds your desktop from the first start."},
          {{}, L"Lock fences there too, so they can't be moved by accident."}},
         L"layout", L"snapshots", L"settings-layout", true},

        {kWhatsNewPageId, L"What's new", std::wstring(L"What's new in openFences ") + OF_VERSION_WSTRING,
         L"Highlights of this release. The full list of changes is in CHANGELOG.md.",
         {{{}, L"New Welcome to openFences window: a quick, built-in tour of every feature with links to the right settings "
               L"and to the full user guide."},
          {{}, L"Open it from the bottom of the Settings sidebar, from the tray menu, or with openFences.exe --welcome."},
          {{}, L"It opens by itself on the first start, and after an update it shows this page."},
          {{}, L"Peek now falls back to Ctrl+Alt+Shift+Space when Win+Space and Ctrl+Alt+Space are already taken by other software."}},
         L"", L"welcome-window", L"hero"},
    };
    return pages;
}

HeroSize FitHero(int imageW, int imageH, int availW) {
    if (imageW <= 0 || imageH <= 0 || availW <= 0) return {};
    if (availW >= imageW) return {imageW, imageH};
    return {availW, std::max(1, static_cast<int>(static_cast<long long>(imageH) * availW / imageW))};
}

int WelcomePageIndex(const std::wstring& id) {
    const auto& pages = WelcomePages();
    for (size_t i = 0; i < pages.size(); ++i)
        if (pages[i].id == id) return static_cast<int>(i);
    return 0;
}

}  // namespace of
