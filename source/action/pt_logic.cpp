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

bool can_stop_today(const PtState& pt, int weekday, bool read_only)
{
    if (read_only || weekday < 0 || weekday > 6) return false;
    return pt.fw_supported && pt.valid && pt.day_min[weekday] != 0;
}

ExtraPlan plan_stop(uint16_t base, bool again, uint16_t recorded_base)
{
    return { 0, again ? recorded_base : base };
}

int stop_soon_limit(const PtState& pt, int weekday, int minutes)
{
    const int played = played_today_min(pt, weekday);
    if (played < 0 || minutes <= 0) return -1;
    const int value = played + minutes;
    return value < pt.day_min[weekday] ? value : -1;
}

Restore restore_action(const ExtraRecord& rec, const std::string& today, const PtState& pt, bool read_only)
{
    if (rec.weekday < 0 || rec.weekday > 6) return Restore::None;
    if (rec.date == today) return Restore::None;           // still the day it was added
    if (read_only || !pt.fw_supported || !pt.valid) return Restore::Later;
    if (pt.day_min[rec.weekday] != rec.value) return Restore::Forget;
    return Restore::Offer;
}

static bool reported(const PtState& pt, const PtBedtime& b)
{
    return b.on == pt.bedtime_enabled && (!b.on || (b.hour == pt.bedtime_hour && b.minute == pt.bedtime_minute));
}

bool bedtime_layout_ok(const PtState& pt, int weekday)
{
    if (weekday < 0 || weekday > 6 || !pt.fw_supported || !pt.valid || !pt.bedtime_valid) return false;
    return reported(pt, pt.bed[weekday]) || reported(pt, pt.bed[(weekday + 6) % 7]);
}

static bool same(const PtBedtime& a, const PtBedtime& b)
{
    return a.on == b.on && (!a.on || (a.hour == b.hour && a.minute == b.minute && a.end_hour == b.end_hour &&
                                      a.end_minute == b.end_minute));
}

bool bedtime_uniform(const PtState& pt, PtBedtime* out)
{
    if (!pt.valid) return false;
    for (int n = 1; n < 7; n++)
        if (!same(pt.bed[n], pt.bed[0])) return false;
    if (out) *out = pt.bed[0];
    return true;
}

static bool end_in_range(uint8_t hour, uint8_t minute)
{
    const int t = hour * 60 + minute;
    return minute < 60 && t >= 5 * 60 && t <= 9 * 60;
}

void bedtime_every_day(const PtState& pt, bool on, uint8_t hour, uint8_t minute, PtBedtime out[7])
{
    for (int n = 0; n < 7; n++) {
        PtBedtime b = pt.bed[n];
        b.on = on;
        b.hour = on ? hour : 0;
        b.minute = on ? minute : 0;
        if (on && !end_in_range(b.end_hour, b.end_minute)) {
            b.end_hour = 6;
            b.end_minute = 0;
        }
        out[n] = b;
    }
}

bool bedtime_end_every_day(const PtState& pt, uint8_t hour, uint8_t minute, PtBedtime out[7])
{
    bool any = false;
    for (int n = 0; n < 7; n++) any = any || pt.bed[n].on;
    if (!any) return false;
    for (int n = 0; n < 7; n++) {
        out[n] = pt.bed[n];
        if (out[n].on) {
            out[n].end_hour = hour;
            out[n].end_minute = minute;
        }
    }
    return true;
}

}   // namespace pt_logic
