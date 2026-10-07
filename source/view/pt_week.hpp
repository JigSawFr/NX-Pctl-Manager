// PtWeekView — the week's limits at a glance: one bar per day, Monday first,
// today highlighted; "—" and a faint full bar for a day with no limit.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include "util/pctl_ops_c.hpp"

class PtWeekView : public brls::Box
{
  public:
    PtWeekView();

    void show(const PtState& pt);

    static brls::View* create();

  private:
    struct Column
    {
        brls::Label*     value = nullptr;
        brls::Rectangle* bar   = nullptr;
        brls::Label*     day   = nullptr;
    };
    Column cols[7];   // Sunday..Saturday (laid out Monday first)
};
