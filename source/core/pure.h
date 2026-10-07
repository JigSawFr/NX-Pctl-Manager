// pure — the parts of the service layer that never talk to a console: the
// PlayTimerSettings block codec, the names of levels and rating bodies, the
// firmware policy. Compiled for the Switch, for the desktop simulator
// (source/sim/) and for the host tests, so all three share one implementation.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

// PlayTimerSettings (fw 21.0.0+): u16[34] (0x44 bytes), read with 145601 and
// written with 195101. Layout decoded from a console with a real limit
// configured through the companion app (fw 22.1.0):
//   [0]   = 0x0101    observed header (non-zero <=> IsPlayTimerEnabled)
//   [1]   = 0x0001    ?
//   [2..6]= 0         reserved
//   7 per-day groups, group n at [7+4n .. 7+4n+3], Sun..Sat:
//     [+0] = 0x0600   ? (constant in the observed config)
//     [+1] = 0x0100   "this day has a configured limit" flag
//     [+2] = minutes  that day's limit
//     [+3] = 0        reserved (absent for the last group: the array stops at 34)
// A group left all-zero means "no limit that day".
// Only the flag and the minutes are understood; the other fields (the header,
// [+0], [+3], [2..6]) may hold what the companion app sets and this app does
// not show (bedtime, "alarm only" vs "suspend the software"), so a write keeps
// them as read (pt_encode).
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
