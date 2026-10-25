#include "core/Templates.h"

#include <algorithm>
#include <nlohmann/json.hpp>

#include "core/Config.h"
#include "core/Rules.h"
#include "core/Strings.h"

namespace of {

std::optional<Template> ParseTemplate(std::string_view text) {
    auto j = nlohmann::json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    auto cfg = ParseConfig(text);
    if (!cfg) return std::nullopt;
    Template t;
    auto header = j.value("template", nlohmann::json::object());
    t.name = FromUtf8(header.value("name", std::string("Template")));
    t.description = FromUtf8(header.value("description", std::string()));
    if (!cfg->layouts.empty()) t.layout = cfg->layouts.begin()->second;
    EnsurePage(t.layout);
    t.rules = cfg->rules;
    if (j.contains("lock")) t.lock = cfg->lock;
    if (j.contains("appearance")) t.appearance = cfg->appearance;
    return t;
}

std::optional<Template> LoadTemplate(const std::filesystem::path& path) {
    auto text = ReadFileText(path);
    if (!text) return std::nullopt;
    auto t = ParseTemplate(*text);
    if (t) t->path = path;
    return t;
}

std::vector<Template> ListTemplates(const std::filesystem::path& dir) {
    std::vector<Template> out;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (e.path().extension() != L".json") continue;
        if (auto t = LoadTemplate(e.path())) out.push_back(std::move(*t));
    }
    std::sort(out.begin(), out.end(), [](const Template& a, const Template& b) { return a.name < b.name; });
    return out;
}

void ApplyTemplate(Config& config, const std::string& layoutKey, const Template& tpl) {
    Layout& target = config.layouts[layoutKey];
    std::vector<std::wstring> monitors = target.monitors;
    target = tpl.layout;
    target.monitors = monitors;
    // Template groups target the primary monitor unless they name one explicitly.
    config.rules = tpl.rules;
    if (tpl.lock) config.lock = *tpl.lock;
    if (tpl.appearance) config.appearance = *tpl.appearance;
}

void BuildDefaultLayout(Config& config, const std::string& layoutKey, const std::wstring& primaryMonitor) {
    Layout& layout = config.layouts[layoutKey];
    Page& page = EnsurePage(layout);
    Group programs = MakeGroup(L"Programs", {0.015, 0.03, 0.2, 0.3}, primaryMonitor);
    Group folders = MakeGroup(L"Folders", {0.015, 0.36, 0.2, 0.26}, primaryMonitor);
    Group files = MakeGroup(L"Files and documents", {0.23, 0.03, 0.26, 0.3}, primaryMonitor);
    config.rules = DefaultRules(programs.tabs[0].id, folders.tabs[0].id, files.tabs[0].id);
    page.groups.push_back(std::move(programs));
    page.groups.push_back(std::move(folders));
    page.groups.push_back(std::move(files));
}

}  // namespace of
