// PlayGuard — play time per game, read from the system play-data service
// (pdm:qry) for every account on the console: all-time totals
// (QueryPlayStatisticsByApplicationId) and today / last 7 days, folded from the
// play-event log (QueryPlayEvent, see util/playlog.h). Names come from each
// game's control data (ns). Read-only: nothing here changes the console.
//
// Like the pctl layer, every service is opened for the fetch and closed before
// it returns. A fetch reads the whole event log and every game's name the
// first time, so the UI runs it off the main thread.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

#define PLAYSTATS_MAX      256
#define PLAYSTATS_NAME_LEN 128

typedef struct {
    u64  app_id;
    char name[PLAYSTATS_NAME_LEN];   // UTF-8, console language; empty when unknown

    bool totals_ok;                  // the all-time statistics below were read
    u64  total_s;                    // all-time play time, seconds
    u32  launches;
    u64  first_played, last_played;  // POSIX (user clock), 0 when unknown

    u32  today_s, week_s;            // valid when PlayStats.windows_ok
} GameStat;

typedef struct {
    Result rc;          // listing the installed games (ns); nothing else is filled when it fails
    Result stats_rc;    // pdm:qry: the all-time totals
    Result events_rc;   // pdm:qry: the event log (today / 7 days)
    bool   windows_ok;  // today_s / week_s are meaningful
    u64    now;         // when the data was read (user clock)
    u32    count;       // games with any play time, in no particular order
    GameStat games[PLAYSTATS_MAX];
} PlayStats;

void playstats_fetch(PlayStats *out);
