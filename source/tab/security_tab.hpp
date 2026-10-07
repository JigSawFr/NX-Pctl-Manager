// SecurityTab — PIN, temporary unlock / relock, delete everything.
// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
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
    BRLS_BIND(brls::DetailCell, unlock,         "sc_unlock");
    BRLS_BIND(brls::DetailCell, relock,         "sc_relock");
    BRLS_BIND(brls::Header,     danger_header,  "sc_danger_header");
    BRLS_BIND(brls::DetailCell, del,            "sc_delete");
};
