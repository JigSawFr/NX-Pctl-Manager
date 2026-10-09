// AboutTab — the version (seven presses on it turn the developer tools on,
// at the end of Tools), launch mode, data folder, license, source and
// credits, then the latest releases from the bundled CHANGELOG.md.
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

    BRLS_BIND(brls::DetailCell, version,        "ab_version");
    BRLS_BIND(brls::DetailCell, mode,           "ab_mode");
    BRLS_BIND(brls::DetailCell, data,           "ab_data");
    BRLS_BIND(brls::DetailCell, license,        "ab_license");
    BRLS_BIND(brls::DetailCell, source,         "ab_source");
    BRLS_BIND(brls::Label,      credits,        "ab_credits");
    BRLS_BIND(brls::Label,      changelog_note, "ab_changelog_note");
    BRLS_BIND(brls::Box,        changelog,      "ab_changelog");
};
