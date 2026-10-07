// calendar — local dates without the C library's time zone: civil date
// arithmetic, and the start of a local day through a time-zone rule given as
// two converters (the console's rule in time_ops.c, a host-side one in the
// simulator, a made-up one in the tests). Console-free, like pure.c.
//
// Why not localtime(): libnx's time() is the clock read at start-up plus the
// ticks since (a clock set while the app runs is not seen), and its time zone
// is an offset fixed at start-up (no daylight-saving change while it runs).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

typedef struct {
    u16 year;
    u8  month;    // 1..12
    u8  day;      // 1..31
    u8  hour, minute, second;
    u8  wday;     // 0 = Sunday
} LocalTime;

typedef struct {
    // `posix` as local wall time; false when the rule cannot say.
    bool (*to_local)(void *ctx, u64 posix, LocalTime *out);
    // The POSIX times showing wall time `wall` (its wday is ignored): 1 as a
    // rule, 2 when the clocks go back (the hour repeats), 0 when they go
    // forward over it (the hour does not exist). Writes at most 2.
    int  (*to_posix)(void *ctx, const LocalTime *wall, u64 out[2]);
    void *ctx;
} TimeRule;

// Days since 1970-01-01 of a proleptic Gregorian date, and back.
s64  calendar_days_from_civil(int year, unsigned month, unsigned day);
void calendar_civil_from_days(s64 days, int *year, unsigned *month, unsigned *day);
// 0 = Sunday.
int  calendar_weekday(s64 days);
// Moves the date of `t` by `days` (its time of day is kept, wday follows).
void calendar_add_days(LocalTime *t, int days);

// The POSIX time at which the local day `days_back` days before the one
// holding `now` began: its midnight, or its first instant when the clocks
// jumped over midnight. Daylight-saving changes in between are counted, so a
// "7 days" window is 7 calendar days, 167 or 169 hours around a change.
// Without a usable rule: `now` minus the time of day in UTC.
u64  local_midnight(const TimeRule *rule, u64 now, int days_back);
