// RestrictionsTab — restriction level, what the chosen preset imposes (age
// rating, social media posting, communication; read-only) or the same three
// settings editable in Custom, VR mode. Same settings as System Settings.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"
#include "util/pctl_ops_c.hpp"

class RestrictionsTab : public TabBase
{
  public:
    RestrictionsTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    PctlStatus st = {};

    void write_custom(const PctlCustomSettings& s);
    // "Restrict software by age rating: Up to 12 years (PEGI)" and the two others.
    std::string settings_text(const PctlCustomSettings& s) const;

    BRLS_BIND(brls::DetailCell,  level,         "rs_level");
    BRLS_BIND(brls::Header,      custom_header, "rs_custom_header");
    BRLS_BIND(brls::DetailCell,  preset_age,    "rs_preset_age");
    BRLS_BIND(brls::DetailCell,  preset_sns,    "rs_preset_sns");
    BRLS_BIND(brls::DetailCell,  preset_comm,   "rs_preset_comm");
    BRLS_BIND(brls::DetailCell,  age,           "rs_age");
    BRLS_BIND(brls::BooleanCell, sns,           "rs_sns");
    BRLS_BIND(brls::BooleanCell, comm,          "rs_comm");
    BRLS_BIND(brls::Label,       custom_note,   "rs_custom_note");
    BRLS_BIND(brls::BooleanCell, vr,            "rs_vr");
    BRLS_BIND(brls::DetailCell,  org,           "rs_org");
    BRLS_BIND(brls::DetailCell,  free_comm,     "rs_free_comm");
    BRLS_BIND(brls::Label,       note,          "rs_note");
};
