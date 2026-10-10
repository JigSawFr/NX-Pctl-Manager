// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "action/pt_flow.hpp"

#include <array>
#include <borealis.hpp>
#include <fmt/format.h>

#include "action/console_lock.hpp"
#include "action/history_flow.hpp"
#include "action/pt_logic.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "view/pt_week.hpp"

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
    return pt_logic::played_today_min(pt, ui::today_weekday());
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
    PctlStatus st;
    pctl_status_fetch(&st);
    if (!st.temp_unlocked_ok) return;   // state unknown: keep the record, try again next time
    if (!st.temp_unlocked) {
        set_relock_pending(false);
        return;
    }
    Result rc = pctl_relock();
    // The record goes only once the console is locked again.
    if (R_SUCCEEDED(rc)) set_relock_pending(false);
    ui::notify_result(rc, "playguard/toast/relocked_after_stop"_i18n, "playguard/toast/relock_err"_i18n);
}

// After an unlock attempt that failed: drop the record only when the console
// reads back as locked (the unlock may have gone through and its lock failed).
static void settle_failed_unlock()
{
    PctlStatus st;
    pctl_status_fetch(&st);
    if (st.temp_unlocked_ok && !st.temp_unlocked) set_relock_pending(false);
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
    if (!pt_logic::state_known(pt)) {
        ui::notify(ui::rc_text(NXM_RC_STATE_UNKNOWN));
        return;
    }

    // A limit below what was already played suspends the game at the relock.
    std::string body = body_in;
    const int played = pt_logic::suspend_warning_min(pt, ui::today_weekday(), new_days);
    if (played > 0) {
        const std::string warning = brls::getStr("playguard/play_timer/suspend_warning", ui::fmt_minutes((uint16_t)played));
        body = body.empty() ? warning : body + "\n\n" + warning;
    }

    // The week as it will be: the days that change in amber.
    auto preview = [&pt, new_days]() -> brls::View* {
        if (!new_days || !pt.valid) return nullptr;
        auto* week = new PtWeekView();
        week->show(new_days, pt.day_min);
        return week;
    };

    if (!pt_logic::needs_unlock(pt)) {
        if (body.empty()) write(false);
        else if (brls::View* week = preview()) ui::confirm_with(body, week, confirm_label, [write]() { write(false); }, nullptr, danger);
        else ui::confirm(body, confirm_label, [write]() { write(false); }, nullptr, danger);
        return;
    }

    std::string text = body.empty() ? std::string() : body + "\n\n";
    text += "playguard/play_timer/gate/body"_i18n + " " +
            (config::get().auto_relock ? "playguard/play_timer/gate/relock_auto"_i18n
                                       : "playguard/play_timer/gate/relock_manual"_i18n);
    auto unlock_and_write = [write]() {
        // Should the app stop before finish_write, the next start locks again.
        set_relock_pending(true);
        Result rc = pctl_unlock_restriction_temporarily();
        if (R_FAILED(rc)) {
            settle_failed_unlock();   // not unlocked, or locked again by the service layer
            ui::error("playguard/play_timer/gate/failed"_i18n + " — " + ui::rc_text(rc));
            return;
        }
        write(true);
    };
    if (brls::View* week = preview()) ui::confirm_with(text, week, "playguard/play_timer/gate/confirm"_i18n, unlock_and_write, nullptr, danger);
    else ui::confirm(text, "playguard/play_timer/gate/confirm"_i18n, unlock_and_write, nullptr, danger);
}

// Limits written by anything but the console lock replace it: its record
// would otherwise put older limits back when it is turned off.
static void limits_replaced(const std::string& source)
{
    if (source != "console_lock") console_lock::forget();
}

Result write_days(const uint16_t days[7], const std::string& source, const std::string& detail)
{
    PtState before;
    pctl_play_timer_query(&before);
    const Result rc = pctl_play_timer_set_days(days);
    if (R_SUCCEEDED(rc)) limits_replaced(source);
    if (R_SUCCEEDED(rc) && before.valid)
        history_flow::record_values("limits", history_flow::days_values(before.day_min), history_flow::days_values(days),
                                    source, detail);
    return rc;
}

