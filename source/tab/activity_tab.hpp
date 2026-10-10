// ActivityTab — play time per game (today, last 7 days, all time) from the
// console's play data, for every account or one, its summary over the chosen
// period and the games to rediscover (util/activity_summary.hpp), and its
// export to the SD card (CSV, JSON, XLSX or PDF). Read-only, so also in
// read-only mode. The data comes from play_data (read off the main thread, shared with the
// Overview, and kept on the SD card for the next run): what is known shows at
// once, and is read again in the background when over a minute old, without
// a spinner; Ⓧ reads it again with one. The chart marks each day's current
// limit (every account only).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <chrono>
#include <memory>
#include <utility>
#include <vector>

#include "tab/tab_base.hpp"
#include "util/pctl_ops_c.hpp"
#include "view/game_cell.hpp"
#include "view/play_days.hpp"

class ActivityTab : public TabBase
{
  public:
    ActivityTab();
    ~ActivityTab() override;
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    enum Period { Today = 0, Week = 1, AllTime = 2 };
    int period = Week;

    // Reads the data again. `shown`: with the spinner, even over a list (Ⓧ);
    // otherwise only when there is nothing to show yet.
    void fetch(bool shown);
    void rebuild();
    // The summary over the period, and the games to rediscover.
    void show_summary(const std::shared_ptr<const PlayStats>& data);
    // Reads the icons the list still lacks (first rows only), off the main
    // thread, then puts them on the cells on screen.
    void load_icons();
    void apply_icons();
    // What Ⓐ on a game's cell opens (its own screen, with this read's data).
    static void on_click(GameCell* cell, const GameStat& game, const std::shared_ptr<const PlayStats>& data);
    void export_to_sd() const;

    std::vector<std::pair<u64, GameCell*>> cells;   // the list on screen, top first
    brls::DetailCell* more_cell = nullptr;          // "Show all", below them, when rows are left out
    bool show_all = false;
    std::vector<u64> rediscover_ids;                  // the games to rediscover on screen, in order
    int listener = 0;                                 // play_data::listen id
    // Data arrived while another screen (a game's own screen, a dialog) was
    // on top: the list is rebuilt once this one is back. Deleting its cells
    // sooner would free the one borealis gives the focus back to.
    bool stale = false;
    bool spinner = false;   // Ⓧ pressed: the spinner stays until its read ends
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    BRLS_BIND(PlayDaysView,     days,        "ac_days");
    BRLS_BIND(brls::DetailCell, today,       "ac_today");
    BRLS_BIND(brls::DetailCell, week,        "ac_week");
    BRLS_BIND(brls::DetailCell, total,       "ac_total");
    BRLS_BIND(brls::DetailCell, account,     "ac_account");
    BRLS_BIND(brls::DetailCell, sort,        "ac_period");
    BRLS_BIND(brls::DetailCell, per_day,     "ac_per_day");
    BRLS_BIND(brls::DetailCell, top,         "ac_top");
    BRLS_BIND(brls::DetailCell, days_played, "ac_days_played");
    BRLS_BIND(brls::DetailCell, busiest,     "ac_busiest");
    BRLS_BIND(brls::DetailCell, session,     "ac_session");
    BRLS_BIND(brls::Box,        rediscover,  "ac_rediscover");
    BRLS_BIND(brls::Box,        rediscover_list, "ac_rediscover_list");
    BRLS_BIND(brls::DetailCell, export_cell, "ac_export");
    BRLS_BIND(brls::Box,        progress,    "ac_progress");
    BRLS_BIND(brls::Box,        list,        "ac_list");
    BRLS_BIND(brls::Label,      status,      "ac_status");
    BRLS_BIND(brls::Label,      note,        "ac_note");
};
