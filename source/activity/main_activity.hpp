// MainActivity — root screen: a TabFrame (sidebar of tabs) described in
// resources/xml/activity/main.xml. The tabs live in source/tab/.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class MainActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/main.xml");

    void onContentAvailable() override;

    // "PlayGuard", or "PlayGuard · read-only" in read-only mode.
    void update_title();

  private:
    // Midnight while the app is open: what start-up does for a new day
    // (extra time from the day before) is done then too.
    brls::RepeatingTimer day_timer;
    std::string day;
};
