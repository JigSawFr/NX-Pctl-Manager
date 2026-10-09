// pure — the parts of the service layer that never talk to a console: the
// PlayTimerSettings block codec, the names of levels and rating bodies, the
// firmware policy. Compiled for the Switch, for the desktop simulator
// (source/sim/) and for the host tests, so all three share one implementation.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

// PlayTimerSettings (fw 21.0.0+): u16[34] (0x44 bytes), read with 145601 and
// written with 195101. Observed on a console with a real limit configured
// through the companion app (fw 22.1.0):
//   [0]   = 0x0101    observed header (non-zero <=> IsPlayTimerEnabled)
//   [1]   = 0x0001    ?
//   [2..6]= 0         reserved
//   7 per-day groups, group n at [7+4n .. 7+4n+3], Sun..Sat:
//     [+0] = 0x0600   ? (constant in the observed config)
//     [+1] = 0x0100   "this day has a configured limit" flag
//     [+2] = minutes  that day's limit
//     [+3] = 0        reserved (absent for the last group: the array stops at 34)
// A group left all-zero means "no limit that day".
//
// Read as bytes, the same block is a 12-byte header and seven 8-byte days
// (12 + 7 * 8 = 0x44), each the companion app's daily regulation (its
// "timeToPlayInOneDay" and "bedtime": enabled, endingTime = the alarm,
// startingTime = when play is allowed again, 06:00 by default):
//   day n at byte 12 + 8n:
//     +0 bedtime on   +1 alarm hour   +2 alarm minute
//     +3 allowed-again hour (the 0x06 of [+0])   +4 allowed-again minute
//     +5 limit flag (the 0x01 of [+1])           +6..7 limit minutes ([+2])
// so the "reserved" [6] and each [+3] hold the next day's bedtime switch and
// alarm hour. The bedtime fields come from that match (pt_bedtime_*), not
// from a console with a bedtime set: the write checks what the console then
// reports (1954/1956/1957) and puts the block back when it differs. The
// header's other bytes (the "alarm only" vs "suspend the software" choice, a
// daily mode) are still unknown, so a write keeps them as read (pt_encode).
#define PT_U16_COUNT 34

// Per-day value: no configured limit that day (pctl_ops.h uses the same).
#ifndef PT_DAY_NOLIMIT
#define PT_DAY_NOLIMIT 0xFFFFu
#endif

// Sun..Sat minutes, or PT_DAY_NOLIMIT for a day without a flag.
void pt_decode(const u16 c[PT_U16_COUNT], u16 days_min[7]);

// Turns the block read with 145601 into the one to write with 195101 for the
// per-day limits `days_min`, changing as little as possible:
//  - a day that keeps its limit gets the new minutes, nothing else changes;
//  - a day that gains a limit gets the observed 0x0600 / 0x0100 / minutes in
//    [+0..+2] (its [+3] stays);
//  - a day that loses its limit gets [+0..+2] cleared, the encoding seen
//    working on hardware (its [+3] stays);
//  - a day without a limit before and after is left exactly as read;
//  - the header stays as read, or gets the observed 0x0101 / 0x0001 when the
//    timer was off;
//  - no limit on any day: all zeros, the one "timer off" block seen working.
// So writing back the limits just read gives the same block, byte for byte.
void pt_encode(u16 c[PT_U16_COUNT], const u16 days_min[7]);
//  (A day whose bedtime is on keeps its times: only its limit flag and minutes
//  change, and "no limit on any day" keeps the block while a bedtime is on.)

// One day's bedtime (pure.h layout above): the alarm at hour:minute, play
// allowed again at end_hour:end_minute.
typedef struct {
    bool on;
    u8   hour, minute;
    u8   end_hour, end_minute;
} PtBedtime;

// The allowed range, as the companion app offers it: the alarm from 16:00 to
// 23:59, play allowed again from 05:00 to 09:00.
bool pt_bedtime_ok(const PtBedtime *b);

// Sun..Sat bedtimes as the block holds them.
void pt_bedtime_decode(const u16 c[PT_U16_COUNT], PtBedtime out[7]);

// Writes the seven bedtimes into the block read with 145601, changing only
// their bytes: a day turned off keeps its allowed-again time (as the
// companion app does) with the alarm cleared. A block that was off (header 0)
// gets the observed header when a bedtime is on; nothing on at all (no limit,
// no bedtime) gives all zeros, as pt_encode. Writing back the bedtimes just
// read gives the same block, byte for byte.
void pt_bedtime_encode(u16 c[PT_U16_COUNT], const PtBedtime in[7]);
