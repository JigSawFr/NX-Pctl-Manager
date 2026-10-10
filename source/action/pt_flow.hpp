// pt_flow — the "write-before-temporary-unlock" gate used by every play-timer
// write. If the timer is counting down, writing a new limit destabilises
// Atmosphère, so: ask the user → UnlockRestrictionTemporarily (1201) →
// verify IsRestrictionTemporaryUnlocked (1006) → let the caller write → lock
// again (automatically, or offered). The service layer re-checks the same state
// right before the write.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "util/pctl_ops_c.hpp"

namespace pt_flow
{

// Limits offered by the pickers, in minutes (0 = no play that day).
const std::vector<uint16_t>& quick_values();

// Minutes already played today, or -1 when the system does not say (timer
// off, no limit today, or no game counted yet).
int played_today_min(const PtState& pt);

// One dialog for the whole change. `body` asks the question ("Set 2 h for every
// day?"); when the timer is counting down the same dialog explains the
// temporary unlock and its button unlocks first. When `new_days` would put
// today's limit below the time already played, the dialog says the game in
// progress will be suspended. With nothing to say and no unlock needed,
// `write` runs at once. `write(did_unlock)` only runs when it is safe to
// write; otherwise the user has already been told why.
// `danger`: the confirm button is drawn in the "bad" colour (removing or
// overwriting limits).
void confirm_write(const std::string& body, const std::string& confirm_label,
                   std::function<void(bool did_unlock)> write, const uint16_t* new_days = nullptr,
                   bool danger = false);

// Writes the seven limits (pctl_play_timer_set_days) and records the change
// in the history with `source` ("uniform", "profile" …) and `detail` (a
// profile's name). Every limit write of the app goes through here.
Result write_days(const uint16_t days[7], const std::string& source, const std::string& detail = "");
// Removes the play-time limit (pctl_play_timer_clear), recorded the same way.
Result clear_days(const std::string& source);

// After a write made through confirm_write: toast the result, then lock again
// if the write needed an unlock (at once when the "lock again automatically"
// preference is on, else by offering it), then `refresh`.
void finish_write(Result rc, bool did_unlock, const std::string& ok_text,
                  const std::string& error_prefix, std::function<void()> refresh);

// "Same limit every day" picker (quick values + Custom…), then confirm_write.
void choose_uniform_limit(const PtState& pt, std::function<void()> refresh);

// One day's limit picker: the quick values, "Enter a duration…" (the number
// pad) and "No limit", with `current` pre-selected. `on_value` gets the
// minutes (PT_DAY_NOLIMIT for no limit).
void pick_limit(const std::string& title, uint16_t current, std::function<void(uint16_t)> on_value);

// Changes one weekday's limit (0 = Sunday) on the console, through pick_limit
// and confirm_write: what the week chart does on A.
void change_day_limit(int day, uint16_t current, std::function<void()> refresh);

// "Extra time today": raises today's weekday limit by 15 / 30 / 60 min and
// remembers the previous value (config extra_*), so that it can be put back.
void add_extra_time(const PtState& pt, std::function<void()> refresh);

// "No more play today": today's weekday limit to 0 for today only (through
// confirm_write: the suspend warning says the game in progress stops at the
// lock). Recorded like extra time, so the usual limit comes back the next day.
void stop_today(const PtState& pt, std::function<void()> refresh);

// Extra time added on an earlier day is still on its weekday limit -> offer
// to put the previous value back (or keep it), or put it back at once when the
// "by itself" preference is on. Called at start-up and when the date changes
// while the app is open; `refresh` runs after a change.
void offer_extra_time_restore(std::function<void()> refresh = nullptr);

// That offer is pending (what offer_extra_time_restore would ask now), and
// the line the Overview shows for it ("Put Saturday's limit back to 2 h").
bool restore_pending(const PtState& pt);
std::string restore_label();

// "+30 min" when extra time was added today and is still there, else "".
std::string extra_today_text(const PtState& pt);

// The timer is on and its "time's up" alarm is off: the console says nothing
// when time is up. The Overview and First steps point it out.
bool alarm_off(const PtState& pt);
// Turns that alarm back on after a confirmation (the PIN asked before a
// change, read-only mode: as for any change), recorded with `source`.
// Not an advanced action: it puts back what the console does by default.
void turn_alarm_on(const std::string& source, std::function<void()> refresh);
// Turns the alarm on or off after a confirmation, through confirm_write (the
// service layer refuses it while the timer counts down and nothing unlocked).
void set_alarm(bool on, const std::string& source, std::function<void()> refresh);
// Writes the alarm flag and records it in the history (callers already went
// through confirm_write).
Result write_alarm_disabled(bool disabled, const std::string& source);
// Advanced: pauses (StopPlayTimer) or resumes (StartPlayTimer) the countdown,
// through confirm_write.
void set_countdown(bool running, std::function<void()> refresh);

// Bedtime, for every day. choose_bedtime: the alarm (16:00 to 23:45, or
// off); choose_bedtime_end: when play is allowed again (05:00 to 09:00), on
// the days whose alarm is on. Both refuse while the block's bedtime is not
// what the console reports (pt_logic::bedtime_layout_ok), then go through
// confirm_write and write_bedtime.
void choose_bedtime(const PtState& pt, std::function<void()> refresh);
void choose_bedtime_end(const PtState& pt, std::function<void()> refresh);
// Writes the bedtimes (pctl_play_timer_set_bedtime, checked by the console's
// answer) and records the change in the history with `source`.
Result write_bedtime(const PtBedtime bed[7], const std::string& source);
// "21:00, allowed again at 06:00", "Off", or "Varies by day".
std::string bedtime_text(const PtState& pt);

// At start-up, before the first screen (and again on the main screen): the
// app stopped between an unlock made for a change and the lock that follows
// it (config relock_pending) -> lock again now, with a toast.
// The record stays until the console reads back as locked. Also called when
// the app leaves read-only mode, which keeps the record.
void relock_if_interrupted();

}   // namespace pt_flow
