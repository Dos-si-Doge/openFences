// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <vector>

namespace of {

// Built-in documentation shown in the "Welcome to openFences" window (modeled on PowerToys' OOBE).
// Keep it in step with docs/user-guide/guide.src.html; WelcomeTests checks that every guide anchor, hero image and
// settings page referenced here exists.

// One "How to use" row: optional key/mouse chips followed by text. The chip "{peek}" is replaced at runtime with the
// Peek shortcut that is actually registered.
struct WelcomeStep {
    std::vector<std::wstring> keys;
    std::wstring text;
};

struct WelcomePage {
    std::wstring id;            // used by --welcome <id>
    std::wstring navTitle;      // sidebar label
    std::wstring title;
    std::wstring description;
    std::vector<WelcomeStep> steps;
    std::wstring settingsPage;  // Settings page opened by "Open settings" ("" = no button)
    std::wstring guideAnchor;   // section of the HTML user guide opened by "Learn more" ("" = no link)
    std::wstring hero;          // res/welcome/<hero>.jpg, embedded as RCDATA "WELCOME_<HERO>"
    bool heroFit = false;       // true: screenshot, framed with padding on a neutral band; false: illustration, edge to edge
    bool credits = false;       // show the version / author / license line and "Report a bug or idea" (the overview)
};

const std::vector<WelcomePage>& WelcomePages();
// Index of the page with this id, or 0 (the overview) when unknown.
int WelcomePageIndex(const std::wstring& id);

constexpr wchar_t kWhatsNewPageId[] = L"whatsnew";

// Size at which a hero image of imageW x imageH pixels is drawn in a band availW pixels wide: the whole image,
// proportional, and never larger than its native size (wider bands center it instead of zooming in).
struct HeroSize {
    int width = 0;
    int height = 0;
};
HeroSize FitHero(int imageW, int imageH, int availW);

}  // namespace of
