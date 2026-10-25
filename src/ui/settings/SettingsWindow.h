#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "core/Snapshots.h"
#include "core/Templates.h"
#include "ui/settings/Form.h"

namespace of {

// "Configure openFences": a sidebar of sections (as in Fences 6) and a scrollable page of controls.
// Every change applies immediately.
class SettingsWindow {
public:
    SettingsWindow() = default;
    ~SettingsWindow();
    void Show(const std::wstring& page);
    HWND Hwnd() const { return m_hwnd; }
    bool TranslateDialogMessage(MSG* msg);

private:
    struct PageDef {
        const wchar_t* key;
        const wchar_t* title;
        void (SettingsWindow::*build)();
        void (SettingsWindow::*command)(int id, int code);
    };
    static const std::vector<PageDef>& Pages();

    static LRESULT CALLBACK FrameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnFrameMessage(UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnPanelMessage(UINT msg, WPARAM wp, LPARAM lp);
    void Create();
    void CreateFonts();
    void LayoutChildren();
    void SelectPage(int index);
    void RebuildPage();
    void ScrollTo(int pos);
    void OnSlider(HWND slider);
    void OnDrawItem(DRAWITEMSTRUCT* dis);
    LRESULT OnNotify(NMHDR* nm);
    void Changed();  // applies settings and saves
    bool Checked(int id) const;
    int ComboIndex(int id) const;
    int SliderPos(int id) const;
    std::wstring TextOf(int id) const;

    // Pages (SettingsPages.cpp).
    void BuildWelcome();
    void OnWelcome(int id, int code);
    void BuildChameleon();
    void OnChameleon(int id, int code);
    void BuildPersonalization();
    void OnPersonalization(int id, int code);
    void BuildPortals();
    void OnPortals(int id, int code);
    void BuildPeek();
    void OnPeek(int id, int code);
    void BuildHide();
    void OnHide(int id, int code);
    void BuildRollup();
    void OnRollup(int id, int code);
    void BuildTabs();
    void OnTabs(int id, int code);
    void BuildPages();
    void OnPages(int id, int code);
    void BuildRules();
    void OnRules(int id, int code);
    void BuildLayout();
    void OnLayout(int id, int code);
    void BuildBusiness();
    void OnBusiness(int id, int code);
    void BuildAbout();
    void OnAbout(int id, int code);
    void FillRules();
    void FillSnapshots();
    void FillPages();
    void DrawSnapshotPreview(DRAWITEMSTRUCT* dis);
    void OnSliderChanged(int id, int pos);

    HWND m_hwnd = nullptr;
    HWND m_sidebar = nullptr;
    HWND m_panel = nullptr;
    HFONT m_font = nullptr;
    HFONT m_headFont = nullptr;
    HFONT m_navFont = nullptr;
    UINT m_dpi = 96;
    int m_page = 0;
    int m_scroll = 0;
    int m_contentHeight = 0;
    bool m_building = false;
    Form m_form;
    std::vector<SnapshotInfo> m_snapshots;
    std::vector<Template> m_templates;
    std::vector<std::pair<std::string, std::wstring>> m_tabChoices;
};

}  // namespace of
