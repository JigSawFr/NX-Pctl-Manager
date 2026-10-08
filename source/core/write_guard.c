// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "write_guard.h"

#include <stddef.h>

static bool s_read_only = false;
static bool (*s_check)(void) = NULL;

void core_set_read_only(bool on) { s_read_only = on; }
bool core_read_only(void)        { return s_read_only; }

void core_set_change_check(bool (*check)(void)) { s_check = check; }

Result core_change_allowed(void)
{
    if (s_read_only) return NXM_RC_READ_ONLY;
    if (s_check && !s_check()) return NXM_RC_NOT_CONFIRMED;
    return 0;
}
