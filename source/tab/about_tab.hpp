// AboutTab — the app: version (seven presses on it turn the developer tools
// on, at the end of Tools), launch mode and data folder; its updates (check
// now, once a day, with which store); what changed in this version (the
// bundled CHANGELOG.md); the credits; how to support it; "Made in France".
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"

class AboutTab : public TabBase
{
  public:
    AboutTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    int        version_presses    = 0;
    brls::Time last_version_press = 0;

    void count_version_press();
    void fill_changelog();

    BRLS_BIND(brls::DetailCell,  version,          "ab_version");
    BRLS_BIND(brls::DetailCell,  mode,             "ab_mode");
    BRLS_BIND(brls::DetailCell,  data,             "ab_data");
    BRLS_BIND(brls::DetailCell,  update_cell,      "ab_update");
    BRLS_BIND(brls::BooleanCell, update_daily,     "ab_update_daily");
    BRLS_BIND(brls::DetailCell,  update_via,       "ab_update_via");
    BRLS_BIND(brls::Header,      changelog_header, "ab_changelog_header");
    BRLS_BIND(brls::Box,         changelog,        "ab_changelog");
    BRLS_BIND(brls::Label,       changelog_note,   "ab_changelog_note");
    BRLS_BIND(brls::DetailCell,  author,           "ab_author");
    BRLS_BIND(brls::DetailCell,  upstream,         "ab_upstream");
    BRLS_BIND(brls::DetailCell,  fixes,            "ab_fixes");
    BRLS_BIND(brls::DetailCell,  ui_lib,           "ab_ui");
    BRLS_BIND(brls::DetailCell,  license,          "ab_license");
    BRLS_BIND(brls::DetailCell,  source,           "ab_source");
    BRLS_BIND(brls::Label,       support_note,     "ab_support_note");
    BRLS_BIND(brls::DetailCell,  sponsors,         "ab_sponsors");
    BRLS_BIND(brls::DetailCell,  kofi,             "ab_kofi");
};
