// PlayTimerTab — daily limit (same for every day / per day / remove), saved
// profiles, read-only bedtime info and the advanced (debug-class) actions.
// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"
#include "view/pt_state_header.hpp"

class PlayTimerTab : public TabBase
{
  public:
    PlayTimerTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    PtState pt = {};

    void choose_uniform();
    void apply_uniform(uint16_t minutes);
    void apply_days(const uint16_t days[7], const std::string& ok_text);
    void remove_limit();
    void save_profile();
    void load_profile();
    void delete_profile();

    BRLS_BIND(PtStateHeader,     state_header,    "pt_state_header");
    BRLS_BIND(brls::Label,       fw_note,         "pt_fw_note");
    BRLS_BIND(brls::Header,      limit_header,    "pt_limit_header");
    BRLS_BIND(brls::DetailCell,  quick,           "pt_quick");
    BRLS_BIND(brls::DetailCell,  per_day,         "pt_per_day");
    BRLS_BIND(brls::DetailCell,  remove,          "pt_remove");
    BRLS_BIND(brls::Header,      profiles_header, "pt_profiles_header");
    BRLS_BIND(brls::DetailCell,  profile_save,    "pt_profile_save");
    BRLS_BIND(brls::DetailCell,  profile_load,    "pt_profile_load");
    BRLS_BIND(brls::DetailCell,  profile_delete,  "pt_profile_delete");
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
