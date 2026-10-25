#include "shell/ChangeWatcher.h"

namespace of {

bool ChangeWatch::Start(HWND hwnd, UINT msg, PCIDLIST_ABSOLUTE folder, bool recursive) {
    Stop();
    SHChangeNotifyEntry entry{folder, recursive ? TRUE : FALSE};
    LONG events = SHCNE_CREATE | SHCNE_DELETE | SHCNE_MKDIR | SHCNE_RMDIR | SHCNE_RENAMEITEM | SHCNE_RENAMEFOLDER | SHCNE_UPDATEITEM |
                  SHCNE_UPDATEDIR | SHCNE_UPDATEIMAGE | SHCNE_ATTRIBUTES | SHCNE_DRIVEADD | SHCNE_DRIVEREMOVED | SHCNE_MEDIAINSERTED |
                  SHCNE_MEDIAREMOVED | SHCNE_NETSHARE | SHCNE_NETUNSHARE | SHCNE_SERVERDISCONNECT | SHCNE_ASSOCCHANGED;
    m_id = SHChangeNotifyRegister(hwnd, SHCNRF_ShellLevel | SHCNRF_InterruptLevel | SHCNRF_NewDelivery, events, msg, 1, &entry);
    return m_id != 0;
}

void ChangeWatch::Stop() {
    if (m_id) SHChangeNotifyDeregister(m_id);
    m_id = 0;
}

void DecodeChangeNotification(WPARAM wp, LPARAM lp, const std::function<void(LONG, PCIDLIST_ABSOLUTE, PCIDLIST_ABSOLUTE)>& fn) {
    PIDLIST_ABSOLUTE* pidls = nullptr;
    LONG event = 0;
    HANDLE lock = SHChangeNotification_Lock(reinterpret_cast<HANDLE>(wp), static_cast<DWORD>(lp), &pidls, &event);
    if (!lock) return;
    fn(event, pidls ? pidls[0] : nullptr, pidls ? pidls[1] : nullptr);
    SHChangeNotification_Unlock(lock);
}

}  // namespace of
