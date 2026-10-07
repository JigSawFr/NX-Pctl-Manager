// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/activity_tab.hpp"

#include <algorithm>
#include <fmt/format.h>
#include <string>
#include <vector>

#include "ui/ui.hpp"

using namespace brls::literals;

namespace
{
// Coming back to the tab (after a dialog, the sort picker …) within this time
// reuses the last read instead of walking the activity log again.
constexpr auto MAX_AGE = std::chrono::seconds(60);

uint64_t value_of(const GameStat& g, int period)
{
    switch (period) {
        case 0:  return g.today_s;
        case 1:  return g.week_s;
        default: return g.totals_ok ? g.total_s : 0;
    }
}

std::string game_name(const GameStat& g)
{
    if (g.name[0]) return g.name;
    return brls::getStr("playguard/activity/deleted_game", fmt::format("{:016X}", (unsigned long long)g.app_id));
}

std::string line(const std::string& label, const std::string& value)
{
    return "\n" + brls::getStr("playguard/common/line", label, value);
}
}   // namespace

ActivityTab::ActivityTab()
    : TabBase("xml/tab/activity.xml")
{
    status->setSingleLine(false);
    note->setSingleLine(false);
    // Ⓧ reads the data again, however recent the last read.
    this->registerAction("playguard/hints/refresh"_i18n, brls::BUTTON_X, [this](brls::View*) {
        this->fetch();
        return true;
    });
    sort->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (int p = 0; p < 3; p++) labels.push_back(brls::getStr(fmt::format("playguard/activity/periods/{}", p)));
        ui::pick("playguard/activity/period"_i18n, labels, this->period, [this](int index) {
            this->period = index;
            this->rebuild();
        });
        return true;
    });
    this->rebuild();
}

void ActivityTab::refresh()
{
    if (this->stats && std::chrono::steady_clock::now() - this->read_at < MAX_AGE) return;
    this->fetch();
}

void ActivityTab::fetch()
{
    if (this->busy) return;
    this->busy = true;
    if (!this->stats) {
        status->setText("playguard/activity/loading"_i18n);
        status->setTextColor(ui::color_note());
        ui::set_visible(status, true);
    }
    std::weak_ptr<bool> weak = this->alive;
    brls::async([this, weak]() {
        auto data = std::make_shared<PlayStats>();
        playstats_fetch(data.get());
        brls::sync([this, weak, data]() {
            if (weak.expired()) return;   // the tab was closed meanwhile
            this->busy    = false;
            this->stats   = data;
            this->read_at = std::chrono::steady_clock::now();
            this->rebuild();
        });
    });
}

void ActivityTab::rebuild()
{
    sort->setDetailText(brls::getStr(fmt::format("playguard/activity/periods/{}", this->period)));
    if (!this->stats) return;
    const PlayStats& s = *this->stats;
    const std::string na = "playguard/common/unavailable"_i18n;

    uint64_t today_total = 0, week_total = 0;
    for (uint32_t i = 0; i < s.count; i++) {
        today_total += s.games[i].today_s;
        week_total  += s.games[i].week_s;
    }
    today->setDetailText(s.windows_ok ? ui::fmt_play_time(today_total) : na);
    week->setDetailText(s.windows_ok ? ui::fmt_play_time(week_total) : na);

    // The games played in the chosen period, most played first.
    std::vector<const GameStat*> rows;
    for (uint32_t i = 0; i < s.count; i++)
        if (value_of(s.games[i], this->period) > 0) rows.push_back(&s.games[i]);
    const int p = this->period;
    std::sort(rows.begin(), rows.end(), [p](const GameStat* a, const GameStat* b) {
        const uint64_t va = value_of(*a, p), vb = value_of(*b, p);
        return va != vb ? va > vb : a->last_played > b->last_played;
    });

    // The cells are about to be deleted: never leave the focus on one.
    bool focus_in_list = false;
    for (brls::View* v = brls::Application::getCurrentFocus(); v && !focus_in_list; v = v->getParent())
        focus_in_list = v == list.getView();
    if (focus_in_list) brls::Application::giveFocus(sort);
    list->clearViews();
    for (const GameStat* g : rows) {
        auto* cell = new brls::DetailCell();
        cell->setText(game_name(*g));
        cell->setDetailText(ui::fmt_play_time(value_of(*g, p)));
        const GameStat copy = *g;
        cell->registerClickAction([this, copy](brls::View*) {
            this->show_details(copy);
            return true;
        });
        list->addView(cell);
    }

    std::string text;
    bool error = true;
    if (R_FAILED(s.rc))
        text = "playguard/activity/err_list"_i18n + " — " + ui::rc_text(s.rc);
    else if (p != AllTime && !s.windows_ok)
        text = "playguard/activity/err_log"_i18n + " — " + ui::rc_text(s.events_rc);
    else if (p == AllTime && R_FAILED(s.stats_rc))
        text = "playguard/activity/err_stats"_i18n + " — " + ui::rc_text(s.stats_rc);
    else if (rows.empty()) {
        text  = brls::getStr(fmt::format("playguard/activity/none/{}", p));
        error = false;
    }
    status->setText(text);
    status->setTextColor(error ? ui::color_warn() : ui::color_note());
    ui::set_visible(status, !text.empty());
}

void ActivityTab::show_details(const GameStat& g) const
{
    std::string text = game_name(g) + "\n";
    if (this->stats && this->stats->windows_ok) {
        text += line("playguard/activity/today"_i18n, ui::fmt_play_time(g.today_s));
        text += line("playguard/activity/week"_i18n, ui::fmt_play_time(g.week_s));
    }
    if (g.totals_ok) {
        text += line("playguard/activity/all_time"_i18n, ui::fmt_play_time(g.total_s));
        text += line("playguard/activity/launches"_i18n, std::to_string(g.launches));
        if (g.first_played) text += line("playguard/activity/first"_i18n, ui::time_text(g.first_played));
        if (g.last_played) text += line("playguard/activity/last"_i18n, ui::time_text(g.last_played));
    }
    ui::info(text);
}

brls::View* ActivityTab::create()
{
    return new ActivityTab();
}
