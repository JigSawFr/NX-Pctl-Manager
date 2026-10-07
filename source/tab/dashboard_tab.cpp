// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/dashboard_tab.hpp"

#include <fmt/format.h>

#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

DashboardTab::DashboardTab()
    : TabBase("xml/tab/dashboard.xml")
{
    this->timer.setPeriod(5000);
    this->timer.setCallback([this]() {
        if (app::in_focus()) this->refresh();
    });
    this->timer.start();
}

DashboardTab::~DashboardTab()
{
    this->timer.stop();
}

void DashboardTab::refresh()
{
    const std::string na = "nx_pctl/common/unavailable"_i18n;

    PctlStatus s;
    pctl_status_fetch(&s);
    PtState pt;
    pctl_play_timer_query(&pt);
    TimeSnapshot ts;
    time_clock_snapshot(&ts);
    SysInfo si;
    sysinfo_get(&si);

    // Parental controls overall state.
    if (!s.restriction_enabled_ok) {
        pc->setDetailText(na);
        pc->setDetailTextColor(ui::color_neutral());
    } else if (s.temp_unlocked_ok && s.temp_unlocked) {
        pc->setDetailText("nx_pctl/dashboard/pc_unlocked"_i18n);
        pc->setDetailTextColor(ui::color_warn());
    } else if (s.restriction_enabled || (s.pin_length_ok && s.pin_length > 0)) {
        pc->setDetailText("nx_pctl/dashboard/pc_active"_i18n);
        pc->setDetailTextColor(ui::color_ok());
    } else {
        pc->setDetailText("nx_pctl/dashboard/pc_off"_i18n);
        pc->setDetailTextColor(ui::color_neutral());
    }

    if (!s.pin_length_ok) pin->setDetailText(na);
    else if (s.pin_length == 0) pin->setDetailText("nx_pctl/common/not_set"_i18n);
    else pin->setDetailText(brls::getStr("nx_pctl/dashboard/pin_set", (int)s.pin_length));

    level->setDetailText(s.safety_level_ok ? ui::level_name(s.safety_level) : na);

    // Today's play time.
    const int today = ui::today_weekday();
    today_limit->setText(brls::getStr("nx_pctl/dashboard/today_limit", ui::day_name(today)));
    if (!pt.fw_supported) {
        gauge->setFraction(-1);
        gauge_text->setText("nx_pctl/play_timer/fw_too_old_short"_i18n);
        today_limit->setDetailText("—");
    } else if (!pt.valid || !pt.enabled_valid) {
        gauge->setFraction(-1);
        gauge_text->setText("nx_pctl/dashboard/gauge_unknown"_i18n);
        today_limit->setDetailText(na);
    } else {
        const uint16_t limit = pt.day_min[today];
        today_limit->setDetailText(ui::fmt_minutes(limit));
        if (limit == PT_DAY_NOLIMIT || !pt.enabled) {
            gauge->setFraction(0);
            gauge_text->setText("nx_pctl/dashboard/gauge_none"_i18n);
        } else if (pt.restricted_valid && pt.restricted) {
            gauge->setFraction(1);
            gauge_text->setText("nx_pctl/dashboard/gauge_reached"_i18n);
        } else if (pt.remaining_valid && pt.remaining_ns > 0 && limit > 0) {
            uint64_t left = pt.remaining_ns / 60000000000ULL;
            uint16_t used = left >= limit ? 0 : (uint16_t)(limit - left);
            gauge->setFraction((float)used / (float)limit);
            gauge_text->setText(brls::getStr("nx_pctl/dashboard/gauge_known",
                                             ui::fmt_minutes(used), ui::fmt_minutes(limit)));
        } else {
            gauge->setFraction(-1);
            gauge_text->setText(brls::getStr("nx_pctl/dashboard/gauge_limit_only", ui::fmt_minutes(limit)));
        }
    }

    if (pt.fw_supported && pt.enabled_valid && pt.enabled && pt.remaining_valid && pt.remaining_ns > 0)
        remaining->setDetailText(ui::fmt_duration_ns(pt.remaining_ns));
    else
        remaining->setDetailText("—");

    if (pt.bedtime_valid) {
        std::string hm = fmt::format("{:02d}:{:02d}", pt.bedtime_hour, pt.bedtime_minute);
        bedtime->setDetailText(pt.bedtime_enabled ? brls::getStr("nx_pctl/play_timer/bedtime_value_on", hm)
                                                  : "nx_pctl/common/off"_i18n);
    } else {
        bedtime->setDetailText(pt.fw_supported ? na : "—");
    }

    // System.
    if (R_FAILED(ts.accuracy_rc)) {
        clock->setDetailText(na);
        clock->setDetailTextColor(ui::color_neutral());
    } else {
        clock->setDetailText(ts.accuracy ? "nx_pctl/dashboard/clock_ok"_i18n : "nx_pctl/dashboard/clock_bad"_i18n);
        clock->setDetailTextColor(ts.accuracy ? ui::color_ok() : ui::color_warn());
    }
    pairing->setDetailText(ui::bool_text(s.pairing_active_ok, s.pairing_active,
        "nx_pctl/dashboard/pairing_on"_i18n, "nx_pctl/dashboard/pairing_off"_i18n));

    fw->setDetailText(ui::fw_text(si));
    NVGcolor c = ui::color_neutral();
    compat->setDetailText(ui::compat_text(si, &c));
    compat->setDetailTextColor(c);
}

brls::View* DashboardTab::create()
{
    return new DashboardTab();
}
