// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/pt_log_flow.hpp"

#include <borealis.hpp>
#include <chrono>
#include <fmt/format.h>

#include "app.hpp"
#include "util/config.hpp"
#include "util/pctl_ops_c.hpp"
#include "util/pt_log.hpp"

namespace pt_log_flow
{

namespace
{
constexpr int PERIOD_MS = 30000;

brls::RepeatingTimer* s_timer = nullptr;   // made at the first start: borealis must be up
bool s_running = false;
pt_log::Previous s_prev;
bool s_warned = false;   // one log line when the SD card refuses, not one every 30 s
pt_log::Clock s_clock;   // the last reading's clocks, to see the user clock move

uint64_t steady_s()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

// A line now. `event`: why, when it is not the 30 s tick.
void record(std::string event = "")
{
    u64 posix = 0;
    LocalTime l {};
    const bool local = time_local_now(&posix, &l);
    const std::string stamp = local ? fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", (int)l.year,
                                                  (int)l.month, (int)l.day, (int)l.hour, (int)l.minute, (int)l.second)
                                    : std::string("?");
    const std::string moved = pt_log::clock_moved(&s_clock, posix, steady_s());
    if (!moved.empty()) event = event.empty() ? moved : event + "; " + moved;
    PtSample s;
    pctl_play_timer_sample(&s);
    std::string error;
    if (pt_log::append(pt_log::row(stamp, posix, s, &s_prev, event), &error)) s_warned = false;
    else if (!s_warned) {
        brls::Logger::warning("pt_log: {}", error);
        s_warned = true;
    }
}

void start()
{
    if (s_running) return;
    s_prev = {};   // the first line of a run carries both hex columns
    s_clock = {};
    s_running = true;
    record("recording started");
    if (!s_timer) {
        s_timer = new brls::RepeatingTimer();
        s_timer->setPeriod(PERIOD_MS);
        s_timer->setCallback([]() { record(); });
    }
    s_timer->start();
}
}   // namespace

void apply()
{
    if (app::dev_mode() && config::get().pt_log) start();
    else stop();
}

void stop()
{
    if (!s_running) return;
    if (s_timer) s_timer->stop();
    s_running = false;
}

bool running() { return s_running; }

void note(const std::string& event)
{
    if (!s_running) return;
    // On the main thread, after the change that called this has returned.
    brls::sync([event]() {
        if (s_running) record(event);
    });
}

}   // namespace pt_log_flow
