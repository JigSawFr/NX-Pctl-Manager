// PlayTimerTab — daily limit (same for every day / extra time today / per day /
// remove), saved profiles, the bedtime alarm and the advanced
// (debug-class) actions.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"
#include "view/pt_state_header.hpp"
#include "view/pt_week.hpp"

class PlayTimerTab : public TabBase
{
  public:
    PlayTimerTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    PtState pt = {};

    void remove_limit();

    BRLS_BIND(brls::DetailCell,  unlocked_banner, "pt_unlocked_banner");
    BRLS_BIND(PtStateHeader,     state_header,    "pt_state_header");
    BRLS_BIND(brls::Header,      week_header,     "pt_week_header");
    BRLS_BIND(PtWeekView,        week,            "pt_week");
    BRLS_BIND(brls::Label,       fw_note,         "pt_fw_note");
    BRLS_BIND(brls::Header,      limit_header,    "pt_limit_header");
    BRLS_BIND(brls::DetailCell,  quick,           "pt_quick");
    BRLS_BIND(brls::DetailCell,  extra,           "pt_extra");
    BRLS_BIND(brls::DetailCell,  stop,            "pt_stop");
    BRLS_BIND(brls::DetailCell,  per_day,         "pt_per_day");
    BRLS_BIND(brls::DetailCell,  remove,          "pt_remove");
    BRLS_BIND(brls::DetailCell,  profiles_cell,   "pt_profiles");
    BRLS_BIND(brls::Label,       limit_note,      "pt_limit_note");
    BRLS_BIND(brls::Header,      bedtime_header,  "pt_bedtime_header");
    BRLS_BIND(brls::DetailCell,  bedtime,         "pt_bedtime");
    BRLS_BIND(brls::DetailCell,  bedtime_reset,   "pt_bedtime_reset");
    BRLS_BIND(brls::Label,       bedtime_note,    "pt_bedtime_note");
    BRLS_BIND(brls::Header,      adv_header,      "pt_adv_header");
    BRLS_BIND(brls::BooleanCell, alarm,           "pt_alarm");
    BRLS_BIND(brls::DetailCell,  pause,           "pt_pause");
    BRLS_BIND(brls::DetailCell,  resume,          "pt_resume");
    BRLS_BIND(brls::DetailCell,  diag,            "pt_diag");
};
