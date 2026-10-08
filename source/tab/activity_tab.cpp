// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/activity_tab.hpp"

#include <algorithm>
#include <cstdlib>
#include <fmt/format.h>
#include <string>
#include <vector>

#include <map>
#include <set>

#include "action/play_data.hpp"
#include "activity/game_activity.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/paths.hpp"
#include "util/table_export.hpp"

using namespace brls::literals;

namespace
{
// Coming back to the tab (after a dialog, the sort picker …) within this time
// reuses the last read instead of walking the activity log again.
constexpr auto MAX_AGE = std::chrono::seconds(60);

// Icons for the first rows of the list only: each one is a texture of a few
// hundred KB, and the list can hold hundreds of games.
constexpr size_t ICON_ROWS = 16;

// Shared by every ActivityTab (borealis rebuilds the tab each time the
// sidebar reaches it); the play data itself is play_data's. UI thread only.
struct
{
    ActivityTab* shown = nullptr;       // the tab on screen, if any
    int account = -1;                   // index in play_data::accounts(), -1 every account
    // Icons read so far (the bytes as the control data holds them), and the
    // games known to have none; kept for the run (icons do not change).
    std::map<u64, std::vector<unsigned char>> icons;
    std::set<u64> no_icon;
    bool icons_busy = false;
    bool icons_wanted = true;           // false in applet mode (little memory)
} s_cache;

// The account the tab shows, or nullptr for every account.
const PlayAccount* chosen_account()
{
    const auto& list = play_data::accounts();
    return s_cache.account >= 0 && s_cache.account < (int)list.size() ? &list[s_cache.account] : nullptr;
}

std::string account_label(const PlayAccount* a)
{
    if (!a) return "playguard/activity/account_all"_i18n;
    return a->nickname[0] ? std::string(a->nickname) : "?";
}

std::shared_ptr<const PlayStats> shown_stats()
{
    return play_data::latest(play_data::key_of(chosen_account()));
}

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
    account->registerClickAction([this](brls::View*) {
        const auto& list = play_data::accounts();
        std::vector<std::string> labels = { account_label(nullptr) };
        for (const auto& a : list) labels.push_back(account_label(&a));
        ui::pick("playguard/activity/account"_i18n, labels, s_cache.account + 1, [this](int index) {
            s_cache.account = index - 1;
            this->rebuild();
            if (!play_data::fresh(play_data::key_of(chosen_account()), MAX_AGE)) this->fetch();
        });
        return true;
    });
    sort->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (int p = 0; p < 3; p++) labels.push_back(brls::getStr(fmt::format("playguard/activity/periods/{}", p)));
        ui::pick("playguard/activity/period"_i18n, labels, this->period, [this](int index) {
            this->period = index;
            config::get().activity_period = index;   // the next visit starts there
            ui::save_config();
            this->rebuild();
        });
        return true;
    });
    export_cell->registerClickAction([this](brls::View*) {
        this->export_to_sd();
        return true;
    });
    this->period = config::get().activity_period;
    s_cache.shown = this;
    this->listener = play_data::listen([this]() { this->rebuild(); });
    // No account to choose between on a console with one (or none listed).
    ui::set_visible(account.getView(), play_data::accounts().size() > 1);
    // Applet mode (opened from the album): a few MB for icons is too much.
    SysInfo si;
    sysinfo_get(&si);
    s_cache.icons_wanted = !si.applet_mode;
    ui::set_visible(progress.getView(), false);
    this->rebuild();
}

ActivityTab::~ActivityTab()
{
    play_data::unlisten(this->listener);
    if (s_cache.shown == this) s_cache.shown = nullptr;
}

void ActivityTab::refresh()
{
    if (play_data::fresh(play_data::key_of(chosen_account()), MAX_AGE)) return;
    this->fetch();
}

void ActivityTab::fetch()
{
    // Every read says so, not only the first (X with a list on screen): a
    // spinner, since a long log takes a few seconds.
    ui::set_visible(progress.getView(), true);
    ui::set_visible(status, false);
    play_data::fetch(chosen_account());   // one read per account at a time
}

