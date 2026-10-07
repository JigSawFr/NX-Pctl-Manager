// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/play_timer_tab.hpp"

#include <array>
#include <fmt/format.h>
#include <vector>

#include "action/pt_flow.hpp"
#include "activity/play_timer_perday_activity.hpp"
#include "activity/profiles_activity.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/diagnostics.hpp"
#include "util/profiles.hpp"

using namespace brls::literals;

namespace
{
void export_diagnostic()
{
    std::string err;
    std::string path = diagnostic::save(diagnostic::current_report(), &err);
    if (path.empty()) ui::notify("playguard/toast/diag_err"_i18n + ": " + err);
    else ui::notify(brls::getStr("playguard/toast/diag_saved", path));
}
}   // namespace

PlayTimerTab::PlayTimerTab()
    : TabBase("xml/tab/play_timer.xml")
{
    fw_note->setSingleLine(false);
    bedtime_note->setSingleLine(false);
    ui::init_unlock_banner(unlocked_banner, [this]() { this->refresh(); });
    this->enable_auto_refresh(5000);

    quick->registerClickAction([this](brls::View*) {
        pt_flow::choose_uniform_limit(this->pt, [this]() { this->refresh(); });
        return true;
    });
    extra->registerClickAction([this](brls::View*) {
        pt_flow::add_extra_time(this->pt, [this]() { this->refresh(); });
        return true;
    });
    per_day->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new PlayTimerPerDayActivity());
        return true;
    });
    remove->registerClickAction([this](brls::View*) { this->remove_limit(); return true; });
    profiles_cell->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new ProfilesActivity());
        return true;
    });

    alarm->init("playguard/play_timer/alarm"_i18n, true, [this](bool on) {
        Result rc = pctl_play_timer_set_alarm_disabled(!on);
        if (R_FAILED(rc)) this->alarm->setOn(!on, false);
        ui::notify_result(rc, "playguard/common/applied"_i18n, "playguard/play_timer/write_err"_i18n);
    });
    pause->registerClickAction([this](brls::View*) {
        ui::confirm("playguard/play_timer/pause_body"_i18n, "playguard/play_timer/pause_confirm"_i18n, [this]() {
            ui::notify_result(pctl_play_timer_stop(), "playguard/common/applied"_i18n, "playguard/play_timer/write_err"_i18n);
            this->refresh();
        });
        return true;
    });
    resume->registerClickAction([this](brls::View*) {
        ui::confirm("playguard/play_timer/resume_body"_i18n, "playguard/play_timer/resume_confirm"_i18n, [this]() {
            ui::notify_result(pctl_play_timer_start(), "playguard/common/applied"_i18n, "playguard/play_timer/write_err"_i18n);
            this->refresh();
        });
        return true;
    });
    // Diagnostic shortcut in developer mode; everyone else uses Tools.
    diag->registerClickAction([](brls::View*) {
        export_diagnostic();
        return true;
    });
}

void PlayTimerTab::refresh()
{
    pctl_play_timer_query(&this->pt);
    state_header->show(this->pt);

    const bool fw_ok    = this->pt.fw_supported;
    const bool writable = fw_ok && !app::read_only();
    const bool advanced = writable && config::get().advanced;
    const bool dev      = fw_ok && app::dev_mode();

    ui::show_unlock_banner(unlocked_banner, this->pt.temporary_unlocked_valid && this->pt.temporary_unlocked);
    ui::set_visible(fw_note.getView(), !fw_ok);
    for (brls::View* v : { (brls::View*)limit_header.getView(), (brls::View*)quick.getView(),
                           (brls::View*)per_day.getView(), (brls::View*)remove.getView(),
                           (brls::View*)profiles_cell.getView() })
        ui::set_visible(v, writable);
    ui::set_visible(extra.getView(), pt_flow::can_add_extra_time(this->pt));
    for (brls::View* v : { (brls::View*)bedtime_header.getView(), (brls::View*)bedtime.getView(),
                           (brls::View*)bedtime_reset.getView(), (brls::View*)bedtime_note.getView() })
        ui::set_visible(v, fw_ok);
    for (brls::View* v : { (brls::View*)alarm.getView(), (brls::View*)pause.getView(), (brls::View*)resume.getView() })
        ui::set_visible(v, advanced);
    ui::set_visible(diag.getView(), dev);
    ui::set_visible(adv_header.getView(), advanced || dev);

    ui::set_visible(week.getView(), fw_ok && this->pt.valid);
    if (!fw_ok) return;
    if (this->pt.valid) week->show(this->pt);

    // Profiles…: the profile applied now, else how many are saved.
    const std::string current = this->pt.valid ? profiles::match(this->pt.day_min) : "";
    const size_t saved = current.empty() ? profiles::count() : 0;
    profiles_cell->setDetailText(!current.empty() ? current
                                 : saved ? brls::getStr("playguard/play_timer/profiles_count", (int)saved) : "");

    // "Same limit every day": show the current value when all days agree.
    bool uniform = this->pt.valid;
    for (int i = 1; i < 7 && uniform; i++) uniform = this->pt.day_min[i] == this->pt.day_min[0];
    quick->setDetailText(uniform ? ui::fmt_minutes(this->pt.day_min[0]) : "—");

    const std::string na = "playguard/common/unavailable"_i18n;
    if (this->pt.bedtime_valid) {
        std::string hm = fmt::format("{:02d}:{:02d}", this->pt.bedtime_hour, this->pt.bedtime_minute);
        bedtime->setDetailText(brls::getStr(this->pt.bedtime_enabled ? "playguard/play_timer/bedtime_value_on"
                                                                     : "playguard/play_timer/bedtime_value_off", hm));
    } else {
        bedtime->setDetailText(na);
    }
    bedtime_reset->setDetailText(this->pt.bedtime_reset_valid
        ? fmt::format("{:02d}:{:02d}", this->pt.bedtime_reset_hour, this->pt.bedtime_reset_minute) : na);

    if (this->pt.alarm_disabled_valid) alarm->setOn(!this->pt.alarm_disabled, false);
}

void PlayTimerTab::remove_limit()
{
    pt_flow::confirm_write("playguard/play_timer/remove_body"_i18n, "playguard/play_timer/remove_confirm"_i18n,
                           [this](bool did_unlock) {
                               Result rc = pctl_play_timer_clear();
                               pt_flow::finish_write(rc, did_unlock, "playguard/play_timer/removed"_i18n,
                                                     "playguard/play_timer/remove_err"_i18n,
                                                     [this]() { this->refresh(); });
                           });
}

brls::View* PlayTimerTab::create()
{
    return new PlayTimerTab();
}
