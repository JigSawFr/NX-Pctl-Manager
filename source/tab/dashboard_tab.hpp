// DashboardTab — read-only overview, refreshed every 5 s while in focus.
// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"
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
    brls::RepeatingTimer timer;

    BRLS_BIND(brls::DetailCell, pc,          "dash_pc");
    BRLS_BIND(brls::DetailCell, pin,         "dash_pin");
    BRLS_BIND(brls::DetailCell, level,       "dash_level");
    BRLS_BIND(PtGauge,          gauge,       "dash_gauge");
    BRLS_BIND(brls::Label,      gauge_text,  "dash_gauge_text");
    BRLS_BIND(brls::DetailCell, today_limit, "dash_today_limit");
    BRLS_BIND(brls::DetailCell, remaining,   "dash_remaining");
    BRLS_BIND(brls::DetailCell, bedtime,     "dash_bedtime");
    BRLS_BIND(brls::DetailCell, clock,       "dash_clock");
    BRLS_BIND(brls::DetailCell, pairing,     "dash_pairing");
    BRLS_BIND(brls::DetailCell, fw,          "dash_fw");
    BRLS_BIND(brls::DetailCell, compat,      "dash_compat");
};
