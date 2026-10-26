// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <windows.h>

#include <string>

namespace of::ipc {

constexpr wchar_t kMessageWindowClass[] = L"openFences.Controller";
constexpr wchar_t kMutexName[] = L"Local\\openFences.SingleInstance";
constexpr ULONG_PTR kCopyDataCommand = 0x6F46;  // 'oF'

// Sends a command to the running instance (WM_COPYDATA). Returns false if none is running.
bool SendCommand(const std::wstring& verb, const std::wstring& arg);
bool DecodeCommand(LPARAM lp, std::wstring& verb, std::wstring& arg);

}  // namespace of::ipc
