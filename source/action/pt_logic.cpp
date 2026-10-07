// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/pt_logic.hpp"

#include <algorithm>

namespace pt_logic
{

bool state_known(const PtState& pt)
{
    return pt.valid && pt.enabled_valid && pt.restricted_valid && pt.temporary_unlocked_valid;
}

bool needs_unlock(const PtState& pt)
{
    return (pt.enabled || pt.restricted) && !pt.temporary_unlocked;
}

int played_today_min(const PtState& pt, int weekday)
{
    if (!pt.valid || !pt.enabled_valid || !pt.enabled || weekday < 0 || weekday > 6) return -1;
    const uint16_t limit = pt.day_min[weekday];
    if (limit == PT_DAY_NOLIMIT) return -1;
    if (pt.restricted_valid && pt.restricted) return limit;
    // The remaining time reads 0 until a game has been counted today.
    if (!pt.remaining_valid || pt.remaining_ns == 0) return -1;
    const uint64_t left = pt.remaining_ns / 60000000000ULL;
    return left >= limit ? 0 : (int)(limit - left);
}

int suspend_warning_min(const PtState& pt, int weekday, const uint16_t new_days[7])
{
    if (!new_days || weekday < 0 || weekday > 6) return -1;
    const int played = played_today_min(pt, weekday);
    const uint16_t next = new_days[weekday];
    return played > 0 && next != PT_DAY_NOLIMIT && next < played ? played : -1;
}

bool can_add_extra_time(const PtState& pt, int weekday, bool read_only)
{
    if (read_only || weekday < 0 || weekday > 6) return false;
    return pt.fw_supported && pt.valid && pt.enabled_valid && pt.enabled &&
           pt.day_min[weekday] != PT_DAY_NOLIMIT && pt.day_min[weekday] < 1440;
}

ExtraPlan plan_extra(uint16_t base, uint16_t extra, bool again, uint16_t recorded_base)
{
    return { (uint16_t)std::min<int>(1440, base + extra), again ? recorded_base : base };
}

Restore restore_action(const ExtraRecord& rec, const std::string& today, const PtState& pt, bool read_only)
{
    if (rec.weekday < 0 || rec.weekday > 6) return Restore::None;
    if (rec.date == today) return Restore::None;           // still the day it was added
    if (read_only || !pt.fw_supported || !pt.valid) return Restore::Later;
    if (pt.day_min[rec.weekday] != rec.value) return Restore::Forget;
    return Restore::Offer;
}

}   // namespace pt_logic
