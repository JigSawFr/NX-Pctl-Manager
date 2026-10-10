// RescueActivity — the recovery screen (action/rescue.hpp). Shown at start-up,
// before anything else, when the playguard-rescue sysmodule reports it acted
// on a RESCUE file: it says what happened and lets the parent show or reset
// the PIN or delete every parental control, then continue to PlayGuard.
//
// While it is on screen the PIN-before-a-change check is off, but only for a
// report the console confirms (rescue::confirmed): the sysmodule really
// unlocked or deleted, which needs it installed and the console restarted.
// The parent came here because they forgot the PIN. A report the console does
// not confirm (written by hand, or a recovery that failed) is only shown: no
// action is offered and "Open PlayGuard" goes through the usual lock screen.
// Proceeding installs the normal gate and opens the app.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include "action/rescue.hpp"

class RescueActivity : public brls::Activity
{
  public:
    RescueActivity(RescueReport report, bool confirmed);

    CONTENT_FROM_XML_RES("activity/rescue.xml");

    void onContentAvailable() override;
    void willAppear(bool resetState = false) override;

  private:
    RescueReport report;
    bool confirmed;

    void refresh();
    void proceed();

    BRLS_BIND(brls::Label,      outcome,   "rc_outcome");
    BRLS_BIND(brls::Label,      note,      "rc_note");
    BRLS_BIND(brls::Header,     actions_header, "rc_actions_header");
    BRLS_BIND(brls::DetailCell, show_pin,  "rc_show_pin");
    BRLS_BIND(brls::DetailCell, reset_pin, "rc_reset_pin");
    BRLS_BIND(brls::DetailCell, del,       "rc_delete");
    BRLS_BIND(brls::DetailCell, cont,      "rc_continue");
};
