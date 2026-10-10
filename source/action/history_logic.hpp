// history_logic — the decisions behind history_flow's undo, without any UI:
// what an undoable entry's dialog offers given the console's value now, and
// what the undo records. Whether an entry can be undone at all is
// history::undoable (tests/history). Host-tested (tests/history_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <vector>

#include "util/history.hpp"

namespace history_logic
{

enum class Undo
{
    AlreadyBack,     // the console already holds `before`
    ReadOnly,        // read-only mode: nothing is written
    NeedsAdvanced,   // the alarm is changed only in advanced mode
    Offer,           // offer to put `before` back
};
// For an undoable entry `e`; `now` is the console's value of its kind ({}
// when it could not be read: the undo is still offered).
Undo undo_action(const history::Entry& e, const std::vector<int>& now, bool read_only, bool advanced);

// The value changed again since the entry: the dialog says so.
bool changed_since(const history::Entry& e, const std::vector<int>& now);

// What an undo records as the value it replaced: the console's now, or the
// entry's `after` when that could not be read.
std::vector<int> undo_replaces(const history::Entry& e, const std::vector<int>& now);

}   // namespace history_logic
