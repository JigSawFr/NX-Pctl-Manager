// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "action/pt_flow.hpp"

#include <algorithm>
#include <borealis.hpp>
#include <ctime>

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

namespace pt_flow
{

const std::vector<uint16_t>& quick_values()
{
    static const std::vector<uint16_t> values = { 30, 45, 60, 90, 120, 180, 240, 0 };
    return values;
}

int played_today_min(const PtState& pt)
{
    if (!pt.valid || !pt.enabled_valid || !pt.enabled) return -1;
    const uint16_t limit = pt.day_min[ui::today_weekday()];
    if (limit == PT_DAY_NOLIMIT) return -1;
    if (pt.restricted_valid && pt.restricted) return limit;
    // The remaining time reads 0 until a game has been counted today.
    if (!pt.remaining_valid || pt.remaining_ns == 0) return -1;
    const uint64_t left = pt.remaining_ns / 60000000000ULL;
    return left >= limit ? 0 : (int)(limit - left);
}

static void set_relock_pending(bool on)
{
    auto& cfg = config::get();
    if (cfg.relock_pending == on) return;
    cfg.relock_pending = on;
    ui::save_config();
}

void relock_if_interrupted()
{
    // Read-only refuses every write, the lock too: keep the record for a
    // start that can lock.
    if (!config::get().relock_pending || app::read_only()) return;
    set_relock_pending(false);
    PctlStatus st;
    pctl_status_fetch(&st);
    if (!st.temp_unlocked_ok || !st.temp_unlocked) return;
    Result rc = pctl_relock();
    ui::notify_result(rc, "playguard/toast/relocked_after_stop"_i18n, "playguard/toast/relock_err"_i18n);
}

void confirm_write(const std::string& body_in, const std::string& confirm_label,
                   std::function<void(bool did_unlock)> write, const uint16_t* new_days, bool danger)
{
    if (app::read_only()) {
        ui::notify(ui::rc_text(NXM_RC_READ_ONLY));
        return;
    }

    PtState pt;
    pctl_play_timer_query(&pt);
    if (!pt.fw_supported) {
        ui::notify(ui::rc_text(NXM_RC_FW_UNSUPPORTED));
        return;
    }
    if (!pt.valid || !pt.enabled_valid || !pt.restricted_valid || !pt.temporary_unlocked_valid) {
        ui::notify(ui::rc_text(NXM_RC_STATE_UNKNOWN));
        return;
    }

    // A limit below what was already played suspends the game at the relock.
    std::string body = body_in;
    if (new_days) {
        const int played = played_today_min(pt);
        const uint16_t next = new_days[ui::today_weekday()];
        if (played > 0 && next != PT_DAY_NOLIMIT && next < played) {
            const std::string warning = brls::getStr("playguard/play_timer/suspend_warning",
                                                     ui::fmt_minutes((uint16_t)played));
            body = body.empty() ? warning : body + "\n\n" + warning;
        }
    }

    const bool needs_unlock = (pt.enabled || pt.restricted) && !pt.temporary_unlocked;
    if (!needs_unlock) {
        if (body.empty()) write(false);
        else ui::confirm(body, confirm_label, [write]() { write(false); }, nullptr, danger);
        return;
    }

    std::string text = body.empty() ? std::string() : body + "\n\n";
    text += "playguard/play_timer/gate/body"_i18n + " " +
            (config::get().auto_relock ? "playguard/play_timer/gate/relock_auto"_i18n
                                       : "playguard/play_timer/gate/relock_manual"_i18n);
    ui::confirm(text, "playguard/play_timer/gate/confirm"_i18n, [write]() {
        // Should the app stop before finish_write, the next start locks again.
        set_relock_pending(true);
        Result rc = pctl_unlock_restriction_temporarily();
        if (R_FAILED(rc)) {
            set_relock_pending(false);   // not unlocked (or locked again by the service layer)
            ui::notify("playguard/play_timer/gate/failed"_i18n + " — " + ui::rc_text(rc));
            return;
        }
        write(true);
    }, nullptr, danger);
}

static void offer_relock(std::function<void()> after)
{
    auto* dialog = new brls::Dialog("playguard/play_timer/relock/body"_i18n);
    dialog->addButton("playguard/play_timer/relock/later"_i18n, [after]() { if (after) after(); });
    dialog->addButton("playguard/play_timer/relock/now"_i18n, [after]() {
        Result rc = pctl_relock();
        ui::notify_result(rc, "playguard/toast/relocked"_i18n, "playguard/toast/relock_err"_i18n);
        if (after) after();
    });
    ui::on_cancel(dialog, after);   // B: "Later"
    dialog->open();
}

void finish_write(Result rc, bool did_unlock, const std::string& ok_text,
                  const std::string& error_prefix, std::function<void()> refresh)
{
    if (!did_unlock) {
        ui::notify_result(rc, ok_text, error_prefix);
        if (refresh) refresh();
        return;
    }
    // The unlock was only for this write: never leave the console unlocked
    // behind the user's back, even when the write failed.
    set_relock_pending(false);
    if (config::get().auto_relock) {
        Result relock = pctl_relock();
        if (R_FAILED(rc)) ui::notify_result(rc, ok_text, error_prefix);
        else if (R_SUCCEEDED(relock)) ui::notify(ok_text + " " + "playguard/toast/relocked"_i18n);
        else ui::notify(ok_text);
        if (R_FAILED(relock)) ui::notify_result(relock, "", "playguard/toast/relock_err"_i18n);
        if (refresh) refresh();
        return;
    }
    ui::notify_result(rc, ok_text, error_prefix);
    if (refresh) refresh();
    offer_relock(refresh);
}

static void apply_uniform(uint16_t minutes, std::function<void()> refresh)
{
    std::string body = minutes == 0 ? "playguard/play_timer/confirm_uniform_zero"_i18n
                                    : brls::getStr("playguard/play_timer/confirm_uniform", ui::fmt_minutes(minutes));
    uint16_t new_days[7];
    for (auto& d : new_days) d = minutes;
    confirm_write(body, "playguard/play_timer/confirm_set"_i18n, [minutes, refresh](bool did_unlock) {
        uint16_t days[7];
        for (auto& d : days) d = minutes;
        Result rc = pctl_play_timer_set_days(days);
        finish_write(rc, did_unlock, brls::getStr("playguard/play_timer/written_uniform", ui::fmt_minutes(minutes)),
                     "playguard/play_timer/write_err"_i18n, refresh);
    }, new_days);
}

void choose_uniform_limit(const PtState& pt, std::function<void()> refresh)
{
    bool uniform = pt.valid;
    for (int i = 1; i < 7 && uniform; i++) uniform = pt.day_min[i] == pt.day_min[0];

    const auto& values = quick_values();
    std::vector<std::string> labels;
    int selected = -1;
    for (size_t i = 0; i < values.size(); i++) {
        labels.push_back(ui::fmt_minutes(values[i]));
        if (uniform && pt.day_min[0] == values[i]) selected = (int)i;
    }
    labels.push_back("playguard/common/custom"_i18n);
    // Days that differ (or a value outside the list): start on "Custom…"
    // rather than pretending the first value is the current one.
    if (selected < 0) selected = (int)labels.size() - 1;

    const std::string title = uniform ? "playguard/play_timer/quick_title"_i18n
                                      : "playguard/play_timer/quick_title_varies"_i18n;
    uint16_t seed = (pt.valid && pt.day_min[0] != PT_DAY_NOLIMIT) ? pt.day_min[0] : 60;
    ui::pick(title, labels, selected, [refresh, seed](int index) {
        const auto& values = quick_values();
        if ((size_t)index < values.size()) {
            apply_uniform(values[index], refresh);
            return;
        }
        ui::prompt_minutes("playguard/play_timer/quick_title"_i18n, seed,
                           [refresh](uint16_t v) { apply_uniform(v, refresh); });
    });
}

static std::string today_date()
{
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmv);
    return buf;
}

