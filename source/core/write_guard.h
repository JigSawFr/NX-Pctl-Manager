// write_guard — the read-only switch of the service layer.
//
// While it is on, every function that would change the console (pctl_ops.c,
// time_ops.c, and their simulated twins in source/sim/) returns
// NXM_RC_READ_ONLY before opening any session. The UI hides its write
// controls too, but this is the check that cannot be skipped.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

void core_set_read_only(bool on);
bool core_read_only(void);
