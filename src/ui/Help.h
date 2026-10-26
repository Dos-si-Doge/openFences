// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <windows.h>

#include <string>

namespace of {

// Opens the external HTML user guide (installed next to openFences.exe) in the default browser, scrolled to `anchor`
// when given. Shows a message and returns false if the guide is missing.
bool OpenUserGuide(HWND owner, const std::wstring& anchor = {});

// Opens the project's GitHub issues page (bug reports and feature requests) in the default browser.
bool OpenIssuesPage(HWND owner);

// Opens LICENSE.txt (installed next to openFences.exe). Shows a message and returns false if it is missing.
bool OpenLicense(HWND owner);

// "openFences <version> · Created by <author> · <license>", for the About page and the Welcome topic.
std::wstring CreditsLine();

}  // namespace of
