// Host tests for source/action/timer_health_logic.cpp: when the Overview says
// the console is not counting play time, and when it stops saying so.
#include "check.h"
#include <cstring>

#include "action/timer_health_logic.hpp"

using namespace timer_health_logic;

static const uint64_t S_NS = 1000000000ULL;

// Timer on, every field read, 2 h every day, 45 min left, not unlocked.
static PtState counting()
{
    PtState pt;
    std::memset(&pt, 0, sizeof(pt));
    pt.session_valid = pt.fw_supported = pt.valid = true;
    pt.enabled_valid = pt.restricted_valid = pt.temporary_unlocked_valid = pt.remaining_valid = true;
    pt.enabled = true;
    for (auto& d : pt.day_min) d = 120;
    pt.remaining_ns = 45 * 60 * S_NS;
    return pt;
}

static void test_should_count()
{
    PtState pt = counting();
    CHECK(should_count(pt, 3, false));
    CHECK(!should_count(pt, 3, true));    // opened from the album: its time does not count
    CHECK(!should_count(pt, 7, false));
    pt.temporary_unlocked = true;
    CHECK(!should_count(pt, 3, false));
    pt = counting();
    pt.temporary_unlocked_valid = false;  // unknown: may be unlocked
    CHECK(!should_count(pt, 3, false));
    pt = counting();
    pt.enabled = false;
    CHECK(!should_count(pt, 3, false));
    pt = counting();
    pt.restricted = true;                 // reached: nothing left to count down
    CHECK(!should_count(pt, 3, false));
    pt = counting();
    pt.day_min[3] = PT_DAY_NOLIMIT;
    CHECK(!should_count(pt, 3, false));
    CHECK(should_count(pt, 2, false));
    pt.day_min[3] = 0;
    CHECK(!should_count(pt, 3, false));
    pt = counting();
    pt.remaining_ns = 0;
    CHECK(!should_count(pt, 3, false));
    pt.remaining_valid = false;
    pt.remaining_ns = 10 * S_NS;
    CHECK(!should_count(pt, 3, false));
    pt = counting();
    pt.valid = false;
    CHECK(!should_count(pt, 3, false));
    pt = counting();
    pt.fw_supported = false;
    CHECK(!should_count(pt, 3, false));
}

// Readings every 5 s with the time left frozen: not counting after 90 s.
static void test_stall()
{
    State s;
    uint64_t t = 1000;
    const uint64_t left = 2700 * S_NS;
    bool stalled = false;
    int readings = 0;
    for (; readings < 30 && !stalled; readings++, t += 5000) stalled = step(s, true, left, 120, t, true);
    CHECK(stalled);
    CHECK(readings == 19);   // the first one is the start, then 18 × 5 s = 90 s

    // Still frozen: it stays.
    CHECK(step(s, true, left, 120, t, true));
    // Counting again (down by 5 s or more): cleared at once, and it takes
    // another 90 s to come back.
    CHECK(!step(s, true, left - 6 * S_NS, 120, t + 5000, true));
    CHECK(!s.stalled);
    CHECK(!step(s, true, left - 6 * S_NS, 120, t + 10000, true));
}

static void test_counting()
{
    // The time left goes down 1 s a second: never stalled.
    State s;
    uint64_t left = 2700 * S_NS;
    for (uint64_t t = 0; t < 600000; t += 5000, left -= 5 * S_NS) CHECK(!step(s, true, left, 120, t, true));
    // Small wobbles (less than 5 s) do not count as moving.
    State w;
    bool stalled = false;
    for (uint64_t t = 0; t <= 95000; t += 5000) stalled = step(w, true, 2700 * S_NS - (t % 10000 ? S_NS : 0), 120, t, true);
    CHECK(stalled);
}

static void test_restarts()
{
    // Time left going up (extra time, a clock set back): the wait starts
    // again from the new value.
    State s;
    uint64_t t = 0;
    for (; t < 60000; t += 5000) CHECK(!step(s, true, 1000 * S_NS, 120, t, true));
    CHECK(!step(s, true, 7200 * S_NS, 120, t, true));
    for (t += 5000; t < 60000 + 90000; t += 5000) CHECK(!step(s, true, 7200 * S_NS, 120, t, true));
    CHECK(step(s, true, 7200 * S_NS, 120, t, true));

    // A new limit today restarts the wait.
    State l;
    for (t = 0; t < 85000; t += 5000) step(l, true, 1000 * S_NS, 120, t, true);
    CHECK(!step(l, true, 1000 * S_NS, 90, t, true));
    CHECK(!step(l, true, 1000 * S_NS, 90, t + 5000, true));

    // A reading where it should not count resets everything.
    State r;
    for (t = 0; t <= 90000; t += 5000) step(r, true, 1000 * S_NS, 120, t, true);
    CHECK(r.stalled);
    CHECK(!step(r, false, 0, 0, t, true));
    CHECK(!r.has_base && !r.stalled);
    CHECK(!step(r, true, 1000 * S_NS, 120, t + 5000, true));

    // A clock going back (should not happen with a steady clock): restart.
    State b;
    step(b, true, 1000 * S_NS, 120, 50000, true);
    CHECK(!step(b, true, 1000 * S_NS, 120, 10000, true));
    CHECK(b.still_ms == 0);
}

static void test_foreground_only()
{
    // Out of focus (HOME menu), or a long gap (sleep mode, another tab with
    // no refresh): that time does not count as time on screen.
    State s;
    uint64_t t = 0;
    for (; t <= 200000; t += 5000) CHECK(!step(s, true, 1000 * S_NS, 120, t, false));
    State g;
    for (t = 0; t <= 600000; t += 20000) CHECK(!step(g, true, 1000 * S_NS, 120, t, true));
    // On screen again: 90 s more.
    for (t += 5000; g.still_ms < STALL_MS; t += 5000) step(g, true, 1000 * S_NS, 120, t, true);
    CHECK(g.stalled);
}

int main()
{
    test_should_count();
    test_stall();
    test_counting();
    test_restarts();
    test_foreground_only();
    return CHECK_DONE("timer_health_logic tests passed");
}
