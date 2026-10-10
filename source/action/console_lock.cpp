// console_lock — Security › Console lock (see console_lock.hpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/console_lock.hpp"

#include <borealis.hpp>

#include "action/console_lock_logic.hpp"
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
        // What we put back later, read now (console_lock_logic::to_save).
        PtState now;
        pctl_play_timer_query(&now);
        const std::vector<int> prev = console_lock_logic::to_save(now);
        if (prev.empty()) {
            pt_flow::finish_write(NXM_RC_STATE_UNKNOWN, did_unlock, "", "playguard/play_timer/write_err"_i18n, refresh);
            return;
        }
        const uint16_t zero[7] = { 0, 0, 0, 0, 0, 0, 0 };
        // The limits to put back are saved before they are replaced: should
        // the app stop between the two, turning the lock off still restores
        // them. Not saved, nothing is written (they would be lost).
        auto& cfg = config::get();
        cfg.console_lock_prev = prev;
        cfg.console_lock = true;
        if (!ui::save_config()) {
            cfg.console_lock_prev.clear();
            cfg.console_lock = false;
            pt_flow::finish_write(NXM_RC_NOT_SAVED, did_unlock, "", "playguard/play_timer/write_err"_i18n, refresh);
            return;
        }
        Result rc = pt_flow::write_days(zero, "console_lock");
        if (R_SUCCEEDED(rc)) {
            history_flow::record_values("console_lock", { 0 }, { 1 });
        } else {
            cfg.console_lock_prev.clear();
            set_flag(false);
        }
        pt_flow::finish_write(rc, did_unlock, "playguard/console_lock/on_done"_i18n,
                              "playguard/play_timer/write_err"_i18n, refresh);
    }, zero, true);
}

// Turn it off: put the saved limits back, or clear the limit if none saved.
void unlock(std::function<void()> refresh)
{
    const console_lock_logic::Unlock plan = console_lock_logic::plan_unlock(config::get().console_lock_prev);

    pt_flow::confirm_write("playguard/console_lock/off_body"_i18n, "playguard/console_lock/off_confirm"_i18n,
                           [refresh, plan](bool did_unlock) {
        Result rc = plan.restore ? pt_flow::write_days(plan.days, "console_lock") : pt_flow::clear_days("console_lock");
        if (R_SUCCEEDED(rc)) {
            config::get().console_lock_prev.clear();
            set_flag(false);
            history_flow::record_values("console_lock", { 1 }, { 0 });
        }
        pt_flow::finish_write(rc, did_unlock, "playguard/console_lock/off_done"_i18n,
                              "playguard/play_timer/write_err"_i18n, refresh);
    }, plan.restore ? plan.days : nullptr, true);
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
