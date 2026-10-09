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
    PlayLogEv_Focus        = 0,   // `app_id` gets the focus
    PlayLogEv_Unfocus      = 1,   // `app_id` loses it, or exits
    PlayLogEv_Away         = 2,   // whatever game had the focus no longer has it
    PlayLogEv_AccountOpen  = 3,   // user account `uid` is selected in the game
    PlayLogEv_AccountClose = 4,   // user account `uid` is closed by the game
    PlayLogEv_Launch       = 5,   // a game starts: as Away, and the accounts the
                                  // previous one had open are closed with it
} PlayLogKind;

typedef struct {
    uint64_t app_id;      // 0 for PlayLogEv_Away and the account events
    uint8_t  kind;        // PlayLogKind
    uint64_t ts_user;     // POSIX seconds, user clock (what "today" refers to)
    uint64_t ts_steady;   // seconds, steady clock (0 when unknown)
    uint64_t uid[2];      // the account events' user account
} PlayLogEvent;

typedef struct {
    uint64_t app_id;
    uint32_t today_s;     // seconds played in [day_start, now)
    uint32_t week_s;      // seconds played in [week_start, now)
    uint32_t day_s[7];    // per day: [0] today, [1] yesterday … [6] six days ago
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

// A span of user-clock time that is not play time although a game had the
// focus: PlayGuard itself, started over that game (util/own_time.hpp).
typedef struct {
    uint64_t start, end;   // POSIX seconds, user clock, [start, end)
} PlayLogSpan;

// The same with every day of the window: `day_starts[k]` is when the day k
// days back began (strictly decreasing: [0] today's midnight, [6] the
// window's start). Days are taken as given, so a 23 or 25 h day (a
// daylight-saving change, calendar.h) is one day.
size_t playlog_fold_days(const PlayLogEvent *events, size_t n, uint64_t now,
                         const uint64_t day_starts[7], PlayLogTotal *out, size_t max);

// playlog_fold_days() without the time inside `skip` (n_skip spans, in any
// order, overlaps allowed): a session is cut around them.
size_t playlog_fold_days_skip(const PlayLogEvent *events, size_t n, uint64_t now,
                              const uint64_t day_starts[7], const PlayLogSpan *skip, size_t n_skip,
                              PlayLogTotal *out, size_t max);

// The log as one user account played it: a game counts for `uid` while it has
// the focus AND that account is open in it (selected when the game started,
// until the game closes it, or until another game is launched). The focus
// events are rewritten at the moments both hold, so playlog_fold_days() on the
// result gives that account's time. Several accounts open at once (local
// multiplayer) each get the whole time, as the system's own per-account
// statistics do. Returns the number of events written to `out` (at most `n`).
size_t playlog_for_account(const PlayLogEvent *events, size_t n, const uint64_t uid[2],
                           PlayLogEvent *out, size_t max);

#ifdef __cplusplus
}
#endif
