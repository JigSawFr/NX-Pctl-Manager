// Host tests for source/action/outside_change_logic.cpp: a time spent that
// starts over, a clock changed, limits changed outside PlayGuard, and the
// watch.json record they are kept in.
#include "check.h"
#include <cstring>

#include "action/outside_change_logic.hpp"

using namespace outside_change_logic;

static Observation reading(const char* date, const char* hm, int64_t spent, int64_t offset, uint16_t limit = 120)
{
    Observation ob;
    ob.date = date;
    ob.hm = hm;
    ob.spent_known = spent >= 0;
    ob.spent_s = spent;
    ob.clock_known = true;
    ob.offset_s = offset;
    ob.steady_id = "aa";
    ob.limits_known = true;
    for (auto& d : ob.limits) d = limit;
    return ob;
}

static bool nothing(const Findings& f) { return !f.reset && !f.clock && !f.limits; }

static void test_first_and_steady()
{
    Record rec;
    Findings f = check(rec, reading("2026-10-08", "16:00", 600, 1000));
    CHECK(nothing(f) && f.save);   // a first reading only records
    CHECK(rec.date == "2026-10-08" && rec.spent_s == 600 && rec.offset_known && rec.limits_known);
    // Counting on, clock and limits unchanged: nothing, and no write for
    // less than SPENT_SAVE_S more.
    f = check(rec, reading("2026-10-08", "16:01", 660, 1002));
    CHECK(nothing(f) && !f.save);
    f = check(rec, reading("2026-10-08", "16:05", 600 + SPENT_SAVE_S, 1003));
    CHECK(nothing(f) && f.save);
    CHECK(rec.saved_spent_s == 600 + SPENT_SAVE_S);
    // A drift of a minute: written, no notice.
    f = check(rec, reading("2026-10-08", "16:06", 960, 1000 + OFFSET_SAVE_S + 5));
    CHECK(nothing(f) && f.save && rec.offset_s == 1000 + OFFSET_SAVE_S + 5);
    CHECK(rec.notice == 0);
}

static void test_reset()
{
    Record rec;
    check(rec, reading("2026-10-08", "16:00", 3000, 1000));
    // Down by less than the slack: not a reset.
    Findings f = check(rec, reading("2026-10-08", "16:01", 3000 - SPENT_SLACK_S, 1000));
    CHECK(!f.reset);
    // Down to 0 on the same day: a reset, at 16:02.
    f = check(rec, reading("2026-10-08", "16:02", 0, 1000));
    CHECK(f.reset && !f.clock && f.save && f.spent_before_s == 3000 - SPENT_SLACK_S);
    CHECK(rec.notice == NOTICE_RESET && rec.reset_at == "16:02" && rec.notice_date == "2026-10-08");
    // Seen once: counting from 0 again is not another one.
    f = check(rec, reading("2026-10-08", "16:03", 60, 1000));
    CHECK(nothing(f));
    // A new day: the console's own reset, and yesterday's notice goes.
    f = check(rec, reading("2026-10-09", "08:00", 0, 1000));
    CHECK(nothing(f) && f.save && rec.notice == 0 && rec.reset_at.empty());
    // Spent unknown (no limit today, nothing counted yet): nothing compared.
    rec = Record();
    check(rec, reading("2026-10-08", "16:00", 3000, 1000));
    f = check(rec, reading("2026-10-08", "16:01", -1, 1000));
    CHECK(nothing(f) && rec.spent_s == 3000);
    f = check(rec, reading("2026-10-08", "16:02", 10, 1000));
    CHECK(f.reset);
}

static void test_clock()
{
    Record rec;
    check(rec, reading("2026-10-08", "20:10", 5772, 1000));
    // The clock moved back 1387 s (PlayGuard's measurement): the clock, and
    // the time spent back to 0 with it.
    Findings f = check(rec, reading("2026-10-08", "19:47", 0, 1000 - 1387));
    CHECK(f.clock && f.reset && f.clock_moved_s == -1387);
    CHECK(rec.notice == (NOTICE_CLOCK | NOTICE_RESET));
    // Exactly the slack is not a change; more is.
    rec = Record();
    check(rec, reading("2026-10-08", "20:10", -1, 1000));
    CHECK(!check(rec, reading("2026-10-08", "20:10", -1, 1000 + CLOCK_SLACK_S)).clock);
    rec = Record();
    check(rec, reading("2026-10-08", "20:10", -1, 1000));
    f = check(rec, reading("2026-10-08", "20:16", -1, 1000 + CLOCK_SLACK_S + 1));
    CHECK(f.clock && f.clock_moved_s == CLOCK_SLACK_S + 1);
    // Another steady clock source: no comparison, only recorded.
    Observation ob = reading("2026-10-08", "20:20", -1, 999999);
    ob.steady_id = "bb";
    f = check(rec, ob);
    CHECK(!f.clock && f.save && rec.steady_id == "bb" && rec.offset_s == 999999);
    // Clock unreadable: kept as it was.
    ob = reading("2026-10-08", "20:21", -1, 5);
    ob.steady_id = "bb";
    ob.clock_known = false;
    CHECK(!check(rec, ob).clock && rec.offset_s == 999999);
}

