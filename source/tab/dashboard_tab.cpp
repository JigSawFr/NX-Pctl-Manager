// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/dashboard_tab.hpp"

#include <ctime>
#include <fmt/format.h>

#include "action/pt_flow.hpp"
#include "activity/play_timer_perday_activity.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

namespace
{
void link(brls::DetailCell* cell, int tab)
{
    cell->registerClickAction([cell, tab](brls::View*) {
        ui::go_to_tab(cell, tab);
        return true;
    });
}
}   // namespace

DashboardTab::DashboardTab()
    : TabBase("xml/tab/dashboard.xml")
{
    hint->setSingleLine(false);
    ui::init_unlock_banner(unlocked_banner, [this]() { this->refresh(); });
    this->enable_auto_refresh(5000);

    today_limit->registerClickAction([this](brls::View*) {
        this->open_today_limit();
        return true;
    });
    extra->registerClickAction([this](brls::View*) {
        pt_flow::add_extra_time(this->pt, [this]() { this->refresh(); });
        return true;
    });
    link(remaining, ui::tab::play_timer);
    link(bedtime, ui::tab::play_timer);
    link(pc, ui::tab::security);
    link(pin, ui::tab::security);
    link(level, ui::tab::restrictions);
    link(clock, ui::tab::clock);
    link(pairing, ui::tab::security);
    link(fw, ui::tab::tools);
    link(compat, ui::tab::tools);
    link(serial, ui::tab::tools);
    link(game_patches, ui::tab::tools);

    SysInfo si;
    sysinfo_get(&si);
    char fw_str[16];
    sysinfo_version_string(si.hos_version, fw_str, sizeof(fw_str));
    this->patch_report = patches::detect(paths::sd_root(), fw_str, si.emummc);
}

void DashboardTab::open_today_limit()
{
    // Writable: change the limit right here (the same picker as the Play timer
    // tab when every day shares one limit, the per-day editor otherwise).
    if (app::read_only() || !this->pt.fw_supported || !this->pt.valid) {
        ui::go_to_tab(today_limit, ui::tab::play_timer);
        return;
    }
    bool uniform = true;
    for (int i = 1; i < 7 && uniform; i++) uniform = this->pt.day_min[i] == this->pt.day_min[0];
    if (uniform) pt_flow::choose_uniform_limit(this->pt, [this]() { this->refresh(); });
    else brls::Application::pushActivity(new PlayTimerPerDayActivity());
}

