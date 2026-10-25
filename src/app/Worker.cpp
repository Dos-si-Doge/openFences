#include "app/Worker.h"

#include <objbase.h>

#include <atomic>

#include "core/Log.h"

namespace of {

Worker::Worker(const wchar_t* name) : m_thread([this, n = std::wstring(name)] { Run(n); }) {}

Worker::~Worker() {
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
        m_tasks.clear();
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void Worker::Post(std::function<void()> task) {
    {
        std::lock_guard lock(m_mutex);
        if (m_stop) return;
        m_tasks.push_back(std::move(task));
    }
    m_cv.notify_one();
}

void Worker::ClearPending() {
    std::lock_guard lock(m_mutex);
    m_tasks.clear();
}

void Worker::Run(std::wstring name) {
    SetThreadDescription(GetCurrentThread(), name.c_str());
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
            if (m_stop) break;
            task = std::move(m_tasks.front());
            m_tasks.pop_front();
        }
        try {
            task();
        } catch (const std::exception& e) {
            log::Info(L"worker {}: task threw: {}", name, std::wstring(e.what(), e.what() + strlen(e.what())));
        }
    }
    if (SUCCEEDED(hr)) CoUninitialize();
}

namespace {
std::atomic<HWND> g_uiWindow{nullptr};
}

void InitUiDispatcher(HWND hwnd) { g_uiWindow = hwnd; }

void PostToUi(std::function<void()> fn) {
    HWND hwnd = g_uiWindow.load();
    if (!hwnd) return;
    auto* heap = new std::function<void()>(std::move(fn));
    if (!PostMessageW(hwnd, WM_APP_INVOKE, 0, reinterpret_cast<LPARAM>(heap))) delete heap;
}

bool HandleUiInvoke(UINT msg, WPARAM, LPARAM lp) {
    if (msg != WM_APP_INVOKE) return false;
    std::unique_ptr<std::function<void()>> fn(reinterpret_cast<std::function<void()>*>(lp));
    if (fn && *fn) (*fn)();
    return true;
}

}  // namespace of
