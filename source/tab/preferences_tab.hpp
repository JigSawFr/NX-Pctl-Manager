// PreferencesTab — language, theme and start tab; the play-timer preferences
// (lock again after a change, extra-time amounts, putting the usual limit
// back by itself, advanced actions); the start-up clock check. Split from
// Tools, which keeps backups, updates, diagnostics, the console and the
// developer tools.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"

class PreferencesTab : public TabBase
{
  public:
    PreferencesTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    BRLS_BIND(brls::DetailCell,  language,      "pf_language");
    BRLS_BIND(brls::DetailCell,  theme,         "pf_theme");
    BRLS_BIND(brls::DetailCell,  start_tab,     "pf_start_tab");
    BRLS_BIND(brls::BooleanCell, auto_relock,   "pf_auto_relock");
    BRLS_BIND(brls::DetailCell,  extra_amounts, "pf_extra_amounts");
    BRLS_BIND(brls::BooleanCell, extra_auto,    "pf_extra_auto");
    BRLS_BIND(brls::BooleanCell, advanced,      "pf_advanced");
    BRLS_BIND(brls::BooleanCell, clock_check,   "pf_clock_check");
    BRLS_BIND(brls::Label,       note,          "pf_note");
};