Result clear_days(const std::string& source)
{
    PtState before;
    pctl_play_timer_query(&before);
    const Result rc = pctl_play_timer_clear();
    if (R_SUCCEEDED(rc)) limits_replaced(source);
    if (R_SUCCEEDED(rc) && before.valid) {
        uint16_t none[7];
        for (auto& d : none) d = PT_DAY_NOLIMIT;
        history_flow::record_values("limits", history_flow::days_values(before.day_min), history_flow::days_values(none), source);
    }
    return rc;
}

static void offer_relock(std::function<void()> after)
{
    auto* dialog = ui::dialog("playguard/play_timer/relock/body"_i18n);
    // "Later" is a choice to stay unlocked: the record goes. Closed before
    // an answer, it stays, and the next start locks again.
    auto later = [after]() {
        set_relock_pending(false);
        if (after) after();
    };
    dialog->addButton("playguard/play_timer/relock/later"_i18n, later);
    dialog->addButton("playguard/play_timer/relock/now"_i18n, [after]() {
        Result rc = pctl_relock();
        if (R_SUCCEEDED(rc)) set_relock_pending(false);
        ui::notify_result(rc, "playguard/toast/relocked"_i18n, "playguard/toast/relock_err"_i18n);
        if (after) after();
    });
    ui::on_cancel(dialog, later);   // B: "Later"
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
    // behind the user's back, even when the write failed. The record of the
    // unlock stays until the console is locked again (or the user says later).
    if (config::get().auto_relock) {
        Result relock = pctl_relock();
        if (R_SUCCEEDED(relock)) set_relock_pending(false);
        if (R_SUCCEEDED(rc) && R_SUCCEEDED(relock)) {
            ui::notify(ok_text + " " + "playguard/toast/relocked"_i18n);
        } else {
            // One dialog says it all: what failed, and what went through.
            std::string text;
            if (R_FAILED(rc)) text = error_prefix + " — " + ui::rc_text(rc);
            else if (!ok_text.empty()) text = ok_text;
            if (R_FAILED(relock)) text += (text.empty() ? "" : "\n\n") + "playguard/toast/relock_err"_i18n + " — " + ui::rc_text(relock);
            ui::error(text);
        }
        if (refresh) refresh();
        return;
    }
    ui::notify_result(rc, ok_text, error_prefix);
    if (refresh) refresh();
    offer_relock(refresh);
}

static std::string hm(int hour, int minute)
{
    return fmt::format("{:02d}:{:02d}", hour, minute);
}

static std::string bed_text(const PtBedtime& b)
{
    return b.on ? brls::getStr("playguard/play_timer/bedtime_summary", hm(b.hour, b.minute), hm(b.end_hour, b.end_minute))
                : "playguard/common/off"_i18n;
}

std::string bedtime_text(const PtState& pt)
{
    PtBedtime b;
    if (!pt_logic::bedtime_uniform(pt, &b)) return "playguard/play_timer/bedtime_varies"_i18n;
    return bed_text(b);
}

Result write_bedtime(const PtBedtime bed[7], const std::string& source)
{
    PtState before;
    pctl_play_timer_query(&before);
    const Result rc = pctl_play_timer_set_bedtime(bed, ui::today_weekday());
    if (R_SUCCEEDED(rc) && before.valid) {
        PtState after = before;
        for (int n = 0; n < 7; n++) after.bed[n] = bed[n];
        const std::string from = bedtime_text(before), to = bedtime_text(after);
        if (from != to) history_flow::record_event("bedtime", source, from + " → " + to);
    }
    return rc;
}

// The bedtime can be changed from this state; says why not otherwise.
static bool bedtime_editable(const PtState& pt)
{
    if (ui::refuse_read_only()) return false;
    if (!pt.fw_supported) {
        ui::notify(ui::rc_text(NXM_RC_FW_UNSUPPORTED));
        return false;
    }
    if (!pt_logic::bedtime_layout_ok(pt, ui::today_weekday())) {
        ui::info("playguard/play_timer/bedtime_unknown"_i18n);
        return false;
    }
    return true;
}

