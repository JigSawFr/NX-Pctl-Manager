// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/data_notice.hpp"

#include <borealis.hpp>
#include <string>

#include "action/history_flow.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/history.hpp"

using namespace brls::literals;

namespace data_notice
{

namespace
{
bool s_rescue  = false;   // a report that could not be read
bool s_history = false;   // history.json put aside before at_start()
bool s_started = false;   // at_start() ran: later news is shown at once
}   // namespace

void rescue_unreadable()
{
    s_rescue = true;
}

void history_put_aside()
{
    brls::Logger::warning("history: damaged file put aside as history.json.bad");
    if (!s_started) {
        s_history = true;
        return;
    }
    brls::sync([]() { ui::info("playguard/data_notice/history_reset"_i18n); });
}

void at_start()
{
    if (s_started) return;
    s_started = true;

    std::string text;
    auto add = [&text](const std::string& part) { text += (text.empty() ? "" : "\n\n") + part; };

    const config::Loaded cfg = config::loaded();
    if (cfg == config::Loaded::PutAside) add("playguard/data_notice/config_reset"_i18n);
    else if (cfg == config::Loaded::Stuck) add("playguard/data_notice/config_stuck"_i18n);

    // Before the entry below, which would otherwise put it aside on its own.
    const history::Damage hist = history::check();
    if (hist == history::Damage::PutAside || s_history) add("playguard/data_notice/history_reset"_i18n);
    else if (hist == history::Damage::Stuck) add("playguard/data_notice/history_stuck"_i18n);
    if (s_rescue) add("playguard/data_notice/rescue_unreadable"_i18n);

    if (cfg == config::Loaded::PutAside || cfg == config::Loaded::Stuck) {
        brls::Logger::warning("config: unreadable, defaults in use ({})",
                              cfg == config::Loaded::PutAside ? "kept as config.json.bad" : "could not be put aside");
        history_flow::record_event("config_reset");
    }
    if (hist != history::Damage::None)
        brls::Logger::warning("history: unreadable at start-up ({})",
                              hist == history::Damage::PutAside ? "kept as history.json.bad" : "could not be put aside");
    if (text.empty()) return;
    brls::sync([text]() { ui::info(text); });
}

}   // namespace data_notice
