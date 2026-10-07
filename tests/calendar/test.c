// Host tests for source/core/calendar.c: civil dates, and the start of a local
// day around daylight-saving changes (a made-up Paris-like rule: UTC+1, UTC+2
// from the last Sunday of March to the last Sunday of October, 01:00 UTC).
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "calendar.h"

typedef struct {
    u64 change[4];   // UTC instants the offset changes at, ascending
    s64 offset[5];   // offset before change[0], after change[0], ...
    int changes;
} FakeZone;

static s64 offset_at(const FakeZone *z, u64 posix)
{
    int i = 0;
    while (i < z->changes && posix >= z->change[i]) i++;
    return z->offset[i];
}

static u64 utc(int y, unsigned m, unsigned d, unsigned h, unsigned mi)
{
    return (u64)(calendar_days_from_civil(y, m, d) * 86400 + h * 3600 + mi * 60);
}

static bool fake_to_local(void *ctx, u64 posix, LocalTime *out)
{
    const s64 wall = (s64)posix + offset_at((const FakeZone *)ctx, posix);
    const s64 days = wall / 86400;
    int y;
    unsigned m, d;
    calendar_civil_from_days(days, &y, &m, &d);
    const s64 rest = wall - days * 86400;
    out->year = (u16)y; out->month = (u8)m; out->day = (u8)d;
    out->hour = (u8)(rest / 3600); out->minute = (u8)(rest / 60 % 60); out->second = (u8)(rest % 60);
    out->wday = (u8)calendar_weekday(days);
    return true;
}

static int fake_to_posix(void *ctx, const LocalTime *w, u64 out[2])
{
    const FakeZone *z = ctx;
    const s64 wall = calendar_days_from_civil(w->year, w->month, w->day) * 86400 + w->hour * 3600 + w->minute * 60 + w->second;
    int n = 0;
    for (int i = 0; i <= z->changes && n < 2; i++) {
        const u64 p = (u64)(wall - z->offset[i]);
        if (offset_at(z, p) == z->offset[i] && (n == 0 || out[0] != p)) out[n++] = p;
    }
    if (n == 2 && out[1] < out[0]) { u64 t = out[0]; out[0] = out[1]; out[1] = t; }
    return n;
}

static FakeZone paris(void)
{
    FakeZone z;
    memset(&z, 0, sizeof(z));
    z.change[0] = utc(2026, 3, 29, 1, 0);    // 02:00 -> 03:00
    z.change[1] = utc(2026, 10, 25, 1, 0);   // 03:00 -> 02:00
    z.offset[0] = 3600; z.offset[1] = 7200; z.offset[2] = 3600;
    z.changes = 2;
    return z;
}

static void test_civil(void)
{
    assert(calendar_days_from_civil(1970, 1, 1) == 0);
    assert(calendar_weekday(0) == 4);                                      // Thursday
    assert(calendar_weekday(calendar_days_from_civil(2026, 10, 7)) == 3);  // Wednesday
    assert(calendar_weekday(calendar_days_from_civil(1969, 12, 28)) == 0); // Sunday, before 1970
    int y; unsigned m, d;
    calendar_civil_from_days(calendar_days_from_civil(2000, 2, 29), &y, &m, &d);
    assert(y == 2000 && m == 2 && d == 29);
    for (s64 z = -800; z < 30000; z += 37) {   // round trip
        calendar_civil_from_days(z, &y, &m, &d);
        assert(calendar_days_from_civil(y, m, d) == z);
    }

    LocalTime t = { 2026, 1, 3, 15, 4, 5, 0 };
    calendar_add_days(&t, -6);                 // into the previous year
    assert(t.year == 2025 && t.month == 12 && t.day == 28 && t.wday == 0 && t.hour == 15);
    LocalTime leap = { 2024, 2, 28, 0, 0, 0, 0 };
    calendar_add_days(&leap, 1);
    assert(leap.month == 2 && leap.day == 29 && leap.wday == 4);
    calendar_add_days(&leap, 1);
    assert(leap.month == 3 && leap.day == 1);
}

static void test_midnight(void)
{
    FakeZone z = paris();
    const TimeRule rule = { fake_to_local, fake_to_posix, &z };

    // An ordinary day (summer time): local 14:03 on 7 October.
    u64 now = utc(2026, 10, 7, 12, 3);
    assert(local_midnight(&rule, now, 0) == utc(2026, 10, 6, 22, 0));
    assert(local_midnight(&rule, now, 6) == utc(2026, 9, 30, 22, 0));

    // The clocks go forward at 02:00: at 10:00 local only 9 h have passed
    // since midnight. "now - 10 h" would start the day an hour early.
    now = utc(2026, 3, 29, 8, 0);
    assert(local_midnight(&rule, now, 0) == utc(2026, 3, 28, 23, 0));

    // The clocks go back at 03:00: 11 h since midnight at 10:00 local.
    now = utc(2026, 10, 25, 9, 0);
    assert(local_midnight(&rule, now, 0) == utc(2026, 10, 24, 22, 0));

    // "Last 7 days" across the change: 6 calendar days back is 143 h before
    // today's midnight, not 144.
    now = utc(2026, 4, 2, 10, 0);
    const u64 today = local_midnight(&rule, now, 0);
    const u64 week = local_midnight(&rule, now, 6);
    assert(today == utc(2026, 4, 1, 22, 0));
    assert(week == utc(2026, 3, 26, 23, 0));
    assert(today - week == 143 * 3600);

    // Just after midnight and just before.
    assert(local_midnight(&rule, utc(2026, 10, 6, 22, 0), 0) == utc(2026, 10, 6, 22, 0));
    assert(local_midnight(&rule, utc(2026, 10, 6, 21, 59), 0) == utc(2026, 10, 5, 22, 0));
}

static void test_midnight_skipped(void)
{
    // A zone moving its clocks from 00:00 to 01:00 (UTC-3 -> UTC-2): that day
    // has no midnight and begins at 01:00, the instant of the change.
    FakeZone z;
    memset(&z, 0, sizeof(z));
    z.change[0] = utc(2026, 9, 6, 3, 0);
    z.offset[0] = -3 * 3600; z.offset[1] = -2 * 3600;
    z.changes = 1;
    const TimeRule rule = { fake_to_local, fake_to_posix, &z };
    assert(local_midnight(&rule, utc(2026, 9, 6, 15, 0), 0) == utc(2026, 9, 6, 3, 0));
    // The day before, an ordinary midnight.
    assert(local_midnight(&rule, utc(2026, 9, 6, 15, 0), 1) == utc(2026, 9, 5, 3, 0));
}

static void test_no_rule(void)
{
    const u64 now = utc(2026, 10, 7, 12, 3);
    assert(local_midnight(NULL, now, 0) == utc(2026, 10, 7, 0, 0));   // UTC midnight
    assert(local_midnight(NULL, now, 2) == utc(2026, 10, 5, 0, 0));
    const TimeRule broken = { NULL, NULL, NULL };
    assert(local_midnight(&broken, now, 0) == utc(2026, 10, 7, 0, 0));
}

int main(void)
{
    test_civil();
    test_midnight();
    test_midnight_skipped();
    test_no_rule();
    puts("calendar dates and daylight-saving day-start assertions passed");
    return 0;
}
