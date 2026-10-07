// ActivityTab — play time per game (today, last 7 days, all time) from the
// console's play data, for every account, and its export to the SD card (CSV,
// JSON, XLSX or PDF). Read-only, so also in read-only mode. The data is read
// off the main thread (it walks the whole activity log) and kept for a minute
// for the whole app, so coming back from a dialog or passing the tab in the
// sidebar does not re-read it (the tab itself is rebuilt each time); Ⓧ does.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <chrono>
#include <memory>

#include "tab/tab_base.hpp"
#include "util/pctl_ops_c.hpp"

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
    void show_details(const GameStat& g) const;
    void export_to_sd() const;

    BRLS_BIND(brls::DetailCell, today,       "ac_today");
    BRLS_BIND(brls::DetailCell, week,        "ac_week");
    BRLS_BIND(brls::DetailCell, sort,        "ac_period");
    BRLS_BIND(brls::DetailCell, export_cell, "ac_export");
    BRLS_BIND(brls::Box,        list,        "ac_list");
    BRLS_BIND(brls::Label,      status,      "ac_status");
    BRLS_BIND(brls::Label,      note,        "ac_note");
};
