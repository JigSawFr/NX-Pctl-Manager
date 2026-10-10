// modules_nx — whether one of PlayGuard's sysmodules runs, and starting or
// stopping it now (pm:dmnt, pm:shell), without a reboot. hbloader gives an
// NRO both services. Each call opens the service and closes it before it
// returns. The desktop build simulates them (source/sim/sim_modules.c).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

// True when a process of `tid` runs. *rc: why it could not be told (0 when
// it could).
bool   module_running(u64 tid, Result *rc);
// Starts `tid` from the SD card (Atmosphère's atmosphere/contents/<tid>).
Result module_launch(u64 tid);
// Stops it.
Result module_terminate(u64 tid);
