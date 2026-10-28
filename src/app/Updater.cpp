// Copyright (c) 2026 Dos-si-Doge
// SPDX-License-Identifier: MIT

#include "app/Updater.h"

#include <windows.h>
#include <bcrypt.h>
#include <msi.h>
#include <msiquery.h>
#include <softpub.h>
#include <wintrust.h>
#include <winhttp.h>

#include <atomic>
#include <filesystem>
#include <format>
#include <functional>
#include <vector>

#include "Version.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Policy.h"
#include "core/Strings.h"

namespace of::updater {

namespace {

// ------------------------------------------------------------------ registry

std::optional<DWORD> GetDword(const wchar_t* name) {
    DWORD v = 0, size = sizeof(v);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, kUpdateKey, name, RRF_RT_REG_DWORD, nullptr, &v, &size) == ERROR_SUCCESS) return v;
    return std::nullopt;
}

std::optional<ULONGLONG> GetQword(const wchar_t* name) {
    ULONGLONG v = 0;
    DWORD size = sizeof(v);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, kUpdateKey, name, RRF_RT_REG_QWORD, nullptr, &v, &size) == ERROR_SUCCESS) return v;
    return std::nullopt;
}

std::wstring GetString(const wchar_t* name) {
    wchar_t buf[1024];
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, kUpdateKey, name, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS) return buf;
    return {};
}

struct Key {
    HKEY h = nullptr;
    ~Key() {
        if (h) RegCloseKey(h);
    }
};

bool OpenForWrite(Key& key) {
    return RegCreateKeyExW(HKEY_LOCAL_MACHINE, kUpdateKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key.h, nullptr) == ERROR_SUCCESS;
}

bool SetDword(HKEY k, const wchar_t* name, DWORD v) {
    return RegSetValueExW(k, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof(v)) == ERROR_SUCCESS;
}

bool SetString(HKEY k, const wchar_t* name, const std::wstring& v) {
    return RegSetValueExW(k, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()), static_cast<DWORD>((v.size() + 1) * sizeof(wchar_t))) ==
           ERROR_SUCCESS;
}

// ------------------------------------------------------------------ service control manager

struct Sc {
    SC_HANDLE h = nullptr;
    explicit Sc(SC_HANDLE handle) : h(handle) {}
    ~Sc() {
        if (h) CloseServiceHandle(h);
    }
};

// ------------------------------------------------------------------ WinHTTP

struct Http {
    HINTERNET h = nullptr;
    explicit Http(HINTERNET handle) : h(handle) {}
    ~Http() {
        if (h) WinHttpCloseHandle(h);
    }
};

std::wstring QueryHeader(HINTERNET request, DWORD info) {
    DWORD size = 0;
    WinHttpQueryHeaders(request, info, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) return {};
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(request, info, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &size, WINHTTP_NO_HEADER_INDEX)) return {};
    value.resize(size / sizeof(wchar_t));
    return value;
}

