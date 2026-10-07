// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/tools_tab.hpp"

#include <fmt/format.h>

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/diagnostics.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

namespace
{
const char* LANGUAGES[] = { "system", "en-US", "fr" };
const char* THEMES[]    = { "system", "light", "dark" };

template <size_t N>
int index_of(const char* const (&list)[N], const std::string& value)
{
    for (size_t i = 0; i < N; i++)
        if (value == list[i]) return (int)i;
    return 0;
}
}   // namespace

ToolsTab::ToolsTab()
    : TabBase("xml/tab/tools.xml")
{
    export_note->setSingleLine(false);
    export_note->setText(brls::getStr("nx_pctl/tools/export_note", paths::logs_dir()));

    export_cell->registerClickAction([](brls::View*) {
        std::string err;
        std::string path = diagnostic::save(diagnostic::current_report(), &err);
        if (path.empty()) ui::notify("nx_pctl/toast/diag_err"_i18n + ": " + err);
        else ui::notify(brls::getStr("nx_pctl/toast/diag_saved", path));
        return true;
    });

    language->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (const char* l : LANGUAGES) labels.push_back(brls::getStr(std::string("nx_pctl/tools/languages/") + l));
        ui::pick("nx_pctl/tools/language"_i18n, labels, index_of(LANGUAGES, config::get().language), [this](int i) {
            config::get().language = LANGUAGES[i];
            config::save();
            ui::notify("nx_pctl/common/restart_needed"_i18n);
            this->refresh();
        });
        return true;
    });

    theme->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (const char* t : THEMES) labels.push_back(brls::getStr(std::string("nx_pctl/tools/themes/") + t));
        ui::pick("nx_pctl/tools/theme"_i18n, labels, index_of(THEMES, config::get().theme), [this](int i) {
            config::get().theme = THEMES[i];
            config::save();
            ui::notify("nx_pctl/common/restart_needed"_i18n);
            this->refresh();
        });
        return true;
    });

    advanced->init("nx_pctl/tools/advanced"_i18n, config::get().advanced, [](bool on) {
        config::get().advanced = on;
        config::save();
    });
    ui::set_visible(advanced.getView(), !app::read_only_build());
}

void ToolsTab::refresh()
{
    const auto& cfg = config::get();
    language->setDetailText(brls::getStr("nx_pctl/tools/languages/" + std::string(LANGUAGES[index_of(LANGUAGES, cfg.language)])));
    theme->setDetailText(brls::getStr("nx_pctl/tools/themes/" + std::string(THEMES[index_of(THEMES, cfg.theme)])));
    advanced->setOn(cfg.advanced, false);

    SysInfo si;
    sysinfo_get(&si);
    char fwv[16];
    sysinfo_version_string(si.hos_version, fwv, sizeof(fwv));
    std::string flags;
    if (app::probe_build()) flags += " · PROBE";
    if (app::read_only_build()) flags += " · READ_ONLY";
    version->setDetailText(app::version() + flags);
    fw->setDetailText(fwv);
    ams->setDetailText(si.ams_valid ? fmt::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro)
                                    : "nx_pctl/common/unavailable"_i18n);
    NVGcolor c = ui::color_neutral();
    compat->setDetailText(ui::compat_text(si, &c));
    compat->setDetailTextColor(c);
    mode->setDetailText(si.applet_mode ? "nx_pctl/tools/mode_applet"_i18n : "nx_pctl/tools/mode_app"_i18n);
    data->setDetailText(paths::data_dir());
    license->setDetailText("nx_pctl/tools/license_value"_i18n);
    source->setDetailText(app::repo_url());
    credits->setDetailText("nx_pctl/tools/credits_value"_i18n);
}

brls::View* ToolsTab::create()
{
    return new ToolsTab();
}
