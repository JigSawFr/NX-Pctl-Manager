// TabBase — common shape of every sidebar tab: inflate the XML, re-read the
// state each time the tab (re)appears, X refreshes on demand, optional periodic
// refresh, and the read-only note (a Label with id "tab_ro_note", if present).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>

class TabBase : public brls::Box
{
  public:
    explicit TabBase(const std::string& xml_res);
    ~TabBase() override;

    // Called when the tab is shown and when the main screen comes back on top
    // (after the per-day editor, a dropdown, a dialog …).
    void willAppear(bool resetState = false) override;

    // Re-reads the tab on screen, if any (after the read-only or developer
    // mode changed).
    static void refresh_shown();

  protected:
    virtual void refresh() = 0;

    // Calls refresh() every `period_ms` while the app is in the foreground and
    // this tab's screen is the top one (not under a dialog or the per-day
    // editor). Tabs are deleted when another one is opened, which stops it.
    void enable_auto_refresh(int period_ms);

  private:
    brls::RepeatingTimer auto_timer;

    void update_ro_note();
};
