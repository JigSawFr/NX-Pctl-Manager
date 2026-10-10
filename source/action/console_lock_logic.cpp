// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/console_lock_logic.hpp"

namespace console_lock_logic
{

std::vector<int> to_save(const PtState& now)
{
    if (!now.valid) return {};
    return std::vector<int>(now.day_min, now.day_min + 7);
}

Unlock plan_unlock(const std::vector<int>& saved)
{
    bool any = false;
    for (int v : saved) any = any || v != (int)PT_DAY_NOLIMIT;
    Unlock u;
    u.restore = saved.size() == 7 && any;
    for (int i = 0; i < 7; i++) u.days[i] = u.restore ? (uint16_t)saved[i] : PT_DAY_NOLIMIT;
    return u;
}

}   // namespace console_lock_logic
