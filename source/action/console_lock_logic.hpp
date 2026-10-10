// console_lock_logic — what the console lock saves when it is turned on and
// what it puts back when it is turned off, without any UI. Plain C++ over the
// service-layer structs so the host tests can run it (tests/console_lock_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <vector>

#include "util/pctl_ops_c.hpp"

namespace console_lock_logic
{

// The seven limits to save (config console_lock_prev) before setting them all
// to 0, or {} when they could not be read: an unread limit would be saved as
// "no limit", and turning the lock off would then remove every limit.
std::vector<int> to_save(const PtState& now);

// Turning it off: the saved limits back when seven were saved and at least one
// is a limit, else the limit cleared.
struct Unlock
{
    bool     restore;   // write `days`; false: clear the limit
    uint16_t days[7];   // the saved limits, or "no limit" every day
};
Unlock plan_unlock(const std::vector<int>& saved);

}   // namespace console_lock_logic
