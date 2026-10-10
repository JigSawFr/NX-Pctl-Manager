// pt_log_flow — runs the play-timer recorder (util/pt_log) of Tools ›
// Developer: a line now, then one every 30 s while PlayGuard is open. Only
// reads the console and appends to the SD card. Its switch is remembered
// (config pt_log) and only acts while the developer mode is on.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace pt_log_flow
{

// Starts or stops recording, as config pt_log and the developer mode say.
// At start-up, and whenever either changes.
void apply();
// At exit.
void stop();
bool running();

// A line right away (once the caller has returned), with `event` in its last
// column, while recording: what PlayGuard just changed (history_flow), so the
// readings before and after it can be told apart.
void note(const std::string& event);

}   // namespace pt_log_flow
