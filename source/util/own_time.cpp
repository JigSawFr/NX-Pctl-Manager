// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/own_time.hpp"

#include <borealis.hpp>
#include <chrono>

#include "util/pctl_ops_c.hpp"   // time_ops.h, as C
#include "util/paths.hpp"

namespace own_time
{

namespace
{
constexpr uint64_t KEEP_S = 8 * 86400;   // the Activity tab looks 7 days back

bool s_on = false;
uint64_t s_start = 0;                                  // user clock
std::chrono::steady_clock::time_point s_started;       // the end follows this one
std::vector<PlayLogSpan> s_earlier;                    // read at start
brls::RepeatingTimer* s_timer = nullptr;   // made at start: borealis must be up

std::string file() { return paths::data_dir() + "/own_time.txt"; }

// This session so far. Its end follows the steady clock, not the user clock
// (the Network clock tab may set that one while PlayGuard is open).
PlayLogSpan current()
{
    const auto elapsed = std::chrono::steady_clock::now() - s_started;
    return { s_start, s_start + (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() };
}

void save()
{
    std::vector<PlayLogSpan> all = s_earlier;
    all.push_back(current());
    paths::ensure_dir(paths::data_dir());
    std::string error;
    if (!paths::atomic_write(file(), format(all), &error))
        brls::Logger::warning("own_time: {}", error);
}
}   // namespace

void start(bool counted)
{
    if (s_on || !counted) return;
    u64 now = 0;
    time_local_now(&now, nullptr);
    if (!now) return;
    std::string text;
    paths::read_file(file(), text);
    s_earlier = parse(text, now > KEEP_S ? now - KEEP_S : 0);
    s_start = now;
    s_started = std::chrono::steady_clock::now();
    s_on = true;
    save();
    s_timer = new brls::RepeatingTimer();
    s_timer->setPeriod(30000);
    s_timer->setCallback([]() { save(); });
    s_timer->start();
}

void stop()
{
    if (!s_on) return;
    if (s_timer) s_timer->stop();
    save();
    s_on = false;
}

std::vector<PlayLogSpan> spans()
{
    std::vector<PlayLogSpan> out = s_earlier;
    if (s_on) out.push_back(current());
    return out;
}

}   // namespace own_time
