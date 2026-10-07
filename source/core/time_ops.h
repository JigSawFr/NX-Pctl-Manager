// time_ops — read the system clocks and write the network clock (time:s).
// All handles are acquired and released within each call.
// Adapted from anbingxi/NX-Pctl-Manager (diag/fw22-5-readonly).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

typedef struct {
    Result service_rc, user_rc, network_rc, local_rc, automatic_rc, accuracy_rc, location_rc;
    u64  user_time, network_time, local_time;     // POSIX seconds (UTC)
    bool automatic;   // 100 IsStandardUserSystemClockAutomaticCorrectionEnabled
    bool accuracy;    // 200 IsStandardNetworkSystemClockAccuracySufficient
    char location[0x25];                          // time zone, e.g. "Europe/Paris"
} TimeSnapshot;

typedef struct {
    TimeSnapshot before, after;
    bool   refused_automatic, write_attempted, verify_attempted, verified;
    Result open_rc, write_rc, verify_rc;
    u64    readback;
} TimeApply;

void time_clock_snapshot(TimeSnapshot *out);
// Writes utc_seconds to the standard network clock. Refuses (refused_automatic)
// when automatic correction is disabled or unknown: the user clock would then
// not follow, and the user asked for the system's own setting to stay in charge.
void time_clock_apply(u64 utc_seconds, TimeApply *out);
void time_clock_dump(char *buf, size_t size);

// Formats a POSIX time in the console's configured time zone
// ("2026-10-07 14:03:12"). Falls back to UTC (with a " UTC" suffix) on failure.
void time_format_local(u64 posix, char *buf, size_t size);
void time_format_utc(u64 posix, char *buf, size_t size);