static void clear_extra_record()
{
    auto& cfg = config::get();
    cfg.extra_weekday = -1;
    cfg.extra_date.clear();
    ui::save_config();
}

bool can_add_extra_time(const PtState& pt)
{
    const int wd = ui::today_weekday();
    return !app::read_only() && pt.fw_supported && pt.valid && pt.enabled_valid && pt.enabled &&
           pt.day_min[wd] != PT_DAY_NOLIMIT && pt.day_min[wd] < 1440;
}

void add_extra_time(const PtState& pt, std::function<void()> refresh)
{
    const int wd = ui::today_weekday();
    if (!pt.valid || pt.day_min[wd] == PT_DAY_NOLIMIT) return;
    static const uint16_t EXTRA[] = { 15, 30, 60 };
    std::vector<std::string> labels;
    for (uint16_t e : EXTRA) labels.push_back("+" + ui::fmt_minutes(e));
    const uint16_t base = pt.day_min[wd];
    ui::pick("playguard/dashboard/extra_title"_i18n, labels, 0, [refresh, wd, base](int index) {
        const uint16_t extra = EXTRA[index];
        const uint16_t value = (uint16_t)std::min<int>(1440, base + extra);
        // What gets put back later is the limit before any extra time today.
        const auto& cfg = config::get();
        const bool again = cfg.extra_weekday == wd && cfg.extra_date == today_date();
        const uint16_t original = again ? (uint16_t)cfg.extra_base : base;
        const std::string body = brls::getStr("playguard/dashboard/extra_body", ui::fmt_minutes(extra),
                                              ui::day_name_in_text(wd), ui::fmt_minutes(base),
                                              ui::fmt_minutes(value), ui::fmt_minutes(original));
        confirm_write(body, "playguard/dashboard/extra_confirm"_i18n, [refresh, wd, base, value, original](bool did_unlock) {
            PtState now;
            pctl_play_timer_query(&now);
            if (!now.valid || now.day_min[wd] != base) {   // changed meanwhile: write nothing
                finish_write(NXM_RC_STATE_UNKNOWN, did_unlock, "", "playguard/play_timer/write_err"_i18n, refresh);
                return;
            }
            uint16_t days[7];
            for (int i = 0; i < 7; i++) days[i] = now.day_min[i];
            days[wd] = value;
            Result rc = pctl_play_timer_set_days(days);
            if (R_SUCCEEDED(rc)) {
                auto& c = config::get();
                c.extra_weekday = wd;
                c.extra_date    = today_date();
                c.extra_base    = original;
                c.extra_value   = value;
                ui::save_config();
            }
            finish_write(rc, did_unlock, brls::getStr("playguard/dashboard/extra_done", ui::fmt_minutes(value)),
                         "playguard/play_timer/write_err"_i18n, refresh);
        });
    });
}

