// console_lock — Security › Console lock: one switch that sets every day's
// play-time limit to 0, so a PIN is needed to play at all (a light "lock the
// console" without turning on age ratings, communication limits or the phone
// app). It reuses the proven play-timer write (pt_flow): the limits it
// replaces are saved (config console_lock_prev) and put back when it is
// turned off.
//
// It needs a PIN on the console (nothing gates play without one) and firmware
// 21.0.0+ (the play-timer limit). It does not stop the HOME menu or System
// Settings — it blocks starting games — so a determined user who can reboot
// to another configuration can still get around it; it is meant to keep a
// child out, like the rest of PlayGuard.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>

#include "util/pctl_ops_c.hpp"

namespace console_lock
{

// Whether the lock is on (config flag). The limits may also have been changed
// elsewhere since; this is PlayGuard's own record of having set the lock.
bool active();

// Turn it on (all days to 0, saving what they were) or off (put the saved
// limits back, or clear the limit when none were saved), through the same
// confirm + temporary-unlock + relock as any limit change. The limits to save
// are read again once confirmed (`pt` only says what to ask); `refresh` runs after the change. The caller checks there is a PIN and
// that the firmware supports the play timer before offering this.
void set(bool on, const PtState& pt, std::function<void()> refresh);

// Limits written by anything else (same limit every day, a profile, a backup,
// the history…) replace the lock: the flag and the saved limits go.
void forget();

}   // namespace console_lock
