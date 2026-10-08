// PlayDaysView — play time on each of the last 7 days (every game, or one
// game's, all accounts or one): one bar per day, oldest on the left, today on
// the right and marked by a line under its name. With limits, each day's limit
// is a line across its bar, and a day played above it is drawn in amber.
// Read from the Activity tab's data.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include "util/pctl_ops_c.hpp"

class PlayDaysView : public brls::Box
{
  public:
    PlayDaysView();

    // `limits` (optional): minutes for each column's day, [0] today … [6] six
    // days back, PT_DAY_NOLIMIT for none.
    void show(const PlayStats& stats, const uint16_t limits[7] = nullptr);   // every game added up
    void show(const uint32_t day_s[7], const uint8_t day_wday[7],
              const uint16_t limits[7] = nullptr);                            // one game

    static brls::View* create();

  private:
    struct Column
    {
        brls::Label*     value = nullptr;
        brls::Rectangle* bar   = nullptr;
        brls::Rectangle* limit = nullptr;   // across the bar, at the day's limit
        brls::Label*     day   = nullptr;
        brls::Rectangle* mark  = nullptr;
    };
    Column cols[7];   // left to right: six days ago … today
};
