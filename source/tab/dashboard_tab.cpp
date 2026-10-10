// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/dashboard_tab.hpp"

#include <algorithm>
#include <fmt/format.h>

#include "action/clock_flow.hpp"
#include "action/outside_watch.hpp"
#include "action/play_data.hpp"
#include "action/pt_flow.hpp"
#include "action/pt_logic.hpp"
#include "action/timer_health.hpp"
#include "activity/onboarding_activity.hpp"
#include "activity/play_timer_perday_activity.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

namespace
{
// A on a line that leads to another tab: the footer says "Open", not "OK"
// (Today's limit and Extra time change the value right here instead), and
// its value ends with a chevron so the two kinds of line look different.
void link(brls::DetailCell* cell, int tab)
{
    cell->registerAction("playguard/hints/open"_i18n, brls::BUTTON_A, [cell, tab](brls::View*) {
        ui::go_to_tab(cell, tab);
        return true;
    }, false, false, brls::SOUND_CLICK);
}

void linked(brls::DetailCell* cell, const std::string& value)
{
    cell->setDetailText(value + "  ›");
}
}   // namespace

DashboardTab::DashboardTab()
    : TabBase("xml/tab/dashboard.xml")
{
    applet->setSingleLine(false);
    counted->setSingleLine(false);
    not_counting->setSingleLine(false);
    home_hint->setSingleLine(false);
    outside_text->setSingleLine(false);
    outside->setDetailTextColor(ui::color_warn());
    outside->registerClickAction([this](brls::View*) {
        outside_watch::dismiss();
        this->refresh();
        return true;
    });
    first_steps->setDetailText("playguard/onboarding/pin_missing"_i18n);
    first_steps->setDetailTextColor(ui::color_warn());
    first_steps->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new OnboardingActivity());
        return true;
    });
    ui::init_unlock_banner(unlocked_banner, [this]() { this->refresh(); });
    this->enable_auto_refresh(5000);
    this->listener = play_data::listen([this]() { this->refresh(); });

    today_limit->registerClickAction([this](brls::View*) {
        this->open_today_limit();
        return true;
    });
    extra->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        pt_flow::add_extra_time(this->pt, [this]() { this->refresh(); });
        return true;
    });
    stop->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        pt_flow::stop_today(this->pt, [this]() { this->refresh(); });
        return true;
    });
    extra_pending->registerClickAction([this](brls::View*) {
        pt_flow::offer_extra_time_restore([this]() { this->refresh(); });
        return true;
    });
    // Shown only while the alarm is off: A turns it back on (the dialog says why).
    alarm->setDetailText("playguard/dashboard/alarm_off"_i18n);
    alarm->setDetailTextColor(ui::color_warn());
    alarm->registerClickAction([this](brls::View*) {
        pt_flow::turn_alarm_on("overview", [this]() { this->refresh(); });
        return true;
    });
    link(remaining, ui::tab::play_timer);
    link(bedtime, ui::tab::play_timer);
    link(pc, ui::tab::security);
    link(pin, ui::tab::security);
    link(level, ui::tab::restrictions);
    // Inaccurate and writable: the guided measure-and-set path; else the tab.
    clock->registerAction("playguard/hints/open"_i18n, brls::BUTTON_A, [this](brls::View*) {
        if (this->clock_inaccurate && !app::read_only()) clock_flow::guided([this]() { this->refresh(); });
        else ui::go_to_tab(clock, ui::tab::clock);
        return true;
    }, false, false, brls::SOUND_CLICK);
    link(pairing, ui::tab::security);
    link(fw, ui::tab::tools);
    link(compat, ui::tab::tools);
    link(serial, ui::tab::tools);
    link(game_patches, ui::tab::tools);
}