void DashboardTab::refresh()
{
    const std::string na = "playguard/common/unavailable"_i18n;

    PctlStatus s;
    pctl_status_fetch(&s);
    pctl_play_timer_query(&this->pt);
    const PtState& pt = this->pt;
    TimeSnapshot ts;
    time_clock_snapshot(&ts);
    SysInfo si;
    sysinfo_get(&si);

    const bool unlocked = s.temp_unlocked_ok && s.temp_unlocked;
    const bool has_pin  = s.pin_length_ok && s.pin_length > 0;

    // Today's play time.
    const int today = ui::today_weekday();
    today_limit->setText(brls::getStr("playguard/dashboard/today_limit", ui::day_name_in_text(today)));
    bool show_gauge = true;
    if (!pt.fw_supported) {
        gauge->setFraction(-1);
        gauge_text->setText("playguard/play_timer/fw_too_old_short"_i18n);
        today_limit->setDetailText("—");
    } else if (!pt.valid || !pt.enabled_valid) {
        gauge->setFraction(-1);
        gauge_text->setText("playguard/dashboard/gauge_unknown"_i18n);
        today_limit->setDetailText(na);
    } else {
        const uint16_t limit = pt.day_min[today];
        today_limit->setDetailText(ui::fmt_minutes(limit));
        if (limit != PT_DAY_NOLIMIT && unlocked) {
            show_gauge = false;   // the countdown is suspended while unlocked
            gauge_text->setText("playguard/dashboard/gauge_unlocked"_i18n);
        } else if (limit == PT_DAY_NOLIMIT || !pt.enabled) {
            show_gauge = false;   // nothing to measure against: the text says it
            gauge_text->setText("playguard/dashboard/gauge_none"_i18n);
        } else if (pt.restricted_valid && pt.restricted) {
            gauge->setFraction(1);
            gauge_text->setText("playguard/dashboard/gauge_reached"_i18n);
        } else if (pt.remaining_valid && pt.remaining_ns > 0 && limit > 0) {
            uint64_t left = pt.remaining_ns / 60000000000ULL;
            uint16_t used = left >= limit ? 0 : (uint16_t)(limit - left);
            gauge->setFraction((float)used / (float)limit);
            gauge_text->setText(brls::getStr("playguard/dashboard/gauge_known",
                                             ui::fmt_minutes(used), ui::fmt_minutes(limit)));
        } else {
            gauge->setFraction(-1);
            gauge_text->setText("playguard/dashboard/gauge_idle"_i18n);
        }
    }
    ui::set_visible(gauge.getView(), show_gauge);

    if (!pt.fw_supported || !pt.valid || !pt.enabled_valid)
        remaining->setDetailText("—");
    else if (unlocked && pt.day_min[today] != PT_DAY_NOLIMIT)
        remaining->setDetailText("playguard/dashboard/remaining_unlocked"_i18n);
    else if (!pt.enabled || pt.day_min[today] == PT_DAY_NOLIMIT)
        remaining->setDetailText("playguard/common/no_limit"_i18n);
    else if (pt.remaining_valid && pt.remaining_ns > 0)
        remaining->setDetailText(ui::fmt_duration_ns(pt.remaining_ns));
    else
        remaining->setDetailText("playguard/dashboard/remaining_idle"_i18n);

    if (pt.bedtime_valid) {
        std::string hm = fmt::format("{:02d}:{:02d}", pt.bedtime_hour, pt.bedtime_minute);
        bedtime->setDetailText(brls::getStr(pt.bedtime_enabled ? "playguard/play_timer/bedtime_value_on"
                                                               : "playguard/play_timer/bedtime_value_off", hm));
    } else {
        bedtime->setDetailText(pt.fw_supported ? na : "—");
    }

    // Parental controls overall state.
    if (!s.restriction_enabled_ok) {
        pc->setDetailText(na);
        pc->setDetailTextColor(ui::color_neutral());
    } else if (unlocked) {
        pc->setDetailText("playguard/dashboard/pc_unlocked"_i18n);
        pc->setDetailTextColor(ui::color_warn());
    } else if (s.restriction_enabled) {
        pc->setDetailText("playguard/dashboard/pc_active"_i18n);
        pc->setDetailTextColor(ui::color_ok());
    } else if (has_pin) {
        pc->setDetailText("playguard/dashboard/pc_pin_only"_i18n);
        pc->setDetailTextColor(ui::color_neutral());
    } else {
        pc->setDetailText("playguard/dashboard/pc_off"_i18n);
        pc->setDetailTextColor(ui::color_neutral());
    }

    if (!s.pin_length_ok) pin->setDetailText(na);
    else if (s.pin_length == 0) pin->setDetailText("playguard/common/not_set"_i18n);
    else pin->setDetailText(brls::getStr("playguard/dashboard/pin_set", (int)s.pin_length));

    level->setDetailText(s.safety_level_ok ? ui::level_name(s.safety_level) : na);

    // System: only what needs attention stands out.
    if (R_FAILED(ts.accuracy_rc)) {
        clock->setDetailText(na);
        clock->setDetailTextColor(ui::color_neutral());
    } else {
        clock->setDetailText(ts.accuracy ? "playguard/dashboard/clock_ok"_i18n : "playguard/dashboard/clock_bad"_i18n);
        clock->setDetailTextColor(ts.accuracy ? ui::color_ok() : ui::color_warn());
    }
    const bool paired = s.pairing_active_ok && s.pairing_active;
    pairing->setDetailText(ui::bool_text(s.pairing_active_ok, s.pairing_active,
        "playguard/dashboard/pairing_on"_i18n, "playguard/dashboard/pairing_off"_i18n));
    // A linked phone overwrites everything set here at its next sync.
    pairing->setDetailTextColor(paired ? ui::color_warn() : ui::color_neutral());

    // Firmware and compatibility appear here only when there is something to
    // check; Tools › About always shows them.
    const bool compat_issue = sysinfo_compat(&si) != SysCompat_Ok;
    fw->setDetailText(ui::fw_text(si));
    NVGcolor c = ui::color_neutral();
    compat->setDetailText(ui::compat_text(si, &c));
    compat->setDetailTextColor(c);

    // Serial number visible on emuMMC, sigpatch files only or sys-patch incomplete.
    const bool serial_issue  = ui::serial_warning(si);
    const bool patches_issue = ui::patches_warning(this->patch_report);
    serial->setDetailText("playguard/dashboard/serial_visible"_i18n);
    serial->setDetailTextColor(ui::color_warn());
    c = ui::color_neutral();
    game_patches->setDetailText(ui::patches_text(this->patch_report, &c));
    game_patches->setDetailTextColor(c);

    // When the values were last read (X refreshes now, the timer every 5 s).
    {
        std::time_t now = std::time(nullptr);
        std::tm tmv{};
        localtime_r(&now, &tmv);
        char hms[16];
        std::strftime(hms, sizeof(hms), "%H:%M:%S", &tmv);
        updated->setText(brls::getStr("playguard/dashboard/updated", std::string(hms)));
    }

    // Visibility last, so a vanished focused cell hands the focus to a neighbour.
    ui::set_visible_all({ { extra.getView(), pt_flow::can_add_extra_time(pt) },
                          { fw.getView(), compat_issue }, { compat.getView(), compat_issue },
                          { serial.getView(), serial_issue }, { game_patches.getView(), patches_issue },
                          { unlocked_banner.getView(), unlocked } });
}

brls::View* DashboardTab::create()
{
    return new DashboardTab();
}
