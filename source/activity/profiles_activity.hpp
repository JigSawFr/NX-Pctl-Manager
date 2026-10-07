// ProfilesActivity — saved play-time profiles (e.g. "School week"): A applies
// one through the play-timer write flow, X deletes it, the last cell saves the
// current limits as a new profile. The list is rebuilt after every change.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>

#include "util/pctl_ops_c.hpp"
#include "util/profiles.hpp"

class ProfilesActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/profiles.xml");

    void onContentAvailable() override;

  private:
    PtState live = {};

    void rebuild(const std::string& focus_name = "");
    void apply(const profiles::Profile& p);
    void remove(const std::string& name);
    void save_current();

    BRLS_BIND(brls::Box,        list,      "pf_list");
    BRLS_BIND(brls::Label,      empty,     "pf_empty");
    BRLS_BIND(brls::Label,      hint,      "pf_hint");
    BRLS_BIND(brls::DetailCell, save_cell, "pf_save");
};
