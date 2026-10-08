// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/preferences_tab.hpp"

#include <vector>

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

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

// "+15 min / +30 min / +1 h"
std::string amounts_text(const int (&set)[3])
{
    std::string text;
    for (int m : set) text += (text.empty() ? "+" : " / +") + ui::fmt_minutes((uint16_t)m);
    return text;
}

int extra_set_index(const std::vector<int>& amounts)
{
    for (size_t i = 0; i < sizeof(config::EXTRA_SETS) / sizeof(config::EXTRA_SETS[0]); i++)
        if (amounts == std::vector<int>(std::begin(config::EXTRA_SETS[i]), std::end(config::EXTRA_SETS[i]))) return (int)i;
    return 0;
}
}   // namespace

PreferencesTab::PreferencesTab()
    : TabBase("xml/tab/preferences.xml")
{
    note->setSingleLine(false);

    language->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (const char* l : LANGUAGES) labels.push_back(brls::getStr(std::string("playguard/tools/languages/") + l));
        ui::pick("playguard/tools/language"_i18n, labels, index_of(LANGUAGES, config::get().language), [this](int i) {
            const bool changed = config::get().language != LANGUAGES[i];
            config::get().language = LANGUAGES[i];
            ui::save_config();
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
            ui::save_config();
            this->refresh();
            if (changed) ui::offer_restart();
        });
        return true;
    });

    start_tab->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (const char* t : config::START_TABS) labels.push_back(brls::getStr(std::string("playguard/tabs/") + t));
        ui::pick("playguard/tools/start_tab"_i18n, labels, index_of(config::START_TABS, config::get().start_tab), [this](int i) {
            config::get().start_tab = config::START_TABS[i];
            ui::save_config();
            this->refresh();
        });
        return true;
    });

    auto_relock->init("playguard/tools/auto_relock"_i18n, config::get().auto_relock, [](bool on) {
        config::get().auto_relock = on;
        ui::save_config();
    });
    extra_amounts->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (const auto& set : config::EXTRA_SETS) labels.push_back(amounts_text(set));
        ui::pick("playguard/tools/extra_amounts"_i18n, labels, extra_set_index(config::get().extra_amounts), [this](int i) {
            config::get().extra_amounts.assign(std::begin(config::EXTRA_SETS[i]), std::end(config::EXTRA_SETS[i]));
            ui::save_config();
            this->refresh();
        });
        return true;
    });
    extra_auto->init("playguard/tools/extra_auto"_i18n, config::get().extra_auto_restore, [](bool on) {
        config::get().extra_auto_restore = on;
        ui::save_config();
    });
    advanced->init("playguard/tools/advanced"_i18n, config::get().advanced, [](bool on) {
        config::get().advanced = on;
        ui::save_config();
    });
    clock_check->init("playguard/tools/clock_check"_i18n, config::get().clock_check_at_start, [](bool on) {
        config::get().clock_check_at_start = on;
        ui::save_config();
    });
}

void PreferencesTab::refresh()
{
    const auto& cfg = config::get();
    language->setDetailText(brls::getStr("playguard/tools/languages/" + std::string(LANGUAGES[index_of(LANGUAGES, cfg.language)])));
    theme->setDetailText(brls::getStr("playguard/tools/themes/" + std::string(THEMES[index_of(THEMES, cfg.theme)])));
    start_tab->setDetailText(brls::getStr(std::string("playguard/tabs/") + config::START_TABS[index_of(config::START_TABS, cfg.start_tab)]));
    auto_relock->setOn(cfg.auto_relock, false);
    extra_amounts->setDetailText(amounts_text(config::EXTRA_SETS[extra_set_index(cfg.extra_amounts)]));
    extra_auto->setOn(cfg.extra_auto_restore, false);
    advanced->setOn(cfg.advanced, false);
    clock_check->setOn(cfg.clock_check_at_start, false);
    // Read-only: the play-timer preferences would change nothing.
    const bool ro = app::read_only();
    ui::set_visible_all({ { auto_relock.getView(), !ro },
                          { extra_amounts.getView(), !ro },
                          { extra_auto.getView(), !ro },
                          { advanced.getView(), !ro } });
}

brls::View* PreferencesTab::create()
{
    return new PreferencesTab();
}
