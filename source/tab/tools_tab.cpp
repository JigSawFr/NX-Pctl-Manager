// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/tools_tab.hpp"

#include <fmt/format.h>

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/diagnostics.hpp"
#include "util/patches.hpp"
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
    credits->setSingleLine(false);
    serial_note->setSingleLine(false);
    patches_note->setSingleLine(false);

    // Ⓐ on the serial number shows / hides the masked digits.
    serial->registerAction("playguard/tools/serial_show"_i18n, brls::BUTTON_A, [this](brls::View*) {
        this->serial_revealed = !this->serial_revealed;
        this->refresh();
        return true;
    }, false, false, brls::SOUND_CLICK);
    export_note->setText(brls::getStr("playguard/tools/export_note", paths::logs_dir()));

    export_cell->registerClickAction([](brls::View*) {
        std::string err;
        std::string path = diagnostic::save(diagnostic::current_report(), &err);
        if (path.empty()) ui::notify("playguard/toast/diag_err"_i18n + ": " + err);
        else ui::notify(brls::getStr("playguard/toast/diag_saved", path));
        return true;
    });

    language->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (const char* l : LANGUAGES) labels.push_back(brls::getStr(std::string("playguard/tools/languages/") + l));
        ui::pick("playguard/tools/language"_i18n, labels, index_of(LANGUAGES, config::get().language), [this](int i) {
            const bool changed = config::get().language != LANGUAGES[i];
            config::get().language = LANGUAGES[i];
            config::save();
            this->refresh();
            if (changed) ui::offer_restart();
        });
        return true;
    });

    theme->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (const char* t : THEMES) labels.push_back(brls::getStr(std::string("playguard/tools/themes/") + t));
        ui::pick("playguard/tools/theme"_i18n, labels, index_of(THEMES, config::get().theme), [this](int i) {
            const bool changed = config::get().theme != THEMES[i];
            config::get().theme = THEMES[i];
            config::save();
            this->refresh();
            if (changed) ui::offer_restart();
        });
        return true;
    });

    advanced->init("playguard/tools/advanced"_i18n, config::get().advanced, [](bool on) {
        config::get().advanced = on;
        config::save();
    });
    auto_relock->init("playguard/tools/auto_relock"_i18n, config::get().auto_relock, [](bool on) {
        config::get().auto_relock = on;
        config::save();
    });
    ui::set_visible(advanced.getView(), !app::read_only_build());
    ui::set_visible(auto_relock.getView(), !app::read_only_build());
}

void ToolsTab::refresh()
{
    const auto& cfg = config::get();
    language->setDetailText(brls::getStr("playguard/tools/languages/" + std::string(LANGUAGES[index_of(LANGUAGES, cfg.language)])));
    theme->setDetailText(brls::getStr("playguard/tools/themes/" + std::string(THEMES[index_of(THEMES, cfg.theme)])));
    advanced->setOn(cfg.advanced, false);
    auto_relock->setOn(cfg.auto_relock, false);

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
                                    : "playguard/common/unavailable"_i18n);
    NVGcolor c = ui::color_neutral();
    compat->setDetailText(ui::compat_text(si, &c));
    compat->setDetailTextColor(c);

    storage->setDetailText(ui::storage_text(si));
    c = ui::color_neutral();
    blank->setDetailText(ui::blank_text(si, &c));
    blank->setDetailTextColor(c);
    const bool maskable = si.serial_valid && si.serial[0] && !(si.blank_valid && si.blank);
    serial->setDetailText(ui::serial_text(si, this->serial_revealed));
    serial->setDetailTextColor(ui::serial_warning(si) ? ui::color_warn() : ui::color_neutral());
    serial->setActionAvailable(brls::BUTTON_A, maskable);
    serial->updateActionHint(brls::BUTTON_A, this->serial_revealed ? "playguard/tools/serial_hide"_i18n
                                                                   : "playguard/tools/serial_show"_i18n);
    brls::Application::getGlobalHintsUpdateEvent()->fire();   // redraw the footer hints

    const patches::Report report = patches::detect(paths::sd_root(), fwv, si.emummc);
    c = ui::color_neutral();
    game_patches->setDetailText(ui::patches_text(report, &c));
    game_patches->setDetailTextColor(c);
    bool warn = false;
    const std::string note = ui::patches_note(report, si, &warn);
    patches_note->setText(note);
    patches_note->setTextColor(warn ? ui::color_warn() : ui::color_note());
    ui::set_visible_all({ { serial_note.getView(), ui::serial_warning(si) },
                          { patches_note.getView(), !note.empty() } });
    mode->setDetailText(si.applet_mode ? "playguard/tools/mode_applet"_i18n : "playguard/tools/mode_app"_i18n);
    data->setDetailText(paths::data_dir());
    license->setDetailText("playguard/tools/license_value"_i18n);
    source->setDetailText(app::repo_url());
    // A note rather than a cell: the credits are longer than a cell's value.
    credits->setText(brls::getStr("playguard/tools/credits_line", "playguard/tools/credits_value"_i18n));
}

brls::View* ToolsTab::create()
{
    return new ToolsTab();
}
