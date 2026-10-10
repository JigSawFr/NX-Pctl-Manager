// pctl_ops_c — the C service layer's headers (source/core/) behind an
// extern "C" bridge, so the C++ UI can call it. Include this anywhere you'd
// want the core headers.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

extern "C" {
#include "core/pctl_ops.h"
#include "core/playstats.h"
#include "core/sysinfo.h"
#include "core/time_ops.h"
#include "core/write_guard.h"
}
