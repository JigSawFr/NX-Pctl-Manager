// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/timer_health_logic.hpp"

namespace timer_health_logic
{

bool should_count(const PtState& pt, int weekday, bool applet_mode)
{
    if (applet_mode || weekday < 0 || weekday > 6) return false;
    if (!pt.fw_supported || !pt.valid || !pt.enabled_valid || !pt.enabled) return false;
    // Unknown counts as unlocked: the countdown may be off then.
    if (!pt.temporary_unlocked_valid || pt.temporary_unlocked) return false;
    if (!pt.restricted_valid || pt.restricted) return false;
    const uint16_t limit = pt.day_min[weekday];
    if (limit == PT_DAY_NOLIMIT || limit == 0) return false;
    return pt.remaining_valid && pt.remaining_ns > 0;
}

static void restart(State& s, uint64_t remaining_ns, uint16_t limit, uint64_t now_ms)
{
    s.has_base = true;
    s.base_ns = remaining_ns;
    s.base_limit = limit;
    s.last_ms = now_ms;
    s.still_ms = 0;
    s.stalled = false;
}

bool step(State& s, bool should, uint64_t remaining_ns, uint16_t limit, uint64_t now_ms, bool in_focus)
{
    if (!should) {
        s = State();
        return false;
    }
    if (!s.has_base || limit != s.base_limit || now_ms < s.last_ms) {
        restart(s, remaining_ns, limit, now_ms);
        return false;
    }
    const uint64_t gap = now_ms - s.last_ms;
    s.last_ms = now_ms;
    if (in_focus && gap <= MAX_GAP_MS) s.still_ms += gap;
    // Down: counting. Up: extra time, or the day started over (a clock
    // change): wait again from there.
    if (remaining_ns + MOVED_NS <= s.base_ns || remaining_ns >= s.base_ns + MOVED_NS) {
        restart(s, remaining_ns, limit, now_ms);
        return false;
    }
    if (s.still_ms >= STALL_MS) s.stalled = true;
    return s.stalled;
}

}   // namespace timer_health_logic
