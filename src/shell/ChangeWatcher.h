#pragma once

#include <functional>

#include "shell/ShellItem.h"

namespace of {

// Thin wrapper over SHChangeNotifyRegister (new-delivery mode).
class ChangeWatch {
public:
    ChangeWatch() = default;
    ~ChangeWatch() { Stop(); }
    ChangeWatch(const ChangeWatch&) = delete;
    ChangeWatch& operator=(const ChangeWatch&) = delete;

    bool Start(HWND hwnd, UINT msg, PCIDLIST_ABSOLUTE folder, bool recursive);
    void Stop();
    bool Active() const { return m_id != 0; }

private:
    ULONG m_id = 0;
};

// Decodes a change notification message. The callback receives the event and up to two PIDLs.
void DecodeChangeNotification(WPARAM wp, LPARAM lp, const std::function<void(LONG event, PCIDLIST_ABSOLUTE a, PCIDLIST_ABSOLUTE b)>& fn);

}  // namespace of