DashboardTab::~DashboardTab()
{
    play_data::unlisten(this->listener);
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

    // Every 5 s: one pctl session with only what this screen shows, and the
    // network clock's accuracy flag alone.
    PctlStatus s;
    pctl_overview_fetch(&s, &this->pt);
    const PtState& pt = this->pt;
    bool accurate = false;
    const Result accuracy_rc = time_network_accuracy(&accurate);
    SysInfo si;
    sysinfo_get(&si);

    const bool unlocked = s.temp_unlocked_ok && s.temp_unlocked;
    const bool has_pin  = s.pin_length_ok && s.pin_length > 0;
    ui::note_unlocked(s.temp_unlocked_ok, s.temp_unlocked);

    // Today's play time from the activity log, for when the timer does not
    // say (no game counted yet today, no limit today). Read again in the
    // background when older than a few minutes.
    const auto log = play_data::latest("");
    const bool log_ok = log && log->windows_ok;
    const uint64_t log_played_s = log_ok ? play_data::today_total_s(*log) : 0;
    if (!play_data::fresh("", std::chrono::minutes(5)) && !play_data::busy("")) play_data::fetch(nullptr);
    const uint16_t log_played_min = (uint16_t)std::min<uint64_t>(1440, (log_played_s + 30) / 60);

    // Changes seen outside PlayGuard, and whether the console counts at all.
    outside_watch::observe(pt);
    const std::vector<std::string> outside_lines = outside_watch::notice_lines();
    std::string outside_joined;
    for (const auto& l : outside_lines) outside_joined += (outside_joined.empty() ? "" : "\n") + l;
    outside_text->setText(outside_joined);
    const bool stalled = timer_health::observe(pt);

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
            show_gauge = false;   // the countdown may be off while unlocked (docs: open)
            gauge_text->setText("playguard/dashboard/gauge_unlocked"_i18n);
        } else if (limit == PT_DAY_NOLIMIT || !pt.enabled) {
            show_gauge = false;   // nothing to measure against: the text says it
            gauge_text->setText(log_ok ? brls::getStr("playguard/dashboard/gauge_none_played", ui::fmt_play_time(log_played_s))
                                       : "playguard/dashboard/gauge_none"_i18n);
        } else if (pt.restricted_valid && pt.restricted) {
            gauge->setFraction(1);
            gauge_text->setText("playguard/dashboard/gauge_reached"_i18n);
        } else if (pt.remaining_valid && pt.remaining_ns > 0 && limit > 0) {
            uint64_t left = pt.remaining_ns / 60000000000ULL;
            uint16_t used = left >= limit ? 0 : (uint16_t)(limit - left);
            gauge->setFraction((float)used / (float)limit);
            gauge_text->setText(brls::getStr("playguard/dashboard/gauge_known",
                                             ui::fmt_played(used), ui::fmt_minutes(limit)));
        } else if (log_ok) {
            // No game counted yet by the timer: the log's figure, marked as an
            // estimate (it counts PlayGuard opened over a game, for instance).
            gauge->setFraction(limit ? std::min(1.0f, (float)log_played_min / (float)limit) : 1.0f);
            gauge_text->setText(brls::getStr("playguard/dashboard/gauge_log", ui::fmt_play_time(log_played_s),
                                             ui::fmt_minutes(limit)));
        } else {
            gauge->setFraction(-1);
            gauge_text->setText("playguard/dashboard/gauge_idle"_i18n);
        }
    }
    ui::set_visible(gauge.getView(), show_gauge);

    if (!pt.fw_supported || !pt.valid || !pt.enabled_valid)
        linked(remaining, "—");
    else if (unlocked && pt.day_min[today] != PT_DAY_NOLIMIT)
        linked(remaining, "playguard/dashboard/remaining_unlocked"_i18n);
    else if (!pt.enabled || pt.day_min[today] == PT_DAY_NOLIMIT)
        linked(remaining, "playguard/common/no_limit"_i18n);
    else if (pt.restricted_valid && pt.restricted)   // as the gauge says: nothing left
        linked(remaining, ui::fmt_played(0));
    else if (pt.remaining_valid && pt.remaining_ns > 0)
        linked(remaining, ui::fmt_duration_ns(pt.remaining_ns));
    else if (log_ok)
        linked(remaining, brls::getStr("playguard/dashboard/remaining_log",
                                       ui::fmt_played((uint16_t)(pt.day_min[today] > log_played_min ? pt.day_min[today] - log_played_min : 0))));
    else
        linked(remaining, "playguard/dashboard/remaining_idle"_i18n);

    if (pt.bedtime_valid) {
        std::string hm = fmt::format("{:02d}:{:02d}", pt.bedtime_hour, pt.bedtime_minute);
        linked(bedtime, brls::getStr(pt.bedtime_enabled ? "playguard/play_timer/bedtime_value_on"
                                                        : "playguard/play_timer/bedtime_value_off", hm));
    } else {
        linked(bedtime, pt.fw_supported ? na : "—");
    }

    // Parental controls overall state.
    if (!s.restriction_enabled_ok) {
        linked(pc, na);
        pc->setDetailTextColor(ui::color_neutral());
    } else if (unlocked) {
        linked(pc, "playguard/dashboard/pc_unlocked"_i18n);
        pc->setDetailTextColor(ui::color_warn());
    } else if (s.restriction_enabled) {
        linked(pc, "playguard/dashboard/pc_active"_i18n);
        pc->setDetailTextColor(ui::color_ok());
    } else if (has_pin) {
        linked(pc, "playguard/dashboard/pc_pin_only"_i18n);
        pc->setDetailTextColor(ui::color_neutral());
    } else {
        linked(pc, "playguard/dashboard/pc_off"_i18n);
        pc->setDetailTextColor(ui::color_neutral());
    }

    if (!s.pin_length_ok) linked(pin, na);
    else if (s.pin_length == 0) linked(pin, "playguard/common/not_set"_i18n);
    else linked(pin, brls::getStr("playguard/dashboard/pin_set", (int)s.pin_length));

    linked(level, s.safety_level_ok ? ui::level_name(s.safety_level) : na);

    // System: only what needs attention stands out.
    this->clock_inaccurate = R_SUCCEEDED(accuracy_rc) && !accurate;
    if (R_FAILED(accuracy_rc)) {
        linked(clock, na);
        clock->setDetailTextColor(ui::color_neutral());
    } else {
        linked(clock, accurate ? "playguard/dashboard/clock_ok"_i18n : "playguard/dashboard/clock_bad"_i18n);
        clock->setDetailTextColor(accurate ? ui::color_ok() : ui::color_warn());
    }
    const bool paired = s.pairing_active_ok && s.pairing_active;
    linked(pairing, ui::bool_text(s.pairing_active_ok, s.pairing_active,
        "playguard/dashboard/pairing_on"_i18n, "playguard/dashboard/pairing_off"_i18n));
    // A linked phone overwrites everything set here at its next sync.
    pairing->setDetailTextColor(paired ? ui::color_warn() : ui::color_neutral());

    // Firmware and compatibility appear here only when there is something to
    // check; Tools › Console always shows them.
    const bool compat_issue = sysinfo_compat(&si) != SysCompat_Ok;
    linked(fw, ui::fw_text(si));
    NVGcolor c = ui::color_neutral();
    linked(compat, ui::compat_text(si, &c));
    compat->setDetailTextColor(c);

    // Serial number visible on emuMMC, sigpatch files only or sys-patch incomplete.
    const bool serial_issue  = ui::serial_warning(si);
    const patches::Report& patch_report = ui::patch_report();
    const bool patches_issue = ui::patches_warning(patch_report);
    linked(serial, "playguard/dashboard/serial_visible"_i18n);
    serial->setDetailTextColor(ui::color_warn());
    c = ui::color_neutral();
    linked(game_patches, ui::patches_text(patch_report, &c));
    game_patches->setDetailTextColor(c);

    // When the values were last read (X refreshes now, the timer every 5 s).
    updated->setText(brls::getStr("playguard/dashboard/updated", ui::now_hms()));

    // Visibility last, so a vanished focused cell hands the focus to a neighbour.
    // No PIN yet: parental controls are not set up, say where to start.
    const bool not_set_up = s.pin_length_ok && s.pin_length == 0;
    extra->setDetailText(pt_flow::extra_today_text(pt));
    const bool pending = pt_flow::restore_pending(pt);
    if (pending) extra_pending->setText(pt_flow::restore_label());
    ui::show_writable(extra, !app::read_only());
    ui::show_writable(stop, !app::read_only());
    // As ui::show_unlock_banner: no "Lock now" in read-only mode (the banner's
    // own visibility goes in the batch below).
    unlocked_banner->detail->setVisibility(app::read_only() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    ui::set_visible_all({ { extra.getView(), pt_logic::can_add_extra_time(pt, today, false) },
                          { stop.getView(), pt_logic::can_stop_today(pt, today, false) },
                          { extra_pending.getView(), pending },
                          { alarm.getView(), pt_flow::alarm_off(pt) },
                          { first_steps.getView(), not_set_up }, { applet.getView(), si.applet_mode },
                          { counted.getView(), !si.applet_mode },
                          { fw.getView(), compat_issue }, { compat.getView(), compat_issue },
                          { serial.getView(), serial_issue }, { game_patches.getView(), patches_issue },
                          { unlocked_banner.getView(), unlocked },
                          { not_counting.getView(), stalled },
                          { outside_text.getView(), !outside_lines.empty() },
                          { outside.getView(), !outside_lines.empty() } });
}

brls::View* DashboardTab::create()
{
    return new DashboardTab();
}
