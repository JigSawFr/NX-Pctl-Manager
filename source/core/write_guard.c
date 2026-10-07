// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "write_guard.h"

static bool s_read_only = false;

void core_set_read_only(bool on) { s_read_only = on; }
bool core_read_only(void)        { return s_read_only; }
