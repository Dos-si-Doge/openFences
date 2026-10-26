// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "ui/Help.h"

#include <shellapi.h>
#include <shlwapi.h>

#include <filesystem>

#include "Version.h"
#include "core/Paths.h"

namespace of {

bool OpenUserGuide(HWND owner, const std::wstring& anchor) {
    std::filesystem::path guide = paths::ExePath().parent_path() / L"openFences-User-Guide.html";
    std::error_code ec;
    if (!std::filesystem::exists(guide, ec)) {
        MessageBoxW(owner, L"The user guide wasn't found next to openFences.exe.", L"openFences", MB_ICONINFORMATION);
        return false;
    }
    if (!anchor.empty()) {
        // The .html file association drops "#section", so give the file URL to the default browser directly.
        wchar_t url[2084];  // INTERNET_MAX_URL_LENGTH
        DWORD urlLen = static_cast<DWORD>(std::size(url));
        wchar_t browser[MAX_PATH];
        DWORD browserLen = MAX_PATH;
        if (SUCCEEDED(UrlCreateFromPathW(guide.c_str(), url, &urlLen, 0)) &&
            SUCCEEDED(AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, L"http", L"open", browser, &browserLen))) {
            std::wstring args = L"\"" + std::wstring(url) + L"#" + anchor + L"\"";
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(owner, nullptr, browser, args.c_str(), nullptr, SW_SHOWNORMAL)) > 32) return true;
        }
    }
    return reinterpret_cast<INT_PTR>(ShellExecuteW(owner, L"open", guide.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

bool OpenIssuesPage(HWND owner) {
    return reinterpret_cast<INT_PTR>(ShellExecuteW(owner, L"open", OF_ISSUES_URL, nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

bool OpenLicense(HWND owner) {
    std::filesystem::path license = paths::ExePath().parent_path() / L"LICENSE.txt";
    std::error_code ec;
    if (!std::filesystem::exists(license, ec)) {
        MessageBoxW(owner, L"openFences is released under the MIT License. LICENSE.txt wasn't found next to openFences.exe.", L"openFences",
                    MB_ICONINFORMATION);
        return false;
    }
    return reinterpret_cast<INT_PTR>(ShellExecuteW(owner, L"open", license.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

std::wstring CreditsLine() {
    return std::wstring(L"openFences ") + OF_VERSION_WSTRING + L"  \u00B7  Created by " + OF_AUTHOR_WSTRING + L"  \u00B7  Free and open source under the " +
           OF_LICENSE_WSTRING;
}

}  // namespace of
