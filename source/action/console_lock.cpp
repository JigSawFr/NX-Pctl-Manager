// console_lock — Security › Console lock (see console_lock.hpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/console_lock.hpp"

#include <borealis.hpp>

#include "action/history_flow.hpp"
#include "action/pt_flow.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

namespace console_lock
{

namespace
{
void set_flag(bool on)
{
    config::get().console_lock = on;
    ui::save_config();
}

// Turn the lock on: save the seven limits, set them all to 0.
void lock(std::function<void()> refresh)
{
    uint16_t zero[7] = { 0, 0, 0, 0, 0, 0, 0 };

    pt_flow::confirm_write("playguard/console_lock/on_body"_i18n, "playguard/console_lock/on_confirm"_i18n,
                           [refresh](bool did_unlock) {
        // What we put back later, read now: a limit that could not be read
        // would be saved as "no limit", and turning the lock off would then
        // remove every limit.
        PtState now;
        pctl_play_timer_query(&now);
        if (!now.valid) {
            pt_flow::finish_write(NXM_RC_STATE_UNKNOWN, did_unlock, "", "playguard/play_timer/write_err"_i18n, refresh);
            return;
        }
        const std::vector<int> prev(now.day_min, now.day_min + 7);
        const uint16_t zero[7] = { 0, 0, 0, 0, 0, 0, 0 };
        Result rc = pt_flow::write_days(zero, "console_lock");
        if (R_SUCCEEDED(rc)) {
            config::get().console_lock_prev = prev;
            set_flag(true);
            history_flow::record_values("console_lock", { 0 }, { 1 });
        }
        pt_flow::finish_write(rc, did_unlock, "playguard/console_lock/on_done"_i18n,
                              "playguard/play_timer/write_err"_i18n, refresh);
    }, zero, true);
}

// Turn it off: put the saved limits back, or clear the limit if none saved.
void unlock(std::function<void()> refresh)
{
    const std::vector<int> saved = config::get().console_lock_prev;
    bool any = false;
    for (int v : saved) any = any || v != (int)PT_DAY_NOLIMIT;
    const bool restore = saved.size() == 7 && any;

    uint16_t back[7];
    for (int i = 0; i < 7; i++) back[i] = restore ? (uint16_t)saved[i] : PT_DAY_NOLIMIT;

    pt_flow::confirm_write("playguard/console_lock/off_body"_i18n, "playguard/console_lock/off_confirm"_i18n,
                           [refresh, restore, saved](bool did_unlock) {
        uint16_t days[7];
        for (int i = 0; i < 7; i++) days[i] = restore ? (uint16_t)saved[i] : PT_DAY_NOLIMIT;
        Result rc = restore ? pt_flow::write_days(days, "console_lock") : pt_flow::clear_days("console_lock");
        if (R_SUCCEEDED(rc)) {
            config::get().console_lock_prev.clear();
            set_flag(false);
            history_flow::record_values("console_lock", { 1 }, { 0 });
        }
        pt_flow::finish_write(rc, did_unlock, "playguard/console_lock/off_done"_i18n,
                              "playguard/play_timer/write_err"_i18n, refresh);
    }, restore ? back : nullptr, true);
}
}   // namespace

bool active()
{
    return config::get().console_lock;
}

void set(bool on, const PtState& pt, std::function<void()> refresh)
{
    if (on && !pt.valid) {
        ui::error(ui::rc_text(NXM_RC_STATE_UNKNOWN));
        return;
    }
    if (on) lock(refresh);
    else unlock(refresh);
}

void forget()
{
    auto& cfg = config::get();
    if (!cfg.console_lock && cfg.console_lock_prev.empty()) return;
    cfg.console_lock = false;
    cfg.console_lock_prev.clear();
    ui::save_config();
}

}   // namespace console_lock
