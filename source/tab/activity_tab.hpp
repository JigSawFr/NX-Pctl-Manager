// ActivityTab — play time per game (today, last 7 days, all time) from the
// console's play data, for every account or one, and its export to the SD
// card (CSV, JSON, XLSX or PDF). Read-only, so also in read-only mode. The
// data comes from play_data (read off the main thread, shared with the
// Overview): coming back within a minute reuses it; Ⓧ reads it again. The
// chart marks each day's current limit (every account only).
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

    void fetch();
    void rebuild();
    // Reads the icons the list still lacks (first rows only), off the main
    // thread, then puts them on the cells on screen.
    void load_icons();
    void apply_icons();
    void export_to_sd() const;

    std::vector<std::pair<u64, GameCell*>> cells;   // the list on screen, top first
    int listener = 0;                                 // play_data::listen id
    // Data arrived while another screen (a game's own screen, a dialog) was
    // on top: the list is rebuilt once this one is back. Deleting its cells
    // sooner would free the one borealis gives the focus back to.
    bool stale = false;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    BRLS_BIND(PlayDaysView,     days,        "ac_days");
    BRLS_BIND(brls::DetailCell, today,       "ac_today");
    BRLS_BIND(brls::DetailCell, week,        "ac_week");
    BRLS_BIND(brls::DetailCell, total,       "ac_total");
    BRLS_BIND(brls::DetailCell, account,     "ac_account");
    BRLS_BIND(brls::DetailCell, sort,        "ac_period");
    BRLS_BIND(brls::DetailCell, export_cell, "ac_export");
    BRLS_BIND(brls::Box,        progress,    "ac_progress");
    BRLS_BIND(brls::Box,        list,        "ac_list");
    BRLS_BIND(brls::Label,      status,      "ac_status");
    BRLS_BIND(brls::Label,      note,        "ac_note");
};
