// OnboardingActivity — "First steps": set a PIN, choose a daily limit, check
// the network clock. Each line shows whether the step is done and does it on
// A (the system PIN screen, the same limit picker as the Play timer tab, the
// guided clock flow), plus unlinking the phone app while it is linked, the
// alarm while it is off, and, once a PIN is set, Ask for the PIN. Shown
// at start-up while no PIN is set (writable builds on a checked firmware)
// unless turned off here; Overview and Tools bring it back.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class OnboardingActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/onboarding.xml");

    void onContentAvailable() override;
    void willAppear(bool resetState = false) override;   // back from a step: the states again

    // Whether to show it by itself at start-up: no PIN yet, a build that can
    // set one (not read-only, checked firmware), and its switch left on.
    static bool wanted_at_start();

  private:
    bool clock_inaccurate = false;

    void refresh();

    BRLS_BIND(brls::Label,      headline, "ob_headline");
    BRLS_BIND(brls::DetailCell, pin,      "ob_pin");
    BRLS_BIND(brls::DetailCell, limit,    "ob_limit");
    BRLS_BIND(brls::DetailCell, clock,    "ob_clock");
    BRLS_BIND(brls::DetailCell, unlink,   "ob_unlink");
    BRLS_BIND(brls::DetailCell, alarm,    "ob_alarm");
    BRLS_BIND(brls::DetailCell, protect,  "ob_protect");
    BRLS_BIND(brls::Label,      note,     "ob_note");
    BRLS_BIND(brls::BooleanCell, at_start, "ob_at_start");
    BRLS_BIND(brls::DetailCell, close,    "ob_close");
    BRLS_BIND(brls::DetailCell, support,  "ob_support");
};
