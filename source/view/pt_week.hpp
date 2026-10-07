// PtWeekView — the week's limits at a glance (limits, not time played): one
// bar per day, Monday first; today is marked by a line under its name, not by
// colour alone; a day with no limit shows "—" and an empty slot.
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
        brls::Rectangle* mark  = nullptr;   // under today's name
    };
    Column cols[7];   // Sunday..Saturday (laid out Monday first)
};
