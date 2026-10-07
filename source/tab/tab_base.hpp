// TabBase — common shape of every sidebar tab: inflate the XML, re-read the
// state each time the tab (re)appears, X refreshes on demand.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>

class TabBase : public brls::Box
{
  public:
    explicit TabBase(const std::string& xml_res);

    // Called when the tab is shown and when the main screen comes back on top
    // (after the per-day editor, a dropdown, a dialog …).
    void willAppear(bool resetState = false) override;

  protected:
    virtual void refresh() = 0;
};
