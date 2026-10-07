// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "action/pt_flow.hpp"

#include <borealis.hpp>

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

void confirm_write(const std::string& body, const std::string& confirm_label,
                   std::function<void(bool did_unlock)> write)
{
    if (app::read_only_build()) {
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

    const bool needs_unlock = (pt.enabled || pt.restricted) && !pt.temporary_unlocked;
    if (!needs_unlock) {
        if (body.empty()) write(false);
        else ui::confirm(body, confirm_label, [write]() { write(false); });
        return;
    }

    std::string text = body.empty() ? std::string() : body + "\n\n";
    text += "playguard/play_timer/gate/body"_i18n + " " +
            (config::get().auto_relock ? "playguard/play_timer/gate/relock_auto"_i18n
                                       : "playguard/play_timer/gate/relock_manual"_i18n);
    ui::confirm(text, "playguard/play_timer/gate/confirm"_i18n, [write]() {
        Result rc = pctl_unlock_restriction_temporarily();
        if (R_FAILED(rc)) {
            ui::notify("playguard/play_timer/gate/failed"_i18n + " — " + ui::rc_text(rc));
            return;
        }
        write(true);
    });
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
    confirm_write(body, "playguard/play_timer/confirm_set"_i18n, [minutes, refresh](bool did_unlock) {
        uint16_t days[7];
        for (auto& d : days) d = minutes;
        Result rc = pctl_play_timer_set_days(days);
        finish_write(rc, did_unlock, brls::getStr("playguard/play_timer/written_uniform", ui::fmt_minutes(minutes)),
                     "playguard/play_timer/write_err"_i18n, refresh);
    });
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

}   // namespace pt_flow