static void apply_bedtime(const std::array<PtBedtime, 7>& bed, const std::string& body, std::function<void()> refresh)
{
    confirm_write(body, "playguard/play_timer/confirm_set"_i18n, [bed, refresh](bool did_unlock) {
        Result rc = write_bedtime(bed.data(), "");
        finish_write(rc, did_unlock, "playguard/play_timer/bedtime_written"_i18n,
                     "playguard/play_timer/bedtime_err"_i18n, refresh);
    });
}

void choose_bedtime(const PtState& pt, std::function<void()> refresh)
{
    if (!bedtime_editable(pt)) return;
    // Off, then 16:00 to 23:45 every quarter of an hour.
    std::vector<std::string> labels = { "playguard/common/off"_i18n };
    std::vector<int> times = { -1 };
    for (int t = 16 * 60; t < 24 * 60; t += 15) {
        labels.push_back(hm(t / 60, t % 60));
        times.push_back(t);
    }
    PtBedtime now;
    int selected = 0;
    if (pt_logic::bedtime_uniform(pt, &now) && now.on) {
        selected = -1;
        for (size_t i = 1; i < times.size(); i++)
            if (times[i] == now.hour * 60 + now.minute) selected = (int)i;
        if (selected < 0) selected = 0;
    } else if (!pt_logic::bedtime_uniform(pt, nullptr)) {
        selected = 21 * 4 - 16 * 4 + 1;   // days differ: start on 21:00
    }
    const PtState copy = pt;
    ui::pick("playguard/play_timer/bedtime_title"_i18n, labels, selected, [copy, times, refresh](int index) {
        const int t = times[(size_t)index];
        std::array<PtBedtime, 7> bed;
        pt_logic::bedtime_every_day(copy, t >= 0, (uint8_t)(t >= 0 ? t / 60 : 0), (uint8_t)(t >= 0 ? t % 60 : 0), bed.data());
        const std::string body = t < 0 ? "playguard/play_timer/bedtime_confirm_off"_i18n
                                       : brls::getStr("playguard/play_timer/bedtime_confirm", hm(t / 60, t % 60),
                                                      hm(bed[0].end_hour, bed[0].end_minute));
        apply_bedtime(bed, body, refresh);
    });
}

