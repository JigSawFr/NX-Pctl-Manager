// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/onboarding_activity.hpp"

#include "action/clock_flow.hpp"
#include "action/history_flow.hpp"
#include "action/fw_gate.hpp"
#include "action/pin_lock.hpp"
#include "action/pt_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/pctl_ops_c.hpp"
#include "view/funding.hpp"

using namespace brls::literals;

bool OnboardingActivity::wanted_at_start()
{
    if (!config::get().onboarding_at_start || app::read_only() || fw_gate::needed()) return false;
    PctlStatus s;
    pctl_status_fetch(&s);
    return s.pin_length_ok && s.pin_length == 0;
}

void OnboardingActivity::onContentAvailable()
{
    headline->setSingleLine(false);
    note->setSingleLine(false);
    at_start->init("playguard/onboarding/at_start"_i18n, config::get().onboarding_at_start, [](bool on) {
        config::get().onboarding_at_start = on;
        ui::save_config();
    });
    unlink->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        // Developer tools: the block comparison needs the link, so say it goes.
        std::string body = "playguard/pairing/unlink_body"_i18n;
        if (config::get().dev_mode) body += "\n\n" + "playguard/pairing/unlink_dev_note"_i18n;
        ui::confirm_danger(body, "playguard/pairing/unlink_confirm"_i18n, [this]() {
            Result rc = pctl_delete_pairing();
            if (R_SUCCEEDED(rc)) history_flow::record_event("unlink", "first_steps");
            ui::notify_result(rc, "playguard/pairing/unlinked"_i18n, "playguard/pairing/unlink_err"_i18n);
            this->refresh();
        });
        return true;
    });
    alarm->setDetailText("playguard/dashboard/alarm_off"_i18n);   // the title says why
    alarm->setDetailTextColor(ui::color_warn());
    alarm->registerClickAction([this](brls::View*) {
        pt_flow::turn_alarm_on("first_steps", [this]() { this->refresh(); });
        return true;
    });

    // Security's own choice: the PIN is the console's, this keeps a child out
    // of PlayGuard itself.
    protect->registerClickAction([this](brls::View*) {
        pin_lock::choose([this]() { this->refresh(); });
        return true;
    });

    pin->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        // Blocks while the system PIN screen is shown.
        Result rc = pctl_set_pin();
        brls::Logger::info("pctl_set_pin returned 0x{:08X}", (unsigned)rc);
        if (R_SUCCEEDED(rc)) history_flow::record_event("pin", "first_steps");
        ui::notify_result(rc, "playguard/security/pin_ok"_i18n, "playguard/security/pin_err"_i18n);
        this->refresh();
        return true;
    });
    limit->registerClickAction([this](brls::View*) {
        PtState pt;
        pctl_play_timer_query(&pt);
        if (!pt.fw_supported) {
            ui::error(ui::rc_text(NXM_RC_FW_UNSUPPORTED));
            return true;
        }
        pt_flow::choose_uniform_limit(pt, [this]() { this->refresh(); });
        return true;
    });
    clock->registerClickAction([this](brls::View*) {
        if (this->clock_inaccurate) clock_flow::guided([this]() { this->refresh(); });
        else ui::info("playguard/clock/guided_fine"_i18n);
        return true;
    });
    close->registerClickAction([](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
    // Not a step: below Close, for whoever wants it.
    support->setDetailText("GitHub Sponsors · Ko-fi");
    support->registerClickAction([](brls::View*) {
        funding::open_dialog();
        return true;
    });
    this->refresh();
}

void OnboardingActivity::willAppear(bool resetState)
{
    brls::Activity::willAppear(resetState);
    this->refresh();
}

void OnboardingActivity::refresh()
{
    PctlStatus s;
    pctl_status_fetch(&s);
    PtState pt;
    pctl_play_timer_query(&pt);
    bool accurate = false;
    const Result accuracy_rc = time_network_accuracy(&accurate);
    const std::string todo = "playguard/onboarding/todo"_i18n;

    const bool has_pin = s.pin_length_ok && s.pin_length > 0;
    pin->setDetailText(!s.pin_length_ok ? "playguard/common/unavailable"_i18n
                       : has_pin ? "playguard/dashboard/pin_set"_i18n : todo);
    pin->setDetailTextColor(has_pin ? ui::color_ok() : ui::color_warn());

    bool any_limit = false;
    for (int d = 0; d < 7 && pt.valid; d++) any_limit |= pt.day_min[d] != PT_DAY_NOLIMIT;
    if (!pt.fw_supported) {
        limit->setDetailText("playguard/play_timer/fw_too_old_short"_i18n);
        limit->setDetailTextColor(ui::color_neutral());
    } else if (!pt.valid) {
        limit->setDetailText("playguard/common/unavailable"_i18n);
        limit->setDetailTextColor(ui::color_neutral());
    } else {
        limit->setDetailText(any_limit ? ui::days_summary(pt.day_min) : todo);
        limit->setDetailTextColor(any_limit ? ui::color_ok() : ui::color_warn());
    }

    this->clock_inaccurate = R_SUCCEEDED(accuracy_rc) && !accurate;
    clock->setDetailText(R_FAILED(accuracy_rc) ? "playguard/common/unavailable"_i18n
                         : accurate ? "playguard/dashboard/clock_ok"_i18n : "playguard/dashboard/clock_bad"_i18n);
    clock->setDetailTextColor(R_FAILED(accuracy_rc) ? ui::color_neutral() : accurate ? ui::color_ok() : ui::color_warn());

    // Step 4 only while the phone app is linked: its next sync would undo the rest.
    const bool paired = s.pairing_active_ok && s.pairing_active;
    unlink->setDetailText("playguard/dashboard/pairing_on"_i18n);
    unlink->setDetailTextColor(ui::color_warn());
    ui::set_visible(unlink.getView(), paired);

    // Then, while the timer runs with its alarm off: nothing says time is up.
    const bool alarm_off = pt_flow::alarm_off(pt);
    alarm->setText(brls::getStr("playguard/onboarding/step_alarm", paired ? 5 : 4));
    ui::set_visible(alarm.getView(), alarm_off);

    // Last, once a PIN is set: Ask for the PIN (any mode but "Never").
    const bool protected_ = config::get().pin_lock != "off";
    protect->setText(brls::getStr("playguard/onboarding/step_protect", 4 + (paired ? 1 : 0) + (alarm_off ? 1 : 0)));
    protect->setDetailText(protected_ ? pin_lock::mode_text() : todo);
    protect->setDetailTextColor(protected_ ? ui::color_ok() : ui::color_warn());
    ui::set_visible(protect.getView(), has_pin);

    const bool done = has_pin && (!pt.fw_supported || any_limit) && (R_FAILED(accuracy_rc) || accurate) && !paired
                      && !alarm_off && protected_;
    headline->setText(done ? "playguard/onboarding/all_done"_i18n : "playguard/onboarding/headline"_i18n);
}
