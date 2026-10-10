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
#include "../util/playlog.h"

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
    u32  day_s[7];                   // same, per day: [0] today … [6] six days ago
} GameStat;

typedef struct {
    Result rc;          // listing the installed games (ns); nothing else is filled when it fails
    Result stats_rc;    // pdm:qry: the all-time totals
    Result events_rc;   // pdm:qry: the event log (today / 7 days)
    bool   windows_ok;  // today_s / week_s are meaningful
    u64    now;         // when the data was read (user clock)
    u8     day_wday[7]; // weekday (0 = Sunday) of day_s[k], when windows_ok
    u32    count;       // games with any play time, in no particular order
    GameStat games[PLAYSTATS_MAX];
} PlayStats;

// A user account of the console, as the Activity filter lists them.
typedef struct {
    u64  uid[2];           // AccountUid
    char nickname[0x21];   // UTF-8, as the HOME menu shows it
} PlayAccount;

// The console's user accounts (acc:u0), at most `max`. *rc says why none.
size_t playstats_accounts(PlayAccount *out, size_t max, Result *rc);

// Every account (`account` NULL), or one: today / 7 days / each day folded
// from the log with only the time that account was open in the game
// (util/playlog.h), all-time totals from its own statistics. Games it never
// played are left out.
void playstats_fetch_for(PlayStats *out, const PlayAccount *account);
void playstats_fetch(PlayStats *out);   // playstats_fetch_for(out, NULL)

// One game's all-time play time per user account on the console (pdm:qry by
// account, acc:u0 for the nicknames). Accounts that never played it are left
// out. Returns how many were written to `out`; *rc says why none when 0.
#define PLAYSTATS_MAX_ACCOUNTS 8
typedef struct {
    char nickname[0x21];   // UTF-8, as the HOME menu shows it
    u64  total_s;
    u32  launches;
} AccountPlay;

size_t playstats_by_account(u64 app_id, AccountPlay *out, size_t max, Result *rc);

// The games' icons, from the same control data as their names (one ns
// session for the whole list). For each entry, `jpeg` is malloc'ed (the
// caller frees it) and `size` set; both stay 0 for a game with no control
// data (deleted) or no icon. A JPEG is at most PLAYSTATS_ICON_MAX bytes.
#define PLAYSTATS_ICON_MAX 0x20000
typedef struct {
    u64            app_id;   // in
    unsigned char *jpeg;     // out
    size_t         size;     // out
} PlayIcon;

void playstats_icons(PlayIcon *icons, size_t count);
