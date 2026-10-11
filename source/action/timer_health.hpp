// timer_health — the Overview's and the Play timer's readings, fed to one
// timer_health_logic::State (the decision and its thresholds are there):
// "the console is not counting play time right now".
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "util/pctl_ops_c.hpp"

namespace timer_health
{

// One reading of the play timer, from a screen's periodic refresh. True
// while the console is not counting play time although it should.
bool observe(const PtState& pt);

}   // namespace timer_health