void choose_bedtime_end(const PtState& pt, std::function<void()> refresh)
{
    if (!bedtime_editable(pt)) return;
    std::array<PtBedtime, 7> probe;
    if (!pt_logic::bedtime_end_every_day(pt, 6, 0, probe.data())) {
        ui::info("playguard/play_timer/bedtime_end_needs_alarm"_i18n);
        return;
    }
    // 05:00 to 09:00 every quarter of an hour.
    std::vector<std::string> labels;
    std::vector<int> times;
    for (int t = 5 * 60; t <= 9 * 60; t += 15) {
        labels.push_back(hm(t / 60, t % 60));
        times.push_back(t);
    }
    PtBedtime now;
    int selected = 4;   // 06:00
    if (pt_logic::bedtime_uniform(pt, &now))
        for (size_t i = 0; i < times.size(); i++)
            if (times[i] == now.end_hour * 60 + now.end_minute) selected = (int)i;
    const PtState copy = pt;
    ui::pick("playguard/play_timer/bedtime_end_title"_i18n, labels, selected, [copy, times, refresh](int index) {
        const int t = times[(size_t)index];
        std::array<PtBedtime, 7> bed;
        pt_logic::bedtime_end_every_day(copy, (uint8_t)(t / 60), (uint8_t)(t % 60), bed.data());
        apply_bedtime(bed, brls::getStr("playguard/play_timer/bedtime_end_confirm", hm(t / 60, t % 60)), refresh);
    });
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
        Result rc = write_days(days, "uniform");
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

void pick_limit(const std::string& title, uint16_t current, std::function<void(uint16_t)> on_value)
{
    // Quick values first (as in "Same limit every day"), then any value, then
    // no limit; the current value is pre-selected.
    const auto& values = quick_values();
    std::vector<std::string> options;
    int selected = -1;
    for (size_t i = 0; i < values.size(); i++) {
        options.push_back(ui::fmt_minutes(values[i]));
        if (values[i] == current) selected = (int)i;
    }
    const int custom_index  = (int)options.size();
    const int nolimit_index = custom_index + 1;
    options.push_back("playguard/play_timer/perday/pick_minutes"_i18n);
    options.push_back("playguard/play_timer/perday/pick_no_limit"_i18n);
    if (current == PT_DAY_NOLIMIT) selected = nolimit_index;
    else if (selected < 0) selected = custom_index;

    ui::pick(title, options, selected, [title, current, on_value, custom_index, nolimit_index](int index) {
        if (index == nolimit_index) {
            on_value(PT_DAY_NOLIMIT);
        } else if (index == custom_index) {
            const uint16_t seed = current == PT_DAY_NOLIMIT ? 60 : current;
            ui::prompt_minutes(title, seed, [on_value](uint16_t v) { on_value(v); });
        } else {
            on_value(quick_values()[index]);
        }
    });
}

void change_day_limit(int day, uint16_t current, std::function<void()> refresh)
{
    if (day < 0 || day > 6) return;
    pick_limit(brls::getStr("playguard/play_timer/perday/pick_title", ui::day_name(day)), current,
               [day, current, refresh](uint16_t minutes) {
        if (minutes == current) return;
        // The other days as the console has them now (the tab's copy may be
        // a few seconds old), only this one changed.
        PtState now;
        pctl_play_timer_query(&now);
        if (!now.valid) {
            ui::error(ui::rc_text(NXM_RC_STATE_UNKNOWN));
            return;
        }
        std::array<uint16_t, 7> days;
        for (int i = 0; i < 7; i++) days[i] = now.day_min[i];
        days[day] = minutes;
        const std::string body = brls::getStr("playguard/play_timer/confirm_day", ui::day_name_in_text(day), ui::fmt_minutes(minutes));
        confirm_write(body, "playguard/play_timer/confirm_set"_i18n, [days, refresh](bool did_unlock) {
            Result rc = write_days(days.data(), "day");
            finish_write(rc, did_unlock, "playguard/play_timer/written_days"_i18n,
                         "playguard/play_timer/write_err"_i18n, refresh);
        }, days.data());
    });
}

// Extra time and "no more play" are for one day of the usual limits: with
// every day at 0 they would open a hole in the console lock.
static bool refuse_console_lock()
{
    if (!console_lock::active()) return false;
    ui::info("playguard/console_lock/blocks_change"_i18n);
    return true;
}

static void clear_extra_record()
{
    auto& cfg = config::get();
    cfg.extra_weekday = -1;
    cfg.extra_date.clear();
    ui::save_config();
}

static pt_logic::ExtraRecord extra_record()
{
    const auto& cfg = config::get();
    pt_logic::ExtraRecord rec;
    rec.weekday = cfg.extra_weekday;
    rec.date    = cfg.extra_date;
    rec.base    = (uint16_t)cfg.extra_base;
    rec.value   = (uint16_t)cfg.extra_value;
    return rec;
}

// What to put back later, saved *before* the write it describes: should the
// app stop between the two, the limit still comes back. False when it could
// not be saved, and then nothing must be written (it would never be put back).
static bool save_extra_record(int wd, uint16_t base, uint16_t value)
{
    auto& c = config::get();
    c.extra_weekday = wd;
    c.extra_date    = ui::today_date();
    c.extra_base    = base;
    c.extra_value   = value;
    return ui::save_config();
}

// The write failed: the record it was saved for goes back to what it was.
static void put_back_extra_record(const pt_logic::ExtraRecord& prev)
{
    auto& c = config::get();
    c.extra_weekday = prev.weekday;
    c.extra_date    = prev.date;
    c.extra_base    = prev.base;
    c.extra_value   = prev.value;
    ui::save_config();
}

// A list of actions, not of values: no row is checked as the current one
// (borealis' Dropdown checks one, ui::pick the `selected`). The first row
// takes the focus.
class ActionList : public brls::Dropdown
{
  public:
    ActionList(const std::string& title, const std::vector<std::string>& values, std::function<void(int)> on_pick)
        : brls::Dropdown(
              title, values, [this](int index) { this->chosen = index; }, 0,
              [this, on_pick](int) {
                  if (this->chosen >= 0 && on_pick) {
                      const int index = this->chosen;
                      brls::sync([on_pick, index]() { on_pick(index); });
                  }
              }),
          labels(values)
    {
        // The rows Dropdown's constructor already made used its own
        // cellForRow (this one did not exist yet): unchecked here.
        uncheck(this);
    }

  private:
    std::vector<std::string> labels;
    int chosen = -1;

    static void uncheck(brls::View* view)
    {
        if (auto* cell = dynamic_cast<brls::RadioCell*>(view)) cell->setSelected(false);
        if (auto* box = dynamic_cast<brls::Box*>(view))
            for (brls::View* child : box->getChildren()) uncheck(child);
    }

    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        auto* cell = (brls::RadioCell*)recycler->dequeueReusableCell("Cell");
        cell->title->setText(this->labels[index.row]);
        cell->setSelected(false);
        return cell;
    }
};