// GET with TLS certificate validation (WinHTTP's default), no automatic redirects: every hop is checked against the
// allow-list (core/Update IsAllowedUrl). sink receives the body; returning false from it aborts.
bool HttpGet(const std::wstring& url, const std::wstring& feed, const wchar_t* accept, uint64_t maxBytes,
             const std::function<bool(const BYTE*, DWORD)>& sink, std::wstring& error) {
    const std::wstring agent = std::wstring(L"openFences-updater/") + OF_VERSION_WSTRING;
    Http session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.h) {
        error = L"the network library couldn't start";
        return false;
    }
    WinHttpSetTimeouts(session.h, 15000, 15000, 30000, 60000);
    std::wstring current = url;
    for (int hop = 0; hop < 6; ++hop) {
        if (!update::IsAllowedUrl(current, feed)) {
            error = L"refused to download from " + current;
            return false;
        }
        URL_COMPONENTS uc{sizeof(uc)};
        uc.dwHostNameLength = uc.dwUrlPathLength = uc.dwExtraInfoLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(current.c_str(), 0, 0, &uc)) {
            error = L"invalid address " + current;
            return false;
        }
        if (!uc.lpszHostName || uc.dwHostNameLength == 0) {
            error = L"invalid address " + current;
            return false;
        }
        std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
        std::wstring path = (uc.lpszUrlPath ? std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength) : std::wstring(L"/")) +
                            (uc.lpszExtraInfo ? std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength) : std::wstring());
        bool secure = uc.nScheme == INTERNET_SCHEME_HTTPS;
        Http connect(WinHttpConnect(session.h, host.c_str(), uc.nPort, 0));
        Http request(connect.h ? WinHttpOpenRequest(connect.h, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                    secure ? WINHTTP_FLAG_SECURE : 0)
                               : nullptr);
        if (!request.h) {
            error = L"couldn't connect to " + host;
            return false;
        }
        DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(request.h, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects));
        std::wstring headers = std::wstring(L"Accept: ") + accept + L"\r\n";
        if (!WinHttpSendRequest(request.h, headers.c_str(), static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request.h, nullptr)) {
            error = L"couldn't reach " + host + L" (error " + std::to_wstring(GetLastError()) + L")";
            return false;
        }
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                            WINHTTP_NO_HEADER_INDEX);
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            std::wstring location = QueryHeader(request.h, WINHTTP_QUERY_LOCATION);
            if (location.empty()) {
                error = L"a redirect without a location";
                return false;
            }
            if (location[0] == L'/') {  // relative: same scheme, host and port
                bool defaultPort = uc.nPort == (secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);
                location = (secure ? L"https://" : L"http://") + host + (defaultPort ? L"" : L":" + std::to_wstring(uc.nPort)) + location;
            }
            current = location;
            continue;
        }
        if (status != 200) {
            error = host + L" answered HTTP " + std::to_wstring(status);
            return false;
        }
        uint64_t total = 0;
        std::vector<BYTE> buf(64 * 1024);
        for (;;) {
            DWORD read = 0;
            if (!WinHttpReadData(request.h, buf.data(), static_cast<DWORD>(buf.size()), &read)) {
                error = L"the download was interrupted";
                return false;
            }
            if (read == 0) return true;
            total += read;
            if (total > maxBytes) {
                error = L"the download is larger than expected";
                return false;
            }
            if (!sink(buf.data(), read)) {
                error = L"the download was cancelled";
                return false;
            }
        }
    }
    error = L"too many redirects";
    return false;
}

// ------------------------------------------------------------------ verification

struct Sha256 {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    Sha256() {
        if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
            if (!BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0))) hash = nullptr;
    }
    ~Sha256() {
        if (hash) BCryptDestroyHash(hash);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    }
    bool Add(const BYTE* data, DWORD size) { return hash && BCRYPT_SUCCESS(BCryptHashData(hash, const_cast<PUCHAR>(data), size, 0)); }
    std::wstring HexDigest() {
        BYTE out[32];
        if (!hash || !BCRYPT_SUCCESS(BCryptFinishHash(hash, out, sizeof(out), 0))) return {};
        static const wchar_t* digits = L"0123456789abcdef";
        std::wstring hex;
        for (BYTE b : out) {
            hex += digits[b >> 4];
            hex += digits[b & 15];
        }
        return hex;
    }
};

std::wstring MsiProperty(MSIHANDLE db, const wchar_t* name) {
    std::wstring sql = std::wstring(L"SELECT `Value` FROM `Property` WHERE `Property`='") + name + L"'";
    MSIHANDLE view = 0, record = 0;
    std::wstring value;
    if (MsiDatabaseOpenViewW(db, sql.c_str(), &view) == ERROR_SUCCESS && MsiViewExecute(view, 0) == ERROR_SUCCESS &&
        MsiViewFetch(view, &record) == ERROR_SUCCESS) {
        wchar_t buf[256];
        DWORD len = 256;
        if (MsiRecordGetStringW(record, 1, buf, &len) == ERROR_SUCCESS) value = buf;
    }
    if (record) MsiCloseHandle(record);
    if (view) MsiCloseHandle(view);
    return value;
}

// The package must be openFences (our UpgradeCode) and exactly the version the release announced.
bool VerifyMsi(const std::filesystem::path& file, const update::Version& expected, std::wstring& error) {
    MSIHANDLE db = 0;
    if (MsiOpenDatabaseW(file.c_str(), MSIDBOPEN_READONLY, &db) != ERROR_SUCCESS) {
        error = L"the downloaded file isn't a Windows Installer package";
        return false;
    }
    std::wstring upgradeCode = MsiProperty(db, L"UpgradeCode");
    std::wstring productVersion = MsiProperty(db, L"ProductVersion");
    MsiCloseHandle(db);
    if (!EqualsI(upgradeCode, kUpgradeCode)) {
        error = L"the downloaded package isn't an openFences installer";
        return false;
    }
    // ProductVersion may carry a fourth field ("1.1.0.0").
    size_t third = productVersion.find(L'.', productVersion.find(L'.', productVersion.find(L'.') + 1) + 1);
    auto version = update::ParseVersion(productVersion.substr(0, third));
    if (!version || *version != expected) {
        error = L"the package's version (" + productVersion + L") doesn't match the release";
        return false;
    }
    return true;
}

