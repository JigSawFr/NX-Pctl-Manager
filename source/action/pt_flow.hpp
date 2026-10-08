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

// True when "Extra time today…" can be offered: a writable build, a limit
// applies today and it is below 24 h.
bool can_add_extra_time(const PtState& pt);

// "Extra time today": raises today's weekday limit by 15 / 30 / 60 min and
// remembers the previous value (config extra_*), so that it can be put back.
void add_extra_time(const PtState& pt, std::function<void()> refresh);

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

// At start-up: the app stopped between an unlock made for a change and the
// lock that follows it (config relock_pending) -> lock again now, with a toast.
void relock_if_interrupted();

}   // namespace pt_flow
