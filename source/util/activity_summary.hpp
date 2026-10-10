// activity_summary — the Activity tab's summary of one account's play data
// (core/playstats.h) over the chosen period: the average per day, the most
// played game, the days played and the busiest one, the average session; and
// the games to rediscover (installed, little played, left aside for a while).
// Pure, so the host tests can run it (tests/activity_summary).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <vector>

#include "util/pctl_ops_c.hpp"

namespace activity_summary
{

enum Period { Today = 0, Week = 1, AllTime = 2 };

// A game's play time over `period` (all time: 0 when its totals were not read).
uint64_t value(const GameStat& g, int period);

struct Summary
{
    bool     known = false;   // the period's figures were read (nothing below is meaningful otherwise)
    uint64_t total_s = 0;     // every game added up over the period
    // Per day: the last 7 days over 7, all time over the days since the first
    // game was first played (today counting as one). 0 days: no average
    // (today, or all time with no first play known).
    uint64_t per_day_s = 0;
    int      days = 0;
    int      top = -1;        // index in PlayStats.games of the most played game, -1 none
    uint64_t top_s = 0;
    // Last 7 days only (-1 otherwise): the days with any play, and the day
    // (k days ago, see PlayStats.day_s) with the most.
    int      days_played = -1;
    int      busiest = -1;
    uint64_t busiest_s = 0;
    // All time only (0 otherwise, or with no launch counted): play time over launches.
    uint64_t session_s = 0;
};

Summary summarize(const PlayStats& stats, int period);

// Games to rediscover: still installed (a name), under 3 h in all and not
// played for 30 days or more, the least played first, at most `max`. Indices
// in PlayStats.games.
constexpr uint64_t REDISCOVER_MAX_S   = 3 * 3600;
constexpr uint64_t REDISCOVER_AFTER_S = 30 * 86400;
std::vector<int> rediscover(const PlayStats& stats, size_t max);

}   // namespace activity_summary
