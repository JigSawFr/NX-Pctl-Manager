// DashboardTab — overview: today's play time, parental-control state, system
// warnings. Refreshed every 5 s while shown; A on a line opens the matching tab.
// When the timer reports no time left (no game running yet, or no limit
// today), today's play time comes from the activity log (play_data, read in
// the background at most every few minutes).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"
#include "util/patches.hpp"
#include "util/pctl_ops_c.hpp"
#include "view/pt_gauge.hpp"

class DashboardTab : public TabBase
{
  public:
    DashboardTab();
    ~DashboardTab() override;
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    PtState pt = {};
    bool clock_inaccurate = false;   // last read: A on the clock line offers to fix it
    int  listener = 0;               // play_data::listen id

    void open_today_limit();

    BRLS_BIND(brls::DetailCell, unlocked_banner, "dash_unlocked_banner");
    BRLS_BIND(brls::DetailCell, pc,          "dash_pc");
    BRLS_BIND(brls::DetailCell, pin,         "dash_pin");
    BRLS_BIND(brls::DetailCell, level,       "dash_level");
    BRLS_BIND(PtGauge,          gauge,       "dash_gauge");
    BRLS_BIND(brls::Label,      gauge_text,  "dash_gauge_text");
    BRLS_BIND(brls::DetailCell, today_limit, "dash_today_limit");
    BRLS_BIND(brls::DetailCell, extra,       "dash_extra");
    BRLS_BIND(brls::DetailCell, stop,        "dash_stop");
    BRLS_BIND(brls::DetailCell, extra_pending, "dash_extra_pending");
    BRLS_BIND(brls::DetailCell, remaining,   "dash_remaining");
    BRLS_BIND(brls::DetailCell, bedtime,     "dash_bedtime");
    BRLS_BIND(brls::DetailCell, alarm,       "dash_alarm");
    BRLS_BIND(brls::DetailCell, clock,       "dash_clock");
    BRLS_BIND(brls::DetailCell, pairing,     "dash_pairing");
    BRLS_BIND(brls::DetailCell, serial,      "dash_serial");
    BRLS_BIND(brls::DetailCell, game_patches, "dash_patches");
    BRLS_BIND(brls::DetailCell, fw,          "dash_fw");
    BRLS_BIND(brls::DetailCell, compat,      "dash_compat");
    BRLS_BIND(brls::DetailCell, first_steps, "dash_first_steps");
    BRLS_BIND(brls::Label,      applet,      "dash_applet");
    BRLS_BIND(brls::Label,      updated,     "dash_updated");
};
