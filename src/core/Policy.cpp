#include "core/Policy.h"

#include <windows.h>

#include "core/Strings.h"

namespace of {

namespace {
constexpr wchar_t kPolicyKey[] = L"SOFTWARE\\Policies\\openFences";
constexpr wchar_t kUserKey[] = L"Software\\openFences";

std::optional<DWORD> ReadDword(HKEY root, const wchar_t* key, const wchar_t* name) {
    DWORD v = 0, size = sizeof(v);
    if (RegGetValueW(root, key, name, RRF_RT_REG_DWORD, nullptr, &v, &size) == ERROR_SUCCESS) return v;
    return std::nullopt;
}

std::wstring ReadString(HKEY root, const wchar_t* key, const wchar_t* name) {
    wchar_t buf[2048];
    DWORD size = sizeof(buf);
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, nullptr, buf, &size) == ERROR_SUCCESS) return buf;
    return {};
}
}  // namespace

Policy ReadPolicy() {
    Policy p;
    p.disabled = ReadDword(HKEY_LOCAL_MACHINE, kPolicyKey, L"Disabled").value_or(0) != 0 ||
                 ReadDword(HKEY_CURRENT_USER, kUserKey, L"Disabled").value_or(0) != 0;
    p.configPath = ExpandEnv(ReadString(HKEY_LOCAL_MACHINE, kPolicyKey, L"ConfigPath"));
    if (auto v = ReadDword(HKEY_LOCAL_MACHINE, kPolicyKey, L"LockLayout")) p.lockLayout = *v != 0;
    if (auto v = ReadDword(HKEY_LOCAL_MACHINE, kPolicyKey, L"LockContent")) p.lockContent = *v != 0;
    p.disableSettings = ReadDword(HKEY_LOCAL_MACHINE, kPolicyKey, L"DisableSettings").value_or(0) != 0;
    return p;
}

}  // namespace of
