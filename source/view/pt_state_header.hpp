// PtStateHeader — the play-timer state in one line (active or not, time left,
// configured limit and the profile it matches, temporary unlock) at the top
// of the Play timer tab and the per-day editor, plus a warning line while the
// day's limit is reached, and one while the console is not counting play
// time (action/timer_health, fed by each show()). The five-row readout it
// replaces hid the week chart.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include "util/pctl_ops_c.hpp"

class PtStateHeader : public brls::Box
{
  public:
    PtStateHeader();

    void show(const PtState& pt);      // renders a state its screen read

    static brls::View* create();
    static std::string configured_text(const PtState& pt);

  private:
    BRLS_BIND(brls::Label, summary, "pt_summary");
    BRLS_BIND(brls::Label, alert,   "pt_alert");
    BRLS_BIND(brls::Label, health,  "pt_health");
};
