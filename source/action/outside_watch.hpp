// outside_watch — changes made outside PlayGuard (outside_change_logic has
// the rules): fed by the Overview's and the Play timer's refreshes, kept in
// watch.json, each change seen once in the change history, and a notice on
// the Overview until it is dismissed or the day ends.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

#include "util/pctl_ops_c.hpp"

namespace outside_watch
{

// One reading of the play timer (the clocks are read here).
void observe(const PtState& pt);

// PlayGuard itself changed the limits, the clock or the whole parental
// controls (history_flow calls it for every change it records).
void own_change();

// The Overview's notice, one line per change seen today; empty: none.
std::vector<std::string> notice_lines();
void dismiss();

}   // namespace outside_watch