void offer_extra_time_restore()
{
    const auto& cfg = config::get();
    if (app::read_only() || cfg.extra_weekday < 0 || cfg.extra_weekday > 6) return;
    if (cfg.extra_date == today_date()) return;   // still the day it was added

    PtState pt;
    pctl_play_timer_query(&pt);
    if (!pt.fw_supported || !pt.valid) return;    // ask again next time
    const int wd = cfg.extra_weekday;
    if (pt.day_min[wd] != cfg.extra_value) {      // changed since: nothing to put back
        clear_extra_record();
        return;
    }
    const uint16_t base = (uint16_t)cfg.extra_base, value = (uint16_t)cfg.extra_value;
    auto* dialog = new brls::Dialog(brls::getStr("playguard/dashboard/extra_restore_body", cfg.extra_date,
                                                 ui::day_name_in_text(wd), ui::fmt_minutes(value),
                                                 ui::fmt_minutes(base)));
    dialog->addButton(brls::getStr("playguard/dashboard/extra_keep", ui::fmt_minutes(value)), []() { clear_extra_record(); });
    dialog->addButton("playguard/dashboard/extra_restore"_i18n, [wd, base, value]() {
        brls::sync([wd, base, value]() {
            confirm_write("", "playguard/play_timer/confirm_set"_i18n, [wd, base, value](bool did_unlock) {
                PtState now;
                pctl_play_timer_query(&now);
                if (!now.valid || now.day_min[wd] != value) {
                    clear_extra_record();
                    finish_write(NXM_RC_STATE_UNKNOWN, did_unlock, "", "playguard/play_timer/write_err"_i18n, nullptr);
                    return;
                }
                uint16_t days[7];
                for (int i = 0; i < 7; i++) days[i] = now.day_min[i];
                days[wd] = base;
                Result rc = pctl_play_timer_set_days(days);
                if (R_SUCCEEDED(rc)) clear_extra_record();
                finish_write(rc, did_unlock,
                             brls::getStr("playguard/dashboard/extra_restored", ui::day_name_in_text(wd), ui::fmt_minutes(base)),
                             "playguard/play_timer/write_err"_i18n, nullptr);
            });
        });
    });
    // B: decide at the next start (the record stays).
    dialog->setCancelable(true);
    dialog->open();
}

}   // namespace pt_flow
