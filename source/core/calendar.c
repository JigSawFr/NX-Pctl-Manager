// PlayGuard — local dates (see calendar.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "calendar.h"

#define DAY_S 86400u

// Howard Hinnant's days_from_civil / civil_from_days (public domain).
s64 calendar_days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const s64 era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (s64)doe - 719468;
}

void calendar_civil_from_days(s64 z, int *year, unsigned *month, unsigned *day)
{
    z += 719468;
    const s64 era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;
    *year = (int)((s64)yoe + era * 400 + (m <= 2));
    *month = m;
    *day = d;
}

int calendar_weekday(s64 days)
{
    // 1970-01-01 was a Thursday.
    const int w = (int)((days + 4) % 7);
    return w < 0 ? w + 7 : w;
}

void calendar_add_days(LocalTime *t, int days)
{
    const s64 z = calendar_days_from_civil(t->year, t->month, t->day) + days;
    int y;
    unsigned m, d;
    calendar_civil_from_days(z, &y, &m, &d);
    t->year = (u16)y;
    t->month = (u8)m;
    t->day = (u8)d;
    t->wday = (u8)calendar_weekday(z);
}

static u64 earliest(const u64 *c, int n)
{
    u64 best = c[0];
    for (int i = 1; i < n && i < 2; i++) if (c[i] < best) best = c[i];
    return best;
}

u64 local_midnight(const TimeRule *rule, u64 now, int days_back)
{
    const u64 back = (u64)(days_back > 0 ? days_back : 0) * DAY_S;
    const u64 utc_day = now - now % DAY_S;
    const u64 fallback = utc_day >= back ? utc_day - back : 0;

    LocalTime t;
    if (!rule || !rule->to_local || !rule->to_posix || !rule->to_local(rule->ctx, now, &t)) return fallback;
    calendar_add_days(&t, -(days_back > 0 ? days_back : 0));
    t.hour = t.minute = t.second = 0;

    u64 c[2] = { 0, 0 };
    int n = rule->to_posix(rule->ctx, &t, c);
    if (n > 0) return earliest(c, n);

    // Midnight did not exist that day (a zone that moves its clocks at 00:00):
    // the day began at the first wall time that did, within the next hours.
    for (u8 h = 1; h <= 3; h++) {
        LocalTime later = t;
        later.hour = h;
        n = rule->to_posix(rule->ctx, &later, c);
        if (n > 0) return earliest(c, n);   // the instant of the jump
    }
    return fallback;
}
