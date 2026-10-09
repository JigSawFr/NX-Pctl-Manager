// SecurityTab — PIN (set / show), temporary unlock / relock, the companion
// phone app link, delete everything.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"

class SecurityTab : public TabBase
{
  public:
    SecurityTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    BRLS_BIND(brls::DetailCell, pin,            "sc_pin");
    BRLS_BIND(brls::DetailCell, restrictions,   "sc_restrictions");
    BRLS_BIND(brls::DetailCell, temp,           "sc_temp");
    BRLS_BIND(brls::Header,     actions_header, "sc_actions_header");
    BRLS_BIND(brls::DetailCell, set_pin,        "sc_set_pin");
    BRLS_BIND(brls::DetailCell, pin_lock_cell,  "sc_pin_lock");
    BRLS_BIND(brls::Label,      pin_lock_note,  "sc_pin_lock_note");
    BRLS_BIND(brls::DetailCell, show_pin,       "sc_show_pin");
    BRLS_BIND(brls::DetailCell, unlock,         "sc_unlock");
    BRLS_BIND(brls::DetailCell, relock,         "sc_relock");
    BRLS_BIND(brls::Header,     lock_header,    "sc_lock_header");
    BRLS_BIND(brls::DetailCell, console_lock_cell, "sc_console_lock");
    BRLS_BIND(brls::Label,      console_lock_note, "sc_console_lock_note");
    BRLS_BIND(brls::DetailCell, pr_active,      "pr_active");
    BRLS_BIND(brls::DetailCell, pr_updated,     "pr_updated");
    BRLS_BIND(brls::DetailCell, pr_unlink,      "pr_unlink");
    BRLS_BIND(brls::Label,      pr_note,        "pr_note");
    BRLS_BIND(brls::Header,     danger_header,  "sc_danger_header");
    BRLS_BIND(brls::DetailCell, del,            "sc_delete");
};
