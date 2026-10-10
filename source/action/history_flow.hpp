// history_flow — records what PlayGuard changes on the console in the change
// history (util/history.hpp), words it for the History screen, and puts a
// recorded value back (through the same gates as any other change: the
// play-timer unlock, the PIN asked before a change, read-only mode).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "util/history.hpp"
#include "util/pctl_ops_c.hpp"

namespace history_flow
{

std::vector<int> days_values(const uint16_t days[7]);
std::vector<int> custom_values(const PctlCustomSettings& s);

// A value change ("limits", "level", "custom", "org", "vr", "alarm"); nothing
// is recorded when `before` equals `after`. A failed SD write is only logged:
// the change itself went through. `when` is now unless given (a change the
// agent made while PlayGuard was closed: "2026-10-08 18:30").
void record_values(const char* kind, std::vector<int> before, std::vector<int> after,
                   const std::string& source = "", const std::string& detail = "", const std::string& when = "");
// An action with no value to put back ("pin", "unlock", "unlink", …).
void record_event(const char* kind, const std::string& source = "", const std::string& detail = "",
                  const std::string& when = "");

// The History screen's wording: one line, and the details for its dialog.
std::string title(const history::Entry& e);
std::string details(const history::Entry& e);

// A on a History line: its details, and when the value can be put back, the
// offer to (through the same gates as the change itself), recorded as an
// "undo". Says why not when it cannot be.
void open(const history::Entry& e, std::function<void()> refresh);

}   // namespace history_flow
