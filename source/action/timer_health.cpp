// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/timer_health.hpp"

#include <borealis.hpp>
#include <chrono>

#include "action/timer_health_logic.hpp"
#include "app.hpp"
#include "ui/ui.hpp"

namespace timer_health
{

namespace
{
timer_health_logic::State s_state;
}

bool observe(const PtState& pt)
{
    SysInfo si;
    sysinfo_get(&si);   // cached after the first call
    const int wd = ui::today_weekday();
    const bool should = timer_health_logic::should_count(pt, wd, si.applet_mode);
    const uint64_t now_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now().time_since_epoch()).count();
    const bool was = s_state.stalled;
    const bool stalled = timer_health_logic::step(s_state, should, pt.remaining_ns, should ? pt.day_min[wd] : 0,
                                                  now_ms, app::in_focus());
    if (stalled != was) brls::Logger::info("timer health: {}", stalled ? "not counting" : "counting");
    return stalled;
}

}   // namespace timer_health
