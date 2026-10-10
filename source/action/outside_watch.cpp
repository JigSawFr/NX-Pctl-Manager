// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/outside_watch.hpp"

#include <borealis.hpp>
#include <fmt/format.h>

#include "action/history_flow.hpp"
#include "action/outside_change_logic.hpp"
#include "action/timer_health_logic.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

namespace outside_watch
{

namespace
{
outside_change_logic::Record s_rec;
bool s_loaded = false;

std::string file() { return paths::data_dir() + "/watch.json"; }

void load_once()
{
    if (s_loaded) return;
    s_loaded = true;
    std::string text;
    if (paths::read_file(file(), text)) s_rec = outside_change_logic::parse(text);
}

void save()
{
    std::string err;
    if (!paths::ensure_dir(paths::data_dir()) || !paths::atomic_write(file(), outside_change_logic::serialize(s_rec), &err))
        brls::Logger::warning("watch: not saved ({})", err);
}

std::string hex(const uint8_t* b, size_t n)
{
    std::string out;
    for (size_t i = 0; i < n; i++) out += fmt::format("{:02x}", b[i]);
    return out;
}
}   // namespace

void observe(const PtState& pt)
{
    load_once();
    namespace ocl = outside_change_logic;
    ocl::Observation ob;
    ob.date = ui::today_date();
    ob.hm = ui::now_hms().substr(0, 5);
    const int wd = ui::today_weekday();
    // Time spent = today's limit - time left (1454 + 1952 = the limit,
    // docs/parental-controls.md), while the time left means something: the
    // same readings as the timer health check, whatever the applet mode.
    if (timer_health_logic::should_count(pt, wd, false)) {
        const int64_t spent = (int64_t)pt.day_min[wd] * 60 - (int64_t)((pt.remaining_ns + 500000000ULL) / 1000000000ULL);
        ob.spent_known = spent >= 0;
        ob.spent_s = spent;
    }
    u64 steady = 0, user = 0;
    uint8_t id[16];
    if (R_SUCCEEDED(time_steady_now(&steady, id))) {
        time_local_now(&user, nullptr);
        ob.clock_known = true;
        ob.offset_s = (int64_t)user - (int64_t)steady;
        ob.steady_id = hex(id, sizeof(id));
    }
    if (pt.fw_supported && pt.valid) {
        ob.limits_known = true;
        for (int i = 0; i < 7; i++) ob.limits[i] = pt.day_min[i];
    }

    const ocl::Findings f = ocl::check(s_rec, ob);
    if (f.save) save();
    // Once each in the change history (not undoable: PlayGuard did not make them).
    if (f.reset) history_flow::record_event("outside_reset", "", ob.hm);
    if (f.clock) {
        const int64_t m = f.clock_moved_s < 0 ? -f.clock_moved_s : f.clock_moved_s;
        history_flow::record_event("outside_clock", "", (f.clock_moved_s < 0 ? "−" : "+") + ui::fmt_play_time((uint64_t)m));
    }
    if (f.limits)
        history_flow::record_values("outside_limits", history_flow::days_values(f.limits_before),
                                    history_flow::days_values(f.limits_after));
    if (f.reset || f.clock || f.limits)
        brls::Logger::info("watch: changed outside PlayGuard:{}{}{}", f.reset ? " play time reset" : "",
                           f.clock ? " clock" : "", f.limits ? " limits" : "");
}

void own_change()
{
    load_once();
    outside_change_logic::own_change(s_rec);
    save();
}

std::vector<std::string> notice_lines()
{
    load_once();
    std::vector<std::string> out;
    if (!s_rec.notice || s_rec.notice_date != ui::today_date()) return out;
    if (s_rec.notice & outside_change_logic::NOTICE_RESET)
        out.push_back(brls::getStr("playguard/dashboard/outside_reset", s_rec.reset_at.empty() ? "?" : s_rec.reset_at));
    if (s_rec.notice & outside_change_logic::NOTICE_CLOCK) out.push_back("playguard/dashboard/outside_clock"_i18n);
    if (s_rec.notice & outside_change_logic::NOTICE_LIMITS) out.push_back("playguard/dashboard/outside_limits"_i18n);
    return out;
}

void dismiss()
{
    load_once();
    outside_change_logic::dismiss(s_rec);
    save();
}

}   // namespace outside_watch