static void test_limits()
{
    Record rec;
    check(rec, reading("2026-10-08", "16:00", 60, 1000, 120));
    Findings f = check(rec, reading("2026-10-08", "16:01", 120, 1000, 90));
    CHECK(f.limits && !f.reset && !f.clock && f.save);
    CHECK(f.limits_before[0] == 120 && f.limits_after[6] == 90);
    CHECK(rec.notice == NOTICE_LIMITS && rec.limits[3] == 90);
    // Once: the new limits are now the known ones.
    CHECK(!check(rec, reading("2026-10-08", "16:02", 180, 1000, 90)).limits);
    // Unreadable limits change nothing.
    Observation ob = reading("2026-10-08", "16:03", 240, 1000, 30);
    ob.limits_known = false;
    CHECK(!check(rec, ob).limits && rec.limits[0] == 90);
}

static void test_own_change()
{
    // PlayGuard set the clock and wrote limits: what follows is not reported.
    Record rec;
    check(rec, reading("2026-10-08", "20:10", 5772, 1000, 120));
    own_change(rec);
    Findings f = check(rec, reading("2026-10-08", "19:47", 0, 1000 - 1387, 180));
    CHECK(nothing(f) && f.save);
    CHECK(rec.offset_s == 1000 - 1387 && rec.limits[0] == 180 && rec.spent_s == 0);
    // From there on, changes are seen again.
    CHECK(check(rec, reading("2026-10-08", "19:48", 0, 1000 - 1387, 120)).limits);
}

static void test_dismiss()
{
    Record rec;
    check(rec, reading("2026-10-08", "16:00", 3000, 1000));
    check(rec, reading("2026-10-08", "16:01", 0, 1000));
    CHECK(rec.notice == NOTICE_RESET);
    dismiss(rec);
    CHECK(rec.notice == 0 && rec.reset_at.empty());
    CHECK(nothing(check(rec, reading("2026-10-08", "16:02", 60, 1000))));
    CHECK(rec.notice == 0);
}

static void test_file()
{
    Record rec;
    check(rec, reading("2026-10-08", "16:00", 3000, -12345, 120));
    Observation ob = reading("2026-10-08", "16:01", 0, -12345, 120);
    ob.limits[0] = 0xFFFF;
    check(rec, ob);
    const Record back = parse(serialize(rec));
    CHECK(back.date == "2026-10-08" && back.spent_s == 0 && back.saved_spent_s == 0);
    CHECK(back.offset_known && back.offset_s == -12345 && back.steady_id == "aa");
    CHECK(back.limits_known && back.limits[0] == 0xFFFF && back.limits[1] == 120);
    CHECK(back.notice == (NOTICE_RESET | NOTICE_LIMITS) && back.notice_date == "2026-10-08" && back.reset_at == "16:01");

    // Nothing known: nothing written but the schema, and read back as such.
    const Record empty = parse(serialize(Record()));
    CHECK(empty.date.empty() && empty.spent_s == -1 && !empty.offset_known && !empty.limits_known && !empty.notice);

    // Damaged or out of range: unknown, never a made-up value.
    for (const char* text : { "", "not json", "[]", "{\"date\": 5}" }) {
        const Record r = parse(text);
        CHECK(r.date.empty() && r.spent_s == -1 && !r.offset_known && !r.limits_known && r.notice == 0);
    }
    Record r = parse(R"({"date": "2026-10-08", "spent_s": -5, "limits": [1, 2, 3, 4, 5, 6, 1441],
                         "notice": 9, "notice_date": "2026-10-08", "offset_s": 1.5})");
    CHECK(r.date == "2026-10-08" && r.spent_s == -1 && !r.limits_known && r.notice == 0 && !r.offset_known);
    r = parse(R"({"date": "08/10/2026", "spent_s": 60, "limits": [1, 2, 3], "notice": 1, "notice_date": "x"})");
    CHECK(r.date.empty() && r.spent_s == -1 && !r.limits_known && r.notice == 0);
    r = parse(R"({"notice": 1, "notice_date": "2026-10-08", "reset_at": "25h"})");
    CHECK(r.notice == NOTICE_RESET && r.reset_at.empty());
}

int main()
{
    test_first_and_steady();
    test_reset();
    test_clock();
    test_limits();
    test_own_change();
    test_dismiss();
    test_file();
    return CHECK_DONE("outside_change_logic tests passed");
}
