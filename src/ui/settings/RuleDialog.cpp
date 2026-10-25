#include "ui/settings/RuleDialog.h"

#include <commctrl.h>
#include <shellscalingapi.h>

#include <regex>

#include "core/Strings.h"
#include "shell/FileOps.h"
#include "ui/settings/Form.h"

namespace of {

namespace {

enum : int {
    kName = 10,
    kEnabled,
    kMode,
    kExt,
    kGlob,
    kRegex,
    kTarget,
    kTargetBrowse,
    kKind,
    kModOlder,
    kModNewer,
    kCreOlder,
    kCreNewer,
    kAction,
    kFence,
    kFolder,
    kFolderBrowse,
};

struct State {
    bool done = false;
    bool ok = false;
    HWND hwnd = nullptr;
    Rule* rule = nullptr;
    const std::vector<std::pair<std::string, std::wstring>>* tabs = nullptr;
};

std::wstring Text(HWND parent, int id) {
    HWND h = GetDlgItem(parent, id);
    int len = GetWindowTextLengthW(h);
    std::wstring s(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    s.resize(static_cast<size_t>(len));
    return Trim(s);
}

std::optional<int> Days(HWND parent, int id) {
    std::wstring t = Text(parent, id);
    if (t.empty()) return std::nullopt;
    int v = _wtoi(t.c_str());
    if (v < 0) return std::nullopt;
    return v;
}

std::wstring DaysText(const std::optional<int>& v) { return v ? std::to_wstring(*v) : L""; }

void UpdateEnabled(HWND hwnd) {
    bool move = SendDlgItemMessageW(hwnd, kAction, CB_GETCURSEL, 0, 0) == 1;
    EnableWindow(GetDlgItem(hwnd, kFence), !move);
    EnableWindow(GetDlgItem(hwnd, kFolder), move);
    EnableWindow(GetDlgItem(hwnd, kFolderBrowse), move);
}

bool Commit(State& st) {
    HWND h = st.hwnd;
    Rule r = *st.rule;
    r.name = Text(h, kName);
    if (r.name.empty()) r.name = L"Rule";
    r.enabled = IsDlgButtonChecked(h, kEnabled) == BST_CHECKED;
    r.mode = SendDlgItemMessageW(h, kMode, CB_GETCURSEL, 0, 0) == 1 ? MatchMode::Any : MatchMode::All;
    r.match.extensions.clear();
    for (auto e : SplitList(Text(h, kExt))) {
        e = ToLower(e);
        if (e.rfind(L"*.", 0) == 0) e = e.substr(1);
        if (!e.empty() && e[0] != L'.') e.insert(e.begin(), L'.');
        r.match.extensions.push_back(e);
    }
    r.match.nameGlob = Text(h, kGlob);
    r.match.nameRegex = Text(h, kRegex);
    if (!r.match.nameRegex.empty()) {
        try {
            std::wregex test(r.match.nameRegex, std::regex::ECMAScript | std::regex::icase);
        } catch (const std::regex_error&) {
            MessageBoxW(h, L"The name pattern is not a valid regular expression.", L"Rule", MB_ICONWARNING);
            return false;
        }
    }
    r.match.targetPrefix = Text(h, kTarget);
    r.match.kind = static_cast<ItemKindFilter>(std::clamp(static_cast<int>(SendDlgItemMessageW(h, kKind, CB_GETCURSEL, 0, 0)), 0, 3));
    r.match.modifiedOlderThanDays = Days(h, kModOlder);
    r.match.modifiedNewerThanDays = Days(h, kModNewer);
    r.match.createdOlderThanDays = Days(h, kCreOlder);
    r.match.createdNewerThanDays = Days(h, kCreNewer);
    r.action = SendDlgItemMessageW(h, kAction, CB_GETCURSEL, 0, 0) == 1 ? RuleAction::MoveToFolder : RuleAction::AssignToTab;
    int fence = static_cast<int>(SendDlgItemMessageW(h, kFence, CB_GETCURSEL, 0, 0));
    r.targetTab = (fence >= 0 && fence < static_cast<int>(st.tabs->size())) ? (*st.tabs)[static_cast<size_t>(fence)].first : std::string();
    r.targetFolder = Text(h, kFolder);
    if (r.action == RuleAction::AssignToTab && r.targetTab.empty()) {
        MessageBoxW(h, L"Choose the fence that should receive matching items.", L"Rule", MB_ICONWARNING);
        return false;
    }
    if (r.action == RuleAction::MoveToFolder && r.targetFolder.empty()) {
        MessageBoxW(h, L"Choose the folder matching files are moved to.", L"Rule", MB_ICONWARNING);
        return false;
    }
    *st.rule = r;
    return true;
}

LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_COMMAND:
            if (!st) break;
            switch (LOWORD(wp)) {
                case IDOK:
                    if (Commit(*st)) {
                        st->ok = true;
                        st->done = true;
                    }
                    return 0;
                case IDCANCEL:
                    st->done = true;
                    return 0;
                case kAction:
                    if (HIWORD(wp) == CBN_SELCHANGE) UpdateEnabled(hwnd);
                    return 0;
                case kTargetBrowse: {
                    std::wstring p = PickFolder(hwnd, L"Shortcuts pointing inside this folder match");
                    if (!p.empty()) SetDlgItemTextW(hwnd, kTarget, p.c_str());
                    return 0;
                }
                case kFolderBrowse: {
                    std::wstring p = PickFolder(hwnd, L"Move matching files to this folder");
                    if (!p.empty()) SetDlgItemTextW(hwnd, kFolder, p.c_str());
                    return 0;
                }
            }
            break;
        case WM_CTLCOLORSTATIC:
            SetBkColor(reinterpret_cast<HDC>(wp), GetSysColor(COLOR_WINDOW));
            if (GetDlgCtrlID(reinterpret_cast<HWND>(lp)) == 0xFFFE) SetTextColor(reinterpret_cast<HDC>(wp), GetSysColor(COLOR_GRAYTEXT));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        case WM_CTLCOLORBTN:
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        case WM_CLOSE:
            if (st) st->done = true;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool EditRule(HWND owner, Rule& rule, const std::vector<std::pair<std::string, std::wstring>>& tabs) {
    static bool registered = false;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = Proc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        wc.lpszClassName = L"openFences.RuleDialog";
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(101));
        RegisterClassExW(&wc);
        registered = true;
    }
    UINT dpi = owner ? GetDpiForWindow(owner) : 96;
    auto px = [&](int v) { return MulDiv(v, static_cast<int>(dpi), 96); };
    RECT ownerRect{0, 0, 800, 600};
    if (owner) GetWindowRect(owner, &ownerRect);
    int w = px(560), h = px(700);
    int x = (ownerRect.left + ownerRect.right - w) / 2, y = std::max<LONG>(0, (ownerRect.top + ownerRect.bottom - h) / 2);

