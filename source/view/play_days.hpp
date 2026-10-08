// PlayDaysView — play time on each of the last 7 days (every game, or one
// game's, all accounts): one bar per day, oldest on the left, today on the
// right and marked by a line under its name. Read from the Activity tab's data.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include "util/pctl_ops_c.hpp"

class PlayDaysView : public brls::Box
{
  public:
    PlayDaysView();

    void show(const PlayStats& stats);                             // every game added up
    void show(const uint32_t day_s[7], const uint8_t day_wday[7]);  // one game ([0] today … [6])

    static brls::View* create();

  private:
    struct Column
    {
        brls::Label*     value = nullptr;
        brls::Rectangle* bar   = nullptr;
        brls::Label*     day   = nullptr;
        brls::Rectangle* mark  = nullptr;
    };
    Column cols[7];   // left to right: six days ago … today
};
