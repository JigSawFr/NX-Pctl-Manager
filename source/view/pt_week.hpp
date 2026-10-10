// PtWeekView — the week's limits at a glance (limits, not time played): one
// bar per day, Monday first; today is marked by a line under its name, not by
// colour alone; a day with no limit shows "—" and an empty slot.
// Made editable (set_on_pick + set_editable), each day takes the focus: ←/→
// move between days, A picks that day's limit. The editor rendering draws a
// day that differs from the console's value in the warning colour, with a
// "*" before it. A legend under the bars says the order and the "*".
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <functional>

#include "util/pctl_ops_c.hpp"

class PtWeekView : public brls::Box
{
  public:
    PtWeekView();

    // The console's limits, as read.
    void show(const PtState& pt);
    // An editor's limits: `days` as they will be, `live` as the console has
    // them (a day that differs is drawn as unsaved).
    void show(const uint16_t days[7], const uint16_t live[7]);

    // A on a day runs `on_pick(day)` (0 = Sunday) while editable.
    void set_on_pick(std::function<void(int)> on_pick);
    // Whether the days take the focus (today first). Off: a plain chart.
    void set_editable(bool editable);

    static brls::View* create();

  private:
    struct Column
    {
        brls::Box*       box   = nullptr;
        brls::Label*     value = nullptr;
        brls::Rectangle* bar   = nullptr;
        brls::Label*     day   = nullptr;
        brls::Rectangle* mark  = nullptr;   // under today's name
    };
    Column cols[7];   // Sunday..Saturday (laid out Monday first)
    brls::Box*   row    = nullptr;   // the seven days
    brls::Label* legend = nullptr;   // under the bars: the order, the "*"
    std::function<void(int)> on_pick;
    bool editable = false;

    void render(const uint16_t days[7], const uint16_t* live);
};
