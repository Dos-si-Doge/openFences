// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#pragma once

#include <windows.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace of {

constexpr UINT WM_APP_INVOKE = WM_APP + 1;

// A background thread with a COM single-threaded apartment that runs queued tasks in order.
class Worker {
public:
    explicit Worker(const wchar_t* name);
    ~Worker();
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    void Post(std::function<void()> task);
    void ClearPending();

private:
    void Run(std::wstring name);

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::function<void()>> m_tasks;
    bool m_stop = false;
    std::thread m_thread;
};

// Marshals work back to the UI thread through a window that forwards WM_APP_INVOKE to HandleUiInvoke.
void InitUiDispatcher(HWND hwnd);
void PostToUi(std::function<void()> fn);
// Call from the dispatcher window's procedure. Returns true if the message was handled.
bool HandleUiInvoke(UINT msg, WPARAM wp, LPARAM lp);

}  // namespace of
