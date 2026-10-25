#include "core/Autostart.h"

#include <windows.h>

namespace of::autostart {

namespace {
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kApprovedKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
constexpr wchar_t kAppKey[] = L"Software\\openFences";
constexpr wchar_t kMachineKey[] = L"SOFTWARE\\openFences";
}  // namespace

bool IsApprovedDataEnabled(const std::vector<uint8_t>& data) {
    if (data.empty()) return true;
    return (data[0] & 1) == 0;
}

State Query() {
    State s;
    wchar_t buf[2048];
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, nullptr, buf, &size) ==
        ERROR_SUCCESS) {
        s.runValuePresent = true;
        s.command = buf;
    }
    std::vector<uint8_t> data(64);
    DWORD dsize = static_cast<DWORD>(data.size());
    if (RegGetValueW(HKEY_CURRENT_USER, kApprovedKey, kValueName, RRF_RT_REG_BINARY, nullptr, data.data(), &dsize) == ERROR_SUCCESS) {
        data.resize(dsize);
        s.disabledByUser = !IsApprovedDataEnabled(data);
    }
    return s;
}

std::wstring CommandLineFor(const std::wstring& exePath) {
    return L"\"" + exePath + L"\" --autostart";
}

bool Set(bool enable, const std::wstring& exePath) {
    if (enable) {
        std::wstring cmd = CommandLineFor(exePath);
        LSTATUS st = RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName, REG_SZ, cmd.c_str(),
                                     static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
        // Clear any "disabled" mark left by Task Manager so the entry actually runs.
        RegDeleteKeyValueW(HKEY_CURRENT_USER, kApprovedKey, kValueName);
        return st == ERROR_SUCCESS;
    }
    LSTATUS st = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName);
    RegDeleteKeyValueW(HKEY_CURRENT_USER, kApprovedKey, kValueName);
    return st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND;
}

bool ApplyInstallerDefaultOnce(const std::wstring& exePath) {
    DWORD applied = 0, size = sizeof(applied);
    if (RegGetValueW(HKEY_CURRENT_USER, kAppKey, L"AutostartDefaultApplied", RRF_RT_REG_DWORD, nullptr, &applied, &size) == ERROR_SUCCESS &&
        applied) {
        return false;
    }
    DWORD def = 0;
    size = sizeof(def);
    bool haveDefault = RegGetValueW(HKEY_LOCAL_MACHINE, kMachineKey, L"DefaultAutostart", RRF_RT_REG_DWORD, nullptr, &def, &size) == ERROR_SUCCESS;
    DWORD one = 1;
    RegSetKeyValueW(HKEY_CURRENT_USER, kAppKey, L"AutostartDefaultApplied", REG_DWORD, &one, sizeof(one));
    if (!haveDefault) return false;
    Set(def != 0, exePath);
    return true;
}

}  // namespace of::autostart