// Signed packages must verify. Releases aren't signed yet, so a package without any signature is accepted (the SHA-256
// digest published with the release still has to match).
bool VerifySignature(const std::filesystem::path& file, std::wstring& error) {
    WINTRUST_FILE_INFO fi{sizeof(fi)};
    fi.pcwszFilePath = file.c_str();
    WINTRUST_DATA wd{sizeof(wd)};
    wd.dwUIChoice = WTD_UI_NONE;
    wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_FILE;
    wd.pFile = &fi;
    wd.dwStateAction = WTD_STATEACTION_VERIFY;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG result = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &wd);
    wd.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &wd);
    if (result == ERROR_SUCCESS) {
        log::Info(L"updater: installer signature verified");
        return true;
    }
    if (result == TRUST_E_NOSIGNATURE || result == TRUST_E_SUBJECT_FORM_UNKNOWN) {
        log::Info(L"updater: installer is not signed (accepted; its SHA-256 digest matched)");
        return true;
    }
    error = L"the installer's digital signature is invalid (0x" + std::format(L"{:08x}", static_cast<unsigned>(result)) + L")";
    return false;
}

// ------------------------------------------------------------------ the check

std::atomic<bool> g_stop{false};

std::filesystem::path UpdatesDir() { return paths::ExePath().parent_path() / L"Updates"; }

