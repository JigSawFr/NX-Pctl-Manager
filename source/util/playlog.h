// playlog — play time per game for today and the last 7 days, folded from the
// console's play-event log (pdm:qry QueryPlayEvent). Plain C with no libnx, so
// the host tests can feed it made-up logs; source/core/playstats.c reads the
// real one.
//
// How the log is read: a game is played from the moment it gets the focus
// until it loses it (out of focus, exit), or until something else takes over
// without that being logged for the game (the HOME menu gets the focus after a
// crash, the console sleeps or shuts down, another game starts or gets the
// focus). Durations come from the steady clock, so changing the console clock
// (Network clock tab) does not stretch them; the user clock only places them
// on the calendar.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PlayLogEv_Focus   = 0,   // `app_id` gets the focus
    PlayLogEv_Unfocus = 1,   // `app_id` loses it, or exits
    PlayLogEv_Away    = 2,   // whatever game had the focus no longer has it
} PlayLogKind;

typedef struct {
    uint64_t app_id;      // 0 for PlayLogEv_Away
    uint8_t  kind;        // PlayLogKind
    uint64_t ts_user;     // POSIX seconds, user clock (what "today" refers to)
    uint64_t ts_steady;   // seconds, steady clock (0 when unknown)
} PlayLogEvent;

typedef struct {
    uint64_t app_id;
    uint32_t today_s;     // seconds played in [day_start, now)
    uint32_t week_s;      // seconds played in [week_start, now)
} PlayLogTotal;

// A session longer than this is a broken log (missing events), not play time.
#define PLAYLOG_MAX_SESSION_S (24u * 3600u)

// Folds `events` (oldest first) into one total per game with time in the
// week window. `now` is the current user-clock time; a game still in focus at
// the end of the log counts until then. Returns the number of totals written
// to `out` (games beyond `max` are dropped).
size_t playlog_fold(const PlayLogEvent *events, size_t n, uint64_t now,
                    uint64_t day_start, uint64_t week_start,
                    PlayLogTotal *out, size_t max);

#ifdef __cplusplus
}
#endif
