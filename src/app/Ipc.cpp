// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "app/Ipc.h"

namespace of::ipc {

bool SendCommand(const std::wstring& verb, const std::wstring& arg) {
    HWND target = FindWindowW(kMessageWindowClass, nullptr);
    if (!target) return false;
    std::wstring payload = verb + L'\n' + arg;
    COPYDATASTRUCT cds{};
    cds.dwData = kCopyDataCommand;
    cds.cbData = static_cast<DWORD>((payload.size() + 1) * sizeof(wchar_t));
    cds.lpData = payload.data();
    // Let the running instance take the foreground (it may open Settings or a menu).
    DWORD pid = 0;
    GetWindowThreadProcessId(target, &pid);
    AllowSetForegroundWindow(pid);
    DWORD_PTR result = 0;
    return SendMessageTimeoutW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds), SMTO_ABORTIFHUNG, 5000, &result) != 0;
}

bool DecodeCommand(LPARAM lp, std::wstring& verb, std::wstring& arg) {
    auto* cds = reinterpret_cast<COPYDATASTRUCT*>(lp);
    if (!cds || cds->dwData != kCopyDataCommand || cds->cbData < sizeof(wchar_t)) return false;
    std::wstring payload(static_cast<const wchar_t*>(cds->lpData), cds->cbData / sizeof(wchar_t));
    while (!payload.empty() && payload.back() == L'\0') payload.pop_back();
    size_t nl = payload.find(L'\n');
    verb = payload.substr(0, nl);
    arg = nl == std::wstring::npos ? L"" : payload.substr(nl + 1);
    return true;
}

}  // namespace of::ipc