void add_extra_time(const PtState& pt, std::function<void()> refresh)
{
    if (refuse_console_lock()) return;
    const int wd = ui::today_weekday();
    if (!pt.valid || pt.day_min[wd] == PT_DAY_NOLIMIT) return;
    // Tools › Extra time amounts.
    const std::vector<int> amounts = config::get().extra_amounts;
    std::vector<std::string> labels;
    for (int e : amounts) labels.push_back("+" + ui::fmt_minutes((uint16_t)e));
    const uint16_t base = pt.day_min[wd];
    auto* list = new ActionList("playguard/dashboard/extra_title"_i18n, labels, [refresh, wd, base, amounts](int index) {
        const uint16_t extra = (uint16_t)amounts[index];
        // What gets put back later is the limit before any extra time today.
        const pt_logic::ExtraRecord rec = extra_record();
        const bool again = rec.weekday == wd && rec.date == ui::today_date();
        const pt_logic::ExtraPlan plan = pt_logic::plan_extra(base, extra, again, rec.base);
        const uint16_t value = plan.value, original = plan.original;
        const std::string body = brls::getStr(config::get().extra_auto_restore ? "playguard/dashboard/extra_body_auto"
                                                                               : "playguard/dashboard/extra_body",
                                              ui::fmt_minutes(extra), ui::day_name_in_text(wd), ui::fmt_minutes(base),
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
            const pt_logic::ExtraRecord prev = extra_record();
            if (!save_extra_record(wd, original, value)) {
                finish_write(NXM_RC_NOT_SAVED, did_unlock, "", "playguard/play_timer/write_err"_i18n, refresh);
                return;
            }
            Result rc = write_days(days, "extra");
            if (R_FAILED(rc)) put_back_extra_record(prev);
            finish_write(rc, did_unlock, brls::getStr("playguard/dashboard/extra_done", ui::fmt_minutes(value)),
                         "playguard/play_timer/write_err"_i18n, refresh);
        });
    });
    brls::Application::pushActivity(new brls::Activity(list));
}

