// RescueActivity — the recovery screen (action/rescue.hpp). Shown at start-up,
// before anything else, when the playguard-rescue sysmodule reports it acted
// on a RESCUE file: it says what happened and lets the parent show or reset
// the PIN or delete every parental control, then continue to PlayGuard.
//
// While it is on screen the PIN-before-a-change check is off: the parent
// reached here by proving they can edit the SD card (the same authority that
// turns PlayGuard's own PIN prompt off), and they came here because they
// forgot the PIN. Proceeding installs the normal gate and opens the app.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include "action/rescue.hpp"

class RescueActivity : public brls::Activity
{
  public:
    explicit RescueActivity(RescueReport report);

    CONTENT_FROM_XML_RES("activity/rescue.xml");

    void onContentAvailable() override;
    void willAppear(bool resetState = false) override;

  private:
    RescueReport report;

    void refresh();
    void proceed();

    BRLS_BIND(brls::Label,      outcome,   "rc_outcome");
    BRLS_BIND(brls::Label,      note,      "rc_note");
    BRLS_BIND(brls::DetailCell, show_pin,  "rc_show_pin");
    BRLS_BIND(brls::DetailCell, reset_pin, "rc_reset_pin");
    BRLS_BIND(brls::DetailCell, del,       "rc_delete");
    BRLS_BIND(brls::DetailCell, cont,      "rc_continue");
};
