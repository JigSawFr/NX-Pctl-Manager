// ProfilesActivity — saved play-time profiles (e.g. "School week"). A on one:
// apply it (through the play-timer write flow), edit its limits or rename it
// (nothing is written to the console), delete it; Y deletes it at once. Below
// the list: save the current limits as a profile, or make a new one from
// scratch. The list is rebuilt after every change.
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

    void rebuild(const std::string& focus_file = "");
    void actions(const profiles::Profile& p);
    void apply(const profiles::Profile& p);
    void edit_limits(const profiles::Profile& p);
    void rename(const profiles::Profile& p);
    void remove(const profiles::Profile& p);
    void save_current();
    void create();
    // Asks for a name (pre-filled with `initial`), checks it can be a file
    // name and asks before replacing another profile; `except_file` is the
    // profile being renamed. Then `done(name)`.
    void ask_name(const std::string& initial, const std::string& except_file, std::function<void(std::string)> done);
    // Writes `p`, says how it went, lists again with it focused.
    bool store(const profiles::Profile& p, const std::string& ok_text);

    BRLS_BIND(brls::Box,        list,      "pf_list");
    BRLS_BIND(brls::Label,      empty,     "pf_empty");
    BRLS_BIND(brls::Label,      hint,      "pf_hint");
    BRLS_BIND(brls::DetailCell, save_cell, "pf_save");
    BRLS_BIND(brls::DetailCell, new_cell,  "pf_new");
};
