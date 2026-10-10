// Host tests for source/util/support.cpp: when the main screen shows
// "What's new" or the monthly "Support PlayGuard" reminder.
#include "check.h"
#include <cstdio>

#include "util/support.hpp"

using support::Show;
using support::State;

int main()
{
    int days = 0;
    CHECK(support::days_between("2026-10-09", "2026-11-08", &days) && days == 30);
    CHECK(support::days_between("2026-12-31", "2027-01-01", &days) && days == 1);
    CHECK(support::days_between("2028-02-28", "2028-03-01", &days) && days == 2);   // leap year
    CHECK(support::days_between("2026-10-09", "2026-10-01", &days) && days == -8);
    CHECK(!support::days_between("", "2026-10-09", &days));
    CHECK(!support::days_between("2026-13-01", "2026-10-09", &days));
    CHECK(!support::days_between("2026/10/09", "2026-10-09", &days));

    // A new install: nothing shown, the version is remembered, the month starts.
    auto d = support::decide(State{}, "1.1.0", "2026-10-09", true);
    CHECK(d.show == Show::Nothing && d.next.seen_version == "1.1.0" && d.next.reminded == "2026-10-09");
    CHECK(d.next.reminder_on);

    // Within the month: nothing; on day 30, the reminder, and a new month.
    State s{ true, "2026-10-09", "1.1.0" };
    d = support::decide(s, "1.1.0", "2026-11-07", true);
    CHECK(d.show == Show::Nothing && d.next.reminded == "2026-10-09");
    d = support::decide(s, "1.1.0", "2026-11-08", true);
    CHECK(d.show == Show::Reminder && d.next.reminded == "2026-11-08");

    // Turned off: never again for this version or the next (no "What's new"
    // reminder of the switch either: it stays off).
    s.reminder_on = false;
    d = support::decide(s, "1.1.0", "2027-06-01", true);
    CHECK(d.show == Show::Nothing && !d.next.reminder_on);
    d = support::decide(s, "1.2.0", "2027-06-01", true);
    CHECK(d.show == Show::WhatsNew && !d.next.reminder_on && d.next.seen_version == "1.2.0");

    // An update: "What's new" once, and the month starts again.
    s = State{ true, "2026-09-01", "1.1.0" };
    d = support::decide(s, "1.2.0", "2026-10-20", true);
    CHECK(d.show == Show::WhatsNew && d.next.seen_version == "1.2.0" && d.next.reminded == "2026-10-20");
    d = support::decide(d.next, "1.2.0", "2026-10-21", true);
    CHECK(d.show == Show::Nothing);
    // A version without notes (a development build): nothing, but handled.
    d = support::decide(s, "1.2.1", "2026-10-20", false);
    CHECK(d.show == Show::Nothing && d.next.seen_version == "1.2.1");

    // The clock set back, or a damaged date: the month starts again.
    s = State{ true, "2027-01-01", "1.1.0" };
    d = support::decide(s, "1.1.0", "2026-10-09", true);
    CHECK(d.show == Show::Nothing && d.next.reminded == "2026-10-09");
    s.reminded = "garbage";
    d = support::decide(s, "1.1.0", "2026-10-09", true);
    CHECK(d.show == Show::Nothing && d.next.reminded == "2026-10-09");

    return CHECK_DONE("support what's-new and monthly reminder assertions passed");
}
