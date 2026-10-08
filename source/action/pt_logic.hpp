// pt_logic — the decisions behind pt_flow, without any UI: is the state known,
// does a write need the temporary unlock, how much was played, what extra time
// gives and when it is put back. Plain C++ over the service-layer structs so
// the host tests can run it (tests/pt_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

#include "util/pctl_ops_c.hpp"

namespace pt_logic
{

// Every field a play-timer write is gated on was read.
bool state_known(const PtState& pt);

// The timer counts down (or has suspended the game) and is not unlocked: a
// write has to unlock first. Only meaningful when state_known().
bool needs_unlock(const PtState& pt);

// Minutes already played today (`weekday` 0 = Sunday), or -1 when the system
// does not say (timer off, no limit today, or no game counted yet).
int played_today_min(const PtState& pt, int weekday);

// The minutes already played when `new_days` puts today's limit below them
// (the game in progress is then suspended at the lock), else -1.
int suspend_warning_min(const PtState& pt, int weekday, const uint16_t new_days[7]);

// "Extra time today…" can be offered: writable, a limit applies today and it
// is below 24 h.
bool can_add_extra_time(const PtState& pt, int weekday, bool read_only);

// Extra time on a day: the limit to write and the one to put back later. When
// extra time was already added today (`again`), what gets put back stays the
// limit from before the first addition (`recorded_base`).
struct ExtraPlan
{
    uint16_t value;      // new limit, at most 24 h
    uint16_t original;   // what "put back" restores
};
ExtraPlan plan_extra(uint16_t base, uint16_t extra, bool again, uint16_t recorded_base);

// "No more play today" can be offered: writable, the play timer can be read
// and today's limit is not already 0 (with no limit today it sets one).
bool can_stop_today(const PtState& pt, int weekday, bool read_only);

// No more play today: 0 for today, and what to put back the next day (the
// limit from before any extra time added earlier today, as for extra time).
ExtraPlan plan_stop(uint16_t base, bool again, uint16_t recorded_base);

// What config remembers about extra time (config extra_*).
struct ExtraRecord
{
    int         weekday = -1;   // -1: nothing recorded
    std::string date;           // "YYYY-MM-DD" it was added on
    uint16_t    base  = 0;      // limit before
    uint16_t    value = 0;      // limit written
};

enum class Restore
{
    None,    // nothing recorded, or still the day it was added
    Later,   // cannot tell now (read failed, read-only): ask at the next start
    Forget,  // the limit changed since: nothing to put back, drop the record
    Offer,   // the extra time is still there: offer to put `base` back
};
Restore restore_action(const ExtraRecord& rec, const std::string& today, const PtState& pt, bool read_only);

}   // namespace pt_logic
