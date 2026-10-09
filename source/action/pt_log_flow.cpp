// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/pt_log_flow.hpp"

#include <borealis.hpp>
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

void record()
{
    u64 posix = 0;
    LocalTime l {};
    const bool local = time_local_now(&posix, &l);
    const std::string stamp = local ? fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", (int)l.year,
                                                  (int)l.month, (int)l.day, (int)l.hour, (int)l.minute, (int)l.second)
                                    : std::string("?");
    PtSample s;
    pctl_play_timer_sample(&s);
    std::string error;
    if (pt_log::append(pt_log::row(stamp, posix, s, &s_prev), &error)) s_warned = false;
    else if (!s_warned) {
        brls::Logger::warning("pt_log: {}", error);
        s_warned = true;
    }
}

void start()
{
    if (s_running) return;
    s_prev = {};   // the first line of a run carries both hex columns
    s_running = true;
    record();
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

}   // namespace pt_log_flow