bool PrepareUpdatesDir(std::wstring& error) {
    auto dir = UpdatesDir();
    std::error_code ec;
    // The install folder is locked down by the MSI (administrators and SYSTEM only), and the folder inherits that.
    // Refuse a junction or symbolic link planted in its place.
    DWORD attrs = GetFileAttributesW(dir.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
        error = L"the Updates folder is a link; not using it";
        return false;
    }
    std::filesystem::create_directories(dir, ec);
    if (!std::filesystem::is_directory(dir, ec)) {
        error = L"couldn't create " + dir.wstring();
        return false;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
        if (EqualsI(entry.path().extension().wstring(), L".msi")) std::filesystem::remove(entry.path(), ec);
    return true;
}

bool LaunchInstaller(const std::filesystem::path& msi, std::wstring& error) {
    wchar_t sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring exe = std::wstring(sys) + L"\\msiexec.exe";
    std::wstring log = (UpdatesDir() / L"install.log").wstring();
    // Running copies have already closed themselves (see Install). MSIRMSHUTDOWN=1 lets Restart Manager close anything
    // left in this session; /norestart: never reboot the PC.
    std::wstring cmd = L"\"" + exe + L"\" /i \"" + msi.wstring() + L"\" /qn /norestart MSIRMSHUTDOWN=1 /l*v \"" + log + L"\"";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, sys, &si, &pi)) {
        error = L"couldn't start Windows Installer (error " + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void Install(const update::Release& rel, const std::wstring& feed, Status& st) {
    const std::wstring version = update::FormatVersion(rel.version);
    auto fail = [&](const std::wstring& why) {
        log::Info(L"updater: not installing {}: {}", version, why);
        st.code = ResultCode::Failed;
        st.message = L"Couldn't install openFences " + version + L": " + why + L".";
    };
    std::wstring error;
    if (rel.msi.sha256.empty()) return fail(L"the release doesn't publish a checksum for its installer");
    if (!PrepareUpdatesDir(error)) return fail(error);
    auto file = UpdatesDir() / rel.msi.name;  // the name was matched against "openFences-<version>-x64.msi"
    {
        HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return fail(L"couldn't save the download");
        Sha256 sha;
        bool ok = HttpGet(rel.msi.url, feed, L"application/octet-stream", 200ull * 1024 * 1024,
                          [&](const BYTE* data, DWORD size) {
                              DWORD written = 0;
                              return !g_stop && sha.Add(data, size) && WriteFile(h, data, size, &written, nullptr) && written == size;
                          },
                          error);
        CloseHandle(h);
        std::error_code ec;
        if (!ok) {
            std::filesystem::remove(file, ec);
            return fail(L"the download failed (" + error + L")");
        }
        std::wstring digest = sha.HexDigest();
        if (digest != rel.msi.sha256) {
            std::filesystem::remove(file, ec);
            log::Info(L"updater: checksum mismatch: got {}, expected {}", digest, rel.msi.sha256);
            return fail(L"the download didn't match its published checksum");
        }
    }
    if (!VerifyMsi(file, rel.version, error) || !VerifySignature(file, error)) {
        std::error_code ec;
        std::filesystem::remove(file, ec);
        return fail(error);
    }
    // Announce first: every running openFences (in any session) watches this status, closes itself and arranges to be
    // started again once the upgrade is done. Restart Manager can't do that from here: msiexec runs in the service's
    // session 0 and only closes applications there. Give them a few seconds before files are replaced.
    st.code = ResultCode::Installing;
    st.message = L"Installing openFences " + version + L"… openFences closes briefly and restarts by itself.";
    WriteStatus(st);
    for (int i = 0; i < 16 && !g_stop; ++i) Sleep(500);
    if (!LaunchInstaller(file, error)) return fail(error);
    log::Info(L"updater: installing {} from {}", version, file.wstring());
}

void RunCheck(const std::wstring& mode) {
    Policy policy = ReadPolicy();
    if (policy.disableUpdates) {
        log::Info(L"updater: updates are turned off by policy");
        return;
    }
    Settings settings = ReadSettings();
    if (mode == L"check" && !settings.enabled) {
        log::Info(L"updater: automatic updates are off");
        return;
    }
    Status st = ReadStatus();
    st.lastCheckUnix = NowUnix();
    st.daysLeft = 0;
    const std::wstring feed = FeedUrl(policy);
    if (!update::IsAcceptableFeedUrl(feed)) {
        st.code = ResultCode::Failed;
        st.message = L"The update address set by your administrator isn't allowed (it must use https).";
        WriteStatus(st);
        return;
    }
    std::wstring error;
    auto rel = FetchLatest(feed, error);
    if (!rel) {
        log::Info(L"updater: check failed: {}", error);
        st.code = ResultCode::Failed;
        st.message = L"Couldn't check for updates: " + error + L".";
        WriteStatus(st);
        return;
    }
    auto current = update::ParseVersion(OF_VERSION_WSTRING).value_or(update::Version{});
    st.latestVersion = update::FormatVersion(rel->version);
    auto d = update::Decide(current, *rel, st.lastCheckUnix, settings.installImmediately || mode == L"install-now");
    log::Info(L"updater: mode {}, installed {}, latest {}, decision {}", mode, OF_VERSION_WSTRING, st.latestVersion, static_cast<int>(d.decision));
    switch (d.decision) {
        case update::Decision::UpToDate:
            st.code = ResultCode::UpToDate;
            st.message = L"openFences is up to date (latest release: " + st.latestVersion + L").";
            break;
        case update::Decision::TooNew:
            st.code = ResultCode::TooNew;
            st.daysLeft = d.daysUntilEligible;
            st.message = L"openFences " + st.latestVersion + L" is available. It's installed automatically in " + std::to_wstring(d.daysUntilEligible) +
                         (d.daysUntilEligible == 1 ? L" day" : L" days") + L", once it has been out for " +
                         std::to_wstring(update::kMinReleaseAgeDays) + L" days.";
            break;
        case update::Decision::Install:
            Install(*rel, feed, st);
            break;
    }
    WriteStatus(st);
}

// ------------------------------------------------------------------ service plumbing

SERVICE_STATUS_HANDLE g_statusHandle = nullptr;

void Report(DWORD state) {
    SERVICE_STATUS s{};
    s.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    s.dwCurrentState = state;
    s.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    s.dwWaitHint = state == SERVICE_RUNNING || state == SERVICE_STOPPED ? 0 : 10000;
    SetServiceStatus(g_statusHandle, &s);
}

DWORD WINAPI ControlHandler(DWORD control, DWORD, LPVOID, LPVOID) {
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        g_stop = true;  // aborts a running download
        Report(SERVICE_STOP_PENDING);
        return NO_ERROR;
    }
    return control == SERVICE_CONTROL_INTERROGATE ? NO_ERROR : ERROR_CALL_NOT_IMPLEMENTED;
}

void WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
    g_statusHandle = RegisterServiceCtrlHandlerExW(kServiceName, ControlHandler, nullptr);
    if (!g_statusHandle) return;
    Report(SERVICE_RUNNING);
    std::error_code ec;
    std::filesystem::create_directories(UpdatesDir(), ec);
    log::InitFile(UpdatesDir() / L"updater.log", false);
    // Only the fixed modes; anything else (the service may be started by any user) is a plain check.
    std::wstring mode = argc > 1 && argv[1] ? argv[1] : L"check";
    if (mode != L"check-now" && mode != L"install-now") mode = L"check";
    RunCheck(mode);
    Report(SERVICE_STOPPED);
}

}  // namespace

// ------------------------------------------------------------------ public

Settings ReadSettings() {
    Settings s;
    s.enabled = GetDword(L"Enabled").value_or(GetDword(L"DefaultAutoUpdate").value_or(0)) != 0;
    s.installImmediately = GetDword(L"InstallImmediately").value_or(0) != 0;
    s.intervalDays = update::NormalizeInterval(static_cast<int>(GetDword(L"CheckIntervalDays").value_or(update::kDefaultCheckIntervalDays)));
    return s;
}

bool WriteSettings(const Settings& s) {
    Key key;
    if (!OpenForWrite(key)) return false;
    return SetDword(key.h, L"Enabled", s.enabled) && SetDword(key.h, L"InstallImmediately", s.installImmediately) &&
           SetDword(key.h, L"CheckIntervalDays", static_cast<DWORD>(update::NormalizeInterval(s.intervalDays)));
}

Status ReadStatus() {
    Status s;
    s.lastCheckUnix = static_cast<int64_t>(GetQword(L"LastCheck").value_or(0));
    s.code = static_cast<ResultCode>(GetDword(L"ResultCode").value_or(0));
    s.message = GetString(L"Result");
    s.latestVersion = GetString(L"LatestVersion");
    s.daysLeft = static_cast<int>(GetDword(L"DaysLeft").value_or(0));
    return s;
}

bool WriteStatus(const Status& s) {
    Key key;
    if (!OpenForWrite(key)) return false;
    ULONGLONG last = static_cast<ULONGLONG>(s.lastCheckUnix);
    bool ok = RegSetValueExW(key.h, L"LastCheck", 0, REG_QWORD, reinterpret_cast<const BYTE*>(&last), sizeof(last)) == ERROR_SUCCESS;
    ok = SetDword(key.h, L"ResultCode", static_cast<DWORD>(s.code)) && ok;
    ok = SetString(key.h, L"Result", s.message) && ok;
    ok = SetString(key.h, L"LatestVersion", s.latestVersion) && ok;
    return SetDword(key.h, L"DaysLeft", static_cast<DWORD>(s.daysLeft)) && ok;
}

int64_t NowUnix() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u{{ft.dwLowDateTime, ft.dwHighDateTime}};
    return static_cast<int64_t>((u.QuadPart - 116444736000000000ull) / 10000000ull);
}

std::wstring FeedUrl(const Policy& policy) { return policy.updateFeedUrl.empty() ? std::wstring(update::kDefaultFeedUrl) : policy.updateFeedUrl; }

bool ServiceUsable() {
    Sc scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm.h) return false;
    Sc svc(OpenServiceW(scm.h, kServiceName, SERVICE_QUERY_CONFIG));
    if (!svc.h) return false;
    DWORD needed = 0;
    (void)QueryServiceConfigW(svc.h, nullptr, 0, &needed);  // size probe: fails with ERROR_INSUFFICIENT_BUFFER
    if (needed == 0) return false;
    std::vector<BYTE> buf(needed);
    auto* cfg = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buf.data());
    if (!QueryServiceConfigW(svc.h, cfg, needed, &needed) || !cfg->lpBinaryPathName) return false;
    return ContainsI(cfg->lpBinaryPathName, paths::ExePath().wstring());
}

bool ServiceRunning() {
    Sc scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    Sc svc(scm.h ? OpenServiceW(scm.h, kServiceName, SERVICE_QUERY_STATUS) : nullptr);
    SERVICE_STATUS s{};
    return svc.h && QueryServiceStatus(svc.h, &s) && s.dwCurrentState != SERVICE_STOPPED;
}

bool StartService(const wchar_t* mode) {
    Sc scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    Sc svc(scm.h ? OpenServiceW(scm.h, kServiceName, SERVICE_START | SERVICE_QUERY_STATUS) : nullptr);
    if (!svc.h) {
        log::Info(L"Update service unavailable (error {})", GetLastError());
        return false;
    }
    LPCWSTR args[] = {mode};
    if (::StartServiceW(svc.h, 1, args) || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) return true;
    log::Info(L"Starting the update service failed (error {})", GetLastError());
    return false;
}