void ActivityTab::rebuild()
{
    const PlayAccount* who = chosen_account();
    sort->setDetailText(brls::getStr(fmt::format("playguard/activity/periods/{}", this->period)));
    account->setDetailText(account_label(who));
    const bool reading = play_data::busy(play_data::key_of(who));
    const std::shared_ptr<const PlayStats> data = shown_stats();
    if (!data) {
        // Nothing read for this account yet: an empty list, not the previous one.
        list->clearViews();
        this->cells.clear();
        ui::set_visible(progress.getView(), reading);
        return;
    }
    const PlayStats& s = *data;
    const std::string na = "playguard/common/unavailable"_i18n;

    uint64_t today_total = 0, week_total = 0, all_total = 0;
    for (uint32_t i = 0; i < s.count; i++) {
        today_total += s.games[i].today_s;
        week_total  += s.games[i].week_s;
        if (s.games[i].totals_ok) all_total += s.games[i].total_s;
    }
    if (s.windows_ok) {
        // Every account: each day's current limit as a line (the limit is the
        // console's, not an account's). A day above it is drawn in amber.
        PtState pt;
        pctl_play_timer_query(&pt);
        uint16_t limits[7];
        const bool with_limits = !who && pt.fw_supported && pt.valid;
        for (int k = 0; k < 7; k++) limits[k] = with_limits ? pt.day_min[s.day_wday[k] % 7] : PT_DAY_NOLIMIT;
        days->show(s, with_limits ? limits : nullptr);
    }
    ui::set_visible(days.getView(), s.windows_ok);
    today->setDetailText(s.windows_ok ? ui::fmt_play_time(today_total) : na);
    week->setDetailText(s.windows_ok ? ui::fmt_play_time(week_total) : na);
    total->setDetailText(R_SUCCEEDED(s.stats_rc) ? ui::fmt_play_time(all_total) : na);

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
    this->cells.clear();
    ui::set_visible(progress.getView(), reading);
    for (const GameStat* g : rows) {
        auto* cell = new GameCell(s_cache.icons_wanted && this->cells.size() < ICON_ROWS);
        cell->setText(game_name(*g));
        cell->setDetailText(ui::fmt_play_time(value_of(*g, p)));
        const GameStat copy = *g;
        cell->registerClickAction([copy, data](brls::View*) {
            // Its own screen: the seven days as bars, the figures, each account.
            std::vector<unsigned char> icon;
            const auto it = s_cache.icons.find(copy.app_id);
            if (it != s_cache.icons.end()) icon = it->second;
            brls::Application::pushActivity(new GameActivity(copy, data, std::move(icon)));
            return true;
        });
        list->addView(cell);
        this->cells.emplace_back(g->app_id, cell);
    }
    this->apply_icons();
    this->load_icons();

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

void ActivityTab::apply_icons()
{
    for (size_t i = 0; i < this->cells.size() && i < ICON_ROWS; i++) {
        const auto it = s_cache.icons.find(this->cells[i].first);
        if (it != s_cache.icons.end()) this->cells[i].second->set_icon(it->second);
    }
}

void ActivityTab::load_icons()
{
    if (!s_cache.icons_wanted || s_cache.icons_busy) return;
    auto wanted = std::make_shared<std::vector<PlayIcon>>();
    for (size_t i = 0; i < this->cells.size() && i < ICON_ROWS; i++) {
        const u64 id = this->cells[i].first;
        if (s_cache.icons.count(id) || s_cache.no_icon.count(id)) continue;
        wanted->push_back(PlayIcon{ id, nullptr, 0 });
    }
    if (wanted->empty()) return;
    s_cache.icons_busy = true;
    brls::async([wanted]() {
        playstats_icons(wanted->data(), wanted->size());
        brls::sync([wanted]() {
            s_cache.icons_busy = false;
            for (PlayIcon& icon : *wanted) {
                if (icon.jpeg && icon.size) s_cache.icons[icon.app_id].assign(icon.jpeg, icon.jpeg + icon.size);
                else s_cache.no_icon.insert(icon.app_id);
                std::free(icon.jpeg);
                icon.jpeg = nullptr;
            }
            // The list may have changed meanwhile (another period): put what
            // arrived on the cells now on screen, then ask for what they lack.
            if (s_cache.shown) {
                s_cache.shown->apply_icons();
                s_cache.shown->load_icons();
            }
        });
    });
}

// Every game with any figure, in the order of the list on screen; minutes as
// numbers, so a spreadsheet can add them up.
void ActivityTab::export_to_sd() const
{
    const std::shared_ptr<const PlayStats> data = shown_stats();
    if (!data) {
        ui::notify("playguard/activity/export_not_ready"_i18n);
        return;
    }
    std::vector<std::string> labels;
    for (int f = 0; f < 4; f++) labels.push_back(brls::getStr(fmt::format("playguard/activity/formats/{}", f)));
    const int p = this->period;
    const std::string who = account_label(chosen_account());
    ui::pick("playguard/activity/export_title"_i18n, labels, config::get().export_format, [data, p, who](int index) {
        config::get().export_format = index;
        ui::save_config();
        const PlayStats& s = *data;
        table_export::Table t;
        t.title    = "PlayGuard — " + "playguard/tabs/activity"_i18n;
        t.subtitle = brls::getStr("playguard/activity/export_subtitle", ui::time_text(s.now), who);
        t.sheet    = "playguard/tabs/activity"_i18n;
        t.columns  = {
            { "game", "playguard/activity/columns/game"_i18n, false },
            { "title_id", "playguard/activity/columns/id"_i18n, false },
            { "today_min", "playguard/activity/columns/today"_i18n, true },
        };
        // The six days before today, most recent first ("Sat (min)").
        for (int k = 1; k < 7; k++)
            t.columns.push_back({ fmt::format("day_{}_min", k),
                                  brls::getStr("playguard/activity/columns/day",
                                               brls::getStr(fmt::format("playguard/days_short/{}", (int)s.day_wday[k]))),
                                  true });
        t.columns.insert(t.columns.end(), {
            { "week_min", "playguard/activity/columns/week"_i18n, true },
            { "total_min", "playguard/activity/columns/total"_i18n, true },
            { "launches", "playguard/activity/launches"_i18n, true },
            { "first_played", "playguard/activity/first"_i18n, false },
            { "last_played", "playguard/activity/last"_i18n, false },
        });
        std::vector<const GameStat*> games;
        for (uint32_t i = 0; i < s.count; i++) games.push_back(&s.games[i]);
        std::sort(games.begin(), games.end(), [p](const GameStat* a, const GameStat* b) {
            const uint64_t va = value_of(*a, p), vb = value_of(*b, p);
            if (va != vb) return va > vb;
            return a->total_s != b->total_s ? a->total_s > b->total_s : a->last_played > b->last_played;
        });
        auto minutes = [](uint64_t seconds) { return std::to_string((seconds + 30) / 60); };
        for (const GameStat* g : games) {
            t.rows.push_back({
                game_name(*g),
                fmt::format("{:016X}", (unsigned long long)g->app_id),
                s.windows_ok ? minutes(g->today_s) : "",
            });
            for (int k = 1; k < 7; k++) t.rows.back().push_back(s.windows_ok ? minutes(g->day_s[k]) : "");
            t.rows.back().insert(t.rows.back().end(), {
                s.windows_ok ? minutes(g->week_s) : "",
                g->totals_ok ? minutes(g->total_s) : "",
                g->totals_ok ? std::to_string(g->launches) : "",
                g->totals_ok && g->first_played ? ui::time_text(g->first_played) : "",
                g->totals_ok && g->last_played ? ui::time_text(g->last_played) : "",
            });
        }
        std::string err;
        const std::string path = table_export::save(t, (table_export::Format)index, paths::exports_dir(), "activity", &err);
        if (path.empty()) ui::error("playguard/activity/export_err"_i18n + ": " + err);
        else ui::notify(brls::getStr("playguard/activity/exported", path));
    });
}

brls::View* ActivityTab::create()
{
    return new ActivityTab();
}
