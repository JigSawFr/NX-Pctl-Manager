// timer_health_logic — is the console actually counting play time? While
// PlayGuard runs as an application its own time counts (docs/parental-
// controls.md, "Time spent and time left"), so with a limit today the time
// left must go down while it is on screen. It does not after the clock was
// set back (the console waits for the clock to pass its old time), and other
// tools report it with a network clock never set. Plain C++ over the
// service-layer structs so the host tests can run it (tests/timer_health_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>

#include "util/pctl_ops_c.hpp"

namespace timer_health_logic
{

// Time on screen without the time left going down before PlayGuard says so.
constexpr uint64_t STALL_MS = 90000;
// A longer gap between two readings is not counted as time on screen: the
// HOME menu or sleep mode in between (refreshes come every 5 s).
constexpr uint64_t MAX_GAP_MS = 15000;
// The time left moved: down (counting) or up (a new limit, a reset) by at
// least this much.
constexpr uint64_t MOVED_NS = 5000000000ULL;

// The time left should be going down now: the timer is on and not
// unlocked, today (`weekday`, 0 = Sunday) has a limit above 0 that is not
// reached yet, the time left was read and is not 0, and PlayGuard runs as an
// application (`applet_mode` false: opened from the album, its time does
// not count).
bool should_count(const PtState& pt, int weekday, bool applet_mode);

struct State
{
    bool     has_base = false;
    uint64_t base_ns = 0;        // the time left the wait started from
    uint16_t base_limit = 0;     // today's limit then
    uint64_t last_ms = 0;        // when the previous reading was taken
    uint64_t still_ms = 0;       // time on screen since, without it going down
    bool     stalled = false;
};

// One reading, `now_ms` from a steady clock; `in_focus`: PlayGuard had the
// screen. Returns whether the console is not counting (the time left has
// not gone down over STALL_MS on screen). Any reading where it should not
// count starts the wait again, as does a new limit or the time left going up.
bool step(State& s, bool should, uint64_t remaining_ns, uint16_t limit, uint64_t now_ms, bool in_focus);

}   // namespace timer_health_logic