std::optional<update::Release> FetchLatest(const std::wstring& feedUrl, std::wstring& error) {
    if (!update::IsAcceptableFeedUrl(feedUrl)) {
        error = L"the update address isn't allowed";
        return std::nullopt;
    }
    std::string body;
    bool ok = HttpGet(feedUrl, feedUrl, L"application/vnd.github+json", 1024 * 1024,
                      [&](const BYTE* data, DWORD size) {
                          body.append(reinterpret_cast<const char*>(data), size);
                          return true;
                      },
                      error);
    if (!ok) return std::nullopt;
    auto rel = update::ParseLatestRelease(body);
    if (!rel) error = L"the update feed didn't describe a usable release";
    return rel;
}

int RunService() {
    SERVICE_TABLE_ENTRYW table[] = {{const_cast<LPWSTR>(kServiceName), ServiceMain}, {nullptr, nullptr}};
    // Fails when not started by the service control manager; nothing else to do then.
    return StartServiceCtrlDispatcherW(table) ? 0 : 1;
}

namespace {
constexpr wchar_t kRelauncherPrefix[] = L"openFences-update-";

std::filesystem::path TempDir() {
    wchar_t buf[MAX_PATH + 1];
    DWORD n = GetTempPathW(MAX_PATH + 1, buf);
    return n ? std::filesystem::path(buf) : std::filesystem::temp_directory_path();
}

std::optional<update::Version> FileVersion(const std::wstring& path) {
    DWORD handle = 0, size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (!size) return std::nullopt;
    std::vector<BYTE> data(size);
    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()) || !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &len) ||
        !info)
        return std::nullopt;
    return update::Version{static_cast<int>(HIWORD(info->dwFileVersionMS)), static_cast<int>(LOWORD(info->dwFileVersionMS)),
                           static_cast<int>(HIWORD(info->dwFileVersionLS))};
}

// Windows Installer holds this mutex while an installation runs.
bool InstallerBusy() {
    HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, L"Global\\_MSIExecute");
    if (!h) return false;
    CloseHandle(h);
    return true;
}
}  // namespace

bool StartRelauncher(const std::wstring& targetVersion) {
    auto copy = TempDir() / (std::wstring(kRelauncherPrefix) + std::to_wstring(GetCurrentProcessId()) + L".exe");
    if (!CopyFileW(paths::ExePath().c_str(), copy.c_str(), FALSE)) {
        log::Info(L"Updates: couldn't create the relaunch helper (error {})", GetLastError());
        return false;
    }
    std::wstring cmd = L"\"" + copy.wstring() + L"\" --relaunch-after-update \"" + paths::ExePath().wstring() + L"\" " + targetVersion;
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(copy.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr,
                        TempDir().c_str(), &si, &pi)) {
        log::Info(L"Updates: couldn't start the relaunch helper (error {})", GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

int RunRelauncher(const std::wstring& installedExe, const std::wstring& targetVersion) {
    log::Init(false);
    auto target = update::ParseVersion(targetVersion);
    const ULONGLONG start = GetTickCount64();
    ULONGLONG idleSince = start;
    bool seenBusy = false;
    for (;;) {
        Sleep(1000);
        ULONGLONG now = GetTickCount64();
        if (InstallerBusy()) {
            seenBusy = true;
            idleSince = now;
            continue;
        }
        auto version = FileVersion(installedExe);
        if (target && version && *version == *target) {
            Sleep(2000);  // let Windows Installer finish its last steps
            break;
        }
        // The installation finished without the new version (it failed), or never started: bring the old one back.
        if (now - idleSince > (seenBusy ? 15000u : 90000u) || now - start > 15 * 60 * 1000) break;
    }
    log::Info(L"Updates: relaunching {} (target {})", installedExe, targetVersion);
    std::wstring cmd = L"\"" + installedExe + L"\" --autostart";
    std::wstring dir = std::filesystem::path(installedExe).parent_path().wstring();
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(installedExe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return 0;
}

void CleanupRelauncherCopies() {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(TempDir(), ec)) {
        std::wstring name = entry.path().filename().wstring();
        if (StartsWithI(name, kRelauncherPrefix) && EqualsI(entry.path().extension().wstring(), L".exe"))
            std::filesystem::remove(entry.path(), ec);  // fails harmlessly while a helper still runs
    }
}

}  // namespace of::updater