void stop_today(const PtState& pt, std::function<void()> refresh)
{
    if (refuse_console_lock()) return;
    const int wd = ui::today_weekday();
    if (!pt.valid) return;
    const uint16_t base = pt.day_min[wd];
    const pt_logic::ExtraRecord rec = extra_record();
    const bool again = rec.weekday == wd && rec.date == ui::today_date();
    const pt_logic::ExtraPlan plan = pt_logic::plan_stop(base, again, rec.base);
    const uint16_t original = plan.original;
    uint16_t new_days[7];
    for (int i = 0; i < 7; i++) new_days[i] = pt.day_min[i];
    new_days[wd] = 0;
    const std::string body = brls::getStr(config::get().extra_auto_restore ? "playguard/dashboard/stop_body_auto"
                                                                           : "playguard/dashboard/stop_body",
                                          ui::day_name_in_text(wd), ui::fmt_minutes(original));
    confirm_write(body, "playguard/dashboard/stop_confirm"_i18n, [refresh, wd, base, original](bool did_unlock) {
        PtState now;
        pctl_play_timer_query(&now);
        if (!now.valid || now.day_min[wd] != base) {   // changed meanwhile: write nothing
            finish_write(NXM_RC_STATE_UNKNOWN, did_unlock, "", "playguard/play_timer/write_err"_i18n, refresh);
            return;
        }
        uint16_t days[7];
        for (int i = 0; i < 7; i++) days[i] = now.day_min[i];
        days[wd] = 0;
        const pt_logic::ExtraRecord prev = extra_record();
        if (!save_extra_record(wd, original, 0)) {
            finish_write(NXM_RC_NOT_SAVED, did_unlock, "", "playguard/play_timer/write_err"_i18n, refresh);
            return;
        }
        Result rc = write_days(days, "stop");
        if (R_FAILED(rc)) put_back_extra_record(prev);
        finish_write(rc, did_unlock, "playguard/dashboard/stop_done"_i18n, "playguard/play_timer/write_err"_i18n, refresh);
    }, new_days, true);
}

// Puts `base` back on weekday `wd`, if it still holds the extra time (`value`).
static void restore_extra(int wd, uint16_t base, uint16_t value, std::function<void()> refresh)
{
    confirm_write("", "playguard/play_timer/confirm_set"_i18n, [wd, base, value, refresh](bool did_unlock) {
        PtState now;
        pctl_play_timer_query(&now);
        if (!now.valid || now.day_min[wd] != value) {
            // Changed meanwhile: nothing left to put back. Unreadable: the
            // record stays, to try again (pt_logic::restore_action says Later).
            if (now.valid) clear_extra_record();
            finish_write(NXM_RC_STATE_UNKNOWN, did_unlock, "", "playguard/play_timer/write_err"_i18n, refresh);
            return;
        }
        uint16_t days[7];
        for (int i = 0; i < 7; i++) days[i] = now.day_min[i];
        days[wd] = base;
        Result rc = write_days(days, "restore_extra");
        if (R_SUCCEEDED(rc)) clear_extra_record();
        finish_write(rc, did_unlock,
                     brls::getStr("playguard/dashboard/extra_restored", ui::day_name_in_text(wd), ui::fmt_minutes(base)),
                     "playguard/play_timer/write_err"_i18n, refresh);
    });
}

bool restore_pending(const PtState& pt)
{
    if (console_lock::active()) return false;   // put back once the lock is off
    return pt_logic::restore_action(extra_record(), ui::today_date(), pt, app::read_only()) == pt_logic::Restore::Offer;
}

std::string extra_today_text(const PtState& pt)
{
    const pt_logic::ExtraRecord rec = extra_record();
    const int wd = ui::today_weekday();
    if (rec.weekday != wd || rec.date != ui::today_date() || !pt.valid || pt.day_min[wd] != rec.value ||
        rec.value <= rec.base)
        return "";
    return "+" + ui::fmt_minutes((uint16_t)(rec.value - rec.base));
}

std::string restore_label()
{
    const pt_logic::ExtraRecord rec = extra_record();
    if (rec.weekday < 0 || rec.weekday > 6) return "";
    return brls::getStr("playguard/dashboard/extra_pending", ui::day_name_in_text(rec.weekday), ui::fmt_minutes(rec.base));
}