    State st;
    st.rule = &rule;
    st.tabs = &tabs;
    st.hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"openFences.RuleDialog", L"Sorting rule", WS_POPUP | WS_CAPTION | WS_SYSMENU,
                              x, y, w, h, owner, nullptr, inst, nullptr);
    SetWindowLongPtrW(st.hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
    HFONT font = CreateUiFont(dpi, 9, false);
    HFONT head = CreateUiFont(dpi, 11, true);
    RECT client;
    GetClientRect(st.hwnd, &client);
    int margin = px(20);
    int width = client.right - 2 * margin;

    Form f;
    f.Reset(st.hwnd, font, head, dpi, width, margin);
    const RuleCriteria& m = rule.match;
    f.Edit(kName, L"Name", rule.name);
    f.Check(kEnabled, L"Enabled", rule.enabled);
    f.Heading(L"When an item on the desktop…");
    f.Combo(kMode, L"Match", {L"all of the conditions below", L"any of the conditions below"}, rule.mode == MatchMode::Any ? 1 : 0);
    f.Edit(kExt, L"File types (e.g. .png .jpg)", JoinList(m.extensions, L" "));
    f.Edit(kGlob, L"Name is like (* and ?)", m.nameGlob);
    f.Edit(kRegex, L"Name matches (regex)", m.nameRegex);
    f.Edit(kTarget, L"Shortcut points into", m.targetPrefix);
    f.Buttons({{kTargetBrowse, L"Browse…"}});
    f.Combo(kKind, L"Item is", {L"anything", L"a file", L"a folder", L"a shortcut"}, static_cast<int>(m.kind));
    f.Edit(kModOlder, L"Modified more than N days ago", DaysText(m.modifiedOlderThanDays));
    f.Edit(kModNewer, L"Modified within N days", DaysText(m.modifiedNewerThanDays));
    f.Edit(kCreOlder, L"Created more than N days ago", DaysText(m.createdOlderThanDays));
    f.Edit(kCreNewer, L"Created within N days", DaysText(m.createdNewerThanDays));
    f.Heading(L"…then");
    f.Combo(kAction, L"Action", {L"Put it in a fence", L"Move the file to a folder (keep it off the desktop)"},
            rule.action == RuleAction::MoveToFolder ? 1 : 0);
    std::vector<std::wstring> names;
    int fenceSel = -1;
    for (size_t i = 0; i < tabs.size(); ++i) {
        names.push_back(tabs[i].second);
        if (tabs[i].first == rule.targetTab) fenceSel = static_cast<int>(i);
    }
    f.Combo(kFence, L"Fence", names, fenceSel);
    f.Edit(kFolder, L"Folder", rule.targetFolder);
    f.Buttons({{kFolderBrowse, L"Browse…"}});
    // Shift all controls right by the margin (Form lays out from x = 0).
    for (HWND c = GetWindow(st.hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        RECT cr;
        GetWindowRect(c, &cr);
        MapWindowPoints(nullptr, st.hwnd, reinterpret_cast<POINT*>(&cr), 2);
        SetWindowPos(c, nullptr, cr.left + margin, cr.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    int buttonsY = f.Height() + px(6);
    HWND ok = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, client.right - margin - px(196), buttonsY,
                              px(90), px(28), st.hwnd, reinterpret_cast<HMENU>(IDOK), inst, nullptr);
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, client.right - margin - px(98), buttonsY, px(90), px(28),
                                  st.hwnd, reinterpret_cast<HMENU>(IDCANCEL), inst, nullptr);
    SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    // Fit the window to its content.
    RECT wr{0, 0, client.right, buttonsY + px(28) + margin};
    AdjustWindowRectExForDpi(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME, dpi);
    SetWindowPos(st.hwnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top, SWP_NOMOVE | SWP_NOZORDER);
    UpdateEnabled(st.hwnd);

    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(st.hwnd, SW_SHOW);
    SetFocus(GetDlgItem(st.hwnd, kName));
    RunModalLoop(st.hwnd, st.done);
    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    DestroyWindow(st.hwnd);
    DeleteObject(font);
    DeleteObject(head);
    return st.ok;
}

}  // namespace of
