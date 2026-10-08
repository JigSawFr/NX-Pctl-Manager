// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/onboarding_activity.hpp"

#include "action/clock_flow.hpp"
#include "action/fw_gate.hpp"
#include "action/pt_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/pctl_ops_c.hpp"

using namespace brls::literals;

bool OnboardingActivity::wanted_at_start()
{
    if (app::read_only() || fw_gate::needed()) return false;
    PctlStatus s;
    pctl_status_fetch(&s);
    return s.pin_length_ok && s.pin_length == 0;
}

void OnboardingActivity::onContentAvailable()
{
    headline->setSingleLine(false);
    paired->setSingleLine(false);
    note->setSingleLine(false);

    pin->registerClickAction([this](brls::View*) {
        // Blocks while the system PIN screen is shown.
        Result rc = pctl_set_pin();
        brls::Logger::info("pctl_set_pin returned 0x{:08X}", (unsigned)rc);
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
                       : has_pin ? brls::getStr("playguard/dashboard/pin_set", (int)s.pin_length) : todo);
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

    const bool done = has_pin && (!pt.fw_supported || any_limit) && (R_FAILED(accuracy_rc) || accurate);
    headline->setText(done ? "playguard/onboarding/all_done"_i18n : "playguard/onboarding/headline"_i18n);
    ui::set_visible(paired.getView(), s.pairing_active_ok && s.pairing_active);
}