void offer_extra_time_restore(std::function<void()> refresh)
{
    const pt_logic::ExtraRecord rec = extra_record();
    if (app::read_only() || rec.weekday < 0 || rec.weekday > 6 || rec.date == ui::today_date()) return;
    // The lock holds every day at 0; the record waits for its limits to come back.
    if (console_lock::active()) return;

    PtState pt;
    pctl_play_timer_query(&pt);
    switch (pt_logic::restore_action(rec, ui::today_date(), pt, app::read_only())) {
        case pt_logic::Restore::None:
        case pt_logic::Restore::Later:  return;                       // ask again next time
        case pt_logic::Restore::Forget: clear_extra_record(); return;  // changed since
        case pt_logic::Restore::Offer:  break;
    }
    const int wd = rec.weekday;
    const uint16_t base = rec.base, value = rec.value;
    if (config::get().extra_auto_restore) {   // no question (the unlock, if needed, still asks)
        restore_extra(wd, base, value, refresh);
        return;
    }
    // Raised (extra time) or lowered (no more play) for that day only.
    auto* dialog = ui::dialog(brls::getStr(value > base ? "playguard/dashboard/extra_restore_body"
                                                        : "playguard/dashboard/stop_restore_body",
                                           rec.date, ui::day_name_in_text(wd), ui::fmt_minutes(value),
                                           ui::fmt_minutes(base)));
    dialog->addButton(brls::getStr("playguard/dashboard/extra_keep", ui::fmt_minutes(value)), [refresh]() {
        clear_extra_record();
        if (refresh) refresh();
    });
    dialog->addButton("playguard/dashboard/extra_restore"_i18n, [wd, base, value, refresh]() {
        brls::sync([wd, base, value, refresh]() { restore_extra(wd, base, value, refresh); });
    });
    // B: decide later (the record stays; the Overview keeps a line for it).
    dialog->setCancelable(true);
    dialog->open();
}

bool alarm_off(const PtState& pt)
{
    return pt.fw_supported && pt.enabled_valid && pt.enabled && pt.alarm_disabled_valid && pt.alarm_disabled;
}

Result write_alarm_disabled(bool disabled, const std::string& source)
{
    PtState before;
    pctl_play_timer_query(&before);
    const Result rc = pctl_play_timer_set_alarm_disabled(disabled);
    if (R_SUCCEEDED(rc) && before.alarm_disabled_valid)
        history_flow::record_values("alarm", { before.alarm_disabled ? 1 : 0 }, { disabled ? 1 : 0 }, source);
    return rc;
}

void set_alarm(bool on, const std::string& source, std::function<void()> refresh)
{
    if (ui::refuse_read_only()) return;
    const std::string body = on ? "playguard/play_timer/alarm_on_body"_i18n : "playguard/play_timer/alarm_off_body"_i18n;
    const std::string label = on ? "playguard/play_timer/alarm_on_confirm"_i18n : "playguard/play_timer/alarm_off_confirm"_i18n;
    confirm_write(body, label, [on, source, refresh](bool did_unlock) {
        const Result rc = write_alarm_disabled(!on, source);
        finish_write(rc, did_unlock, on ? "playguard/play_timer/alarm_on_done"_i18n : "playguard/common/applied"_i18n,
                     "playguard/play_timer/write_err"_i18n, refresh);
    }, nullptr, !on);
}

void turn_alarm_on(const std::string& source, std::function<void()> refresh)
{
    set_alarm(true, source, refresh);
}

void set_countdown(bool running, std::function<void()> refresh)
{
    if (ui::refuse_read_only()) return;
    const std::string body = running ? "playguard/play_timer/resume_body"_i18n : "playguard/play_timer/pause_body"_i18n;
    const std::string label = running ? "playguard/play_timer/resume_confirm"_i18n : "playguard/play_timer/pause_confirm"_i18n;
    confirm_write(body, label, [running, refresh](bool did_unlock) {
        const Result rc = running ? pctl_play_timer_start() : pctl_play_timer_stop();
        finish_write(rc, did_unlock, "playguard/common/applied"_i18n, "playguard/play_timer/write_err"_i18n, refresh);
    }, nullptr, !running);
}

}   // namespace pt_flow
