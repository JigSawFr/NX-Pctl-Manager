// write_guard — what the service layer checks before changing the console.
//
// The read-only switch: while it is on, every function that would change the
// console (pctl_ops.c, time_ops.c, and their simulated twins in source/sim/)
// returns NXM_RC_READ_ONLY before opening any session. The UI hides its write
// controls too, but this is the check that cannot be skipped.
//
// The change check: a function the UI installs (Security › Ask for the PIN)
// that runs before every change and may ask for the parental-control PIN; it
// refuses with NXM_RC_NOT_CONFIRMED. It runs before any session is opened,
// so it may show the system's PIN applet. Locking again (1007) never asks:
// it is never refused for want of a PIN.
//
// The reveal check: the same, before the stored PIN is read for display
// (pctl_get_pin). The UI installs one that asks for the PIN every time,
// whatever the change check would let through (its grace, a recovery).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

void core_set_read_only(bool on);
bool core_read_only(void);

// NULL (the default): every change is allowed.
void core_set_change_check(bool (*check)(void));

// What every change calls first: NXM_RC_READ_ONLY, NXM_RC_NOT_CONFIRMED, or 0.
Result core_change_allowed(void);

// NULL (the default): the change check stands in for it.
void core_set_reveal_check(bool (*check)(void));

// What showing the PIN calls first: the same results as core_change_allowed.
Result core_reveal_allowed(void);
