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

// Rows built at first: each is a cell inflated from XML, and a large library
// holds hundreds of games. "Show all" builds the rest on demand.
constexpr size_t ROWS_SHOWN = 50;

// Decoded icons kept for the run, at most this many (a few hundred KB of
// texture each): past it, a cell decodes its own and frees it.
constexpr size_t TEXTURES_KEPT = 48;

// Shared by every ActivityTab (borealis rebuilds the tab each time the
// sidebar reaches it); the play data itself is play_data's. UI thread only.
struct
{
    ActivityTab* shown = nullptr;       // the tab on screen, if any
    int account = -1;                   // index in play_data::accounts(), -1 every account
    // Icons read so far (the bytes as the control data holds them), and the
    // games known to have none; kept for the run (icons do not change).
    std::map<u64, std::vector<unsigned char>> icons;
    std::map<u64, int> textures;        // the same icons decoded once (NVG images)
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

// Most played in `period` first, a tie to the most recently played. The
// export (`by_total`) breaks a tie by all-time play first (raw total_s), so
// its unplayed games keep a useful order too.
void sort_by_play(std::vector<const GameStat*>& games, int period, bool by_total)
{
    std::sort(games.begin(), games.end(), [period, by_total](const GameStat* a, const GameStat* b) {
        const uint64_t va = value_of(*a, period), vb = value_of(*b, period);
        if (va != vb) return va > vb;
        if (by_total && a->total_s != b->total_s) return a->total_s > b->total_s;
        return a->last_played > b->last_played;
    });
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
        this->fetch(true);
        return true;
    });
    account->registerClickAction([this](brls::View*) {
        const auto& list = play_data::accounts();
        std::vector<std::string> labels = { account_label(nullptr) };
        for (const auto& a : list) labels.push_back(account_label(&a));
        ui::pick("playguard/activity/account"_i18n, labels, s_cache.account + 1, [this](int index) {
            s_cache.account = index - 1;
            this->show_all = false;
            this->rebuild();
            if (!play_data::fresh(play_data::key_of(chosen_account()), MAX_AGE)) this->fetch(false);
        });
        return true;
    });
    sort->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (int p = 0; p < 3; p++) labels.push_back(brls::getStr(fmt::format("playguard/activity/periods/{}", p)));
        ui::pick("playguard/activity/period"_i18n, labels, this->period, [this](int index) {
            this->period = index;
            this->show_all = false;
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
    this->listener = play_data::listen([this]() {
        const auto stack = brls::Application::getActivitiesStack();
        if (!stack.empty() && stack.back() != this->getParentActivity()) {
            this->stale = true;
            return;
        }
        this->rebuild();
    });
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
    if (this->stale) {
        // Back on top: next frame, once borealis has put the focus back.
        this->stale = false;
        std::weak_ptr<bool> weak = this->alive;
        brls::sync([this, weak]() {
            if (!weak.expired()) this->rebuild();
        });
    }
    if (play_data::fresh(play_data::key_of(chosen_account()), MAX_AGE)) return;
    this->fetch(false);
}

void ActivityTab::fetch(bool shown)
{
    // A long log takes a few seconds. What is known stays on screen meanwhile
    // (the last read, this run's or the last run's), so a read of its own is
    // silent; Ⓧ, or nothing to show yet, gets the spinner.
    const bool nothing = !shown_stats();
    if (shown) this->spinner = true;
    if (shown || nothing) {
        ui::set_visible(progress.getView(), true);
        ui::set_visible(status, false);
    }
    play_data::fetch(chosen_account());   // one read per account at a time
}

void ActivityTab::rebuild()
{
    const PlayAccount* who = chosen_account();
    sort->setDetailText(brls::getStr(fmt::format("playguard/activity/periods/{}", this->period)));
    account->setDetailText(account_label(who));
    const bool reading = play_data::busy(play_data::key_of(who));
    if (!reading) this->spinner = false;
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
    sort_by_play(rows, p, false);

    // The list on screen: the first ROWS_SHOWN rows unless "Show all" was
    // chosen. When the same games are already there in the same order (a read
    // in the background, a refresh), only their figures change: no cell is
    // rebuilt and the focus stays where it is.
    const size_t shown_n = this->show_all ? rows.size() : std::min(rows.size(), ROWS_SHOWN);
    const bool more = shown_n < rows.size();
    // The cells there already are the first games, in order: kept (and the
    // rest added below them, after "Show every game").
    bool prefix = this->cells.size() <= shown_n;
    for (size_t i = 0; prefix && i < this->cells.size(); i++) prefix = this->cells[i].first == rows[i]->app_id;
    const bool grow = prefix && !this->cells.empty() && this->cells.size() < shown_n && this->more_cell;
    const bool same = prefix && this->cells.size() == shown_n && more == (this->more_cell != nullptr);
    ui::set_visible(progress.getView(), reading && this->spinner);
    if (same || grow) {
        for (size_t i = 0; i < this->cells.size(); i++) {
            GameCell* cell = this->cells[i].second;
            cell->setText(game_name(*rows[i]));
            cell->setDetailText(ui::fmt_play_time(value_of(*rows[i], p)));
            this->on_click(cell, *rows[i], data);
        }
        if (this->more_cell) this->more_cell->setDetailText(std::to_string(rows.size()));
    }
    if (grow) {
        // "Show every game": the rest below, then the focus on the first of
        // them (next frame, once they are laid out, so the list scrolls to it).
        const size_t first_new = this->cells.size();
        for (size_t i = first_new; i < shown_n; i++) {
            auto* cell = new GameCell(false);
            cell->setText(game_name(*rows[i]));
            cell->setDetailText(ui::fmt_play_time(value_of(*rows[i], p)));
            this->on_click(cell, *rows[i], data);
            list->addView(cell, list->getChildren().size() - 1);   // above "Show every game"
            this->cells.emplace_back(rows[i]->app_id, cell);
        }
        brls::Application::giveFocus(this->cells[first_new].second);
        if (!more) {
            list->removeView(this->more_cell);   // deletes it
            this->more_cell = nullptr;
        }
        std::weak_ptr<bool> weak = this->alive;
        const u64 id = this->cells[first_new].first;
        brls::sync([this, weak, id]() {
            if (weak.expired()) return;
            for (const auto& c : this->cells)
                if (c.first == id) brls::Application::giveFocus(c.second);
        });
        brls::Logger::info("activity list: {} of {} games", shown_n, rows.size());
    } else if (!same) {
        // The cells are about to be deleted: never leave the focus on one, and
        // give it back to the same game afterwards when it is still listed.
        u64 focused = 0;
        bool focus_in_list = false;
        for (brls::View* v = brls::Application::getCurrentFocus(); v && !focus_in_list; v = v->getParent()) {
            for (const auto& c : this->cells)
                if (v == c.second) focused = c.first;
            focus_in_list = v == list.getView();
        }
        if (focus_in_list) brls::Application::giveFocus(sort);
        list->clearViews();
        this->cells.clear();
        this->more_cell = nullptr;
        brls::View* refocus = nullptr;
        for (size_t i = 0; i < shown_n; i++) {
            const GameStat* g = rows[i];
            auto* cell = new GameCell(s_cache.icons_wanted && this->cells.size() < ICON_ROWS);
            cell->setText(game_name(*g));
            cell->setDetailText(ui::fmt_play_time(value_of(*g, p)));
            this->on_click(cell, *g, data);
            list->addView(cell);
            this->cells.emplace_back(g->app_id, cell);
            if (focused && g->app_id == focused) refocus = cell;
        }
        if (more) {
            this->more_cell = new brls::DetailCell();
            this->more_cell->setText("playguard/activity/show_all"_i18n);
            this->more_cell->setDetailText(std::to_string(rows.size()));
            this->more_cell->registerClickAction([this](brls::View*) {
                // Next frame: rebuilding deletes this cell, whose action runs now.
                std::weak_ptr<bool> weak = this->alive;
                brls::sync([this, weak]() {
                    if (weak.expired()) return;
                    this->show_all = true;
                    this->rebuild();
                });
                return true;
            });
            list->addView(this->more_cell);
        }
        if (refocus) brls::Application::giveFocus(refocus);
        brls::Logger::info("activity list: {} of {} games", shown_n, rows.size());
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

void ActivityTab::on_click(GameCell* cell, const GameStat& game, const std::shared_ptr<const PlayStats>& data)
{
    const GameStat copy = game;
    cell->registerClickAction([copy, data](brls::View*) {
        // Its own screen: the seven days as bars, the figures, each account.
        std::vector<unsigned char> icon;
        const auto it = s_cache.icons.find(copy.app_id);
        if (it != s_cache.icons.end()) icon = it->second;
        brls::Application::pushActivity(new GameActivity(copy, data, std::move(icon)));
        return true;
    });
}

void ActivityTab::apply_icons()
{
    for (size_t i = 0; i < this->cells.size() && i < ICON_ROWS; i++) {
        const u64 id = this->cells[i].first;
        const auto it = s_cache.icons.find(id);
        if (it == s_cache.icons.end() || it->second.empty()) continue;
        // Decoded once for the run (a rebuild, another period, another
        // account reuse it), up to TEXTURES_KEPT icons.
        auto tex = s_cache.textures.find(id);
        if (tex == s_cache.textures.end() && s_cache.textures.size() < TEXTURES_KEPT) {
            const int t = nvgCreateImageMem(brls::Application::getNVGContext(), 0, it->second.data(), (int)it->second.size());
            if (t) tex = s_cache.textures.emplace(id, t).first;
        }
        if (tex != s_cache.textures.end()) this->cells[i].second->set_icon_texture(tex->second);
        else this->cells[i].second->set_icon(it->second);
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
        sort_by_play(games, p, true);
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
