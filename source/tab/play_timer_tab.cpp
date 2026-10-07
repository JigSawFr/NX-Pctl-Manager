// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/play_timer_tab.hpp"

#include <array>
#include <fmt/format.h>
#include <vector>

#include "action/pt_flow.hpp"
#include "activity/play_timer_perday_activity.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/diagnostics.hpp"
#include "util/profiles.hpp"

using namespace brls::literals;

namespace
{
std::string days_summary(const uint16_t days[7])
{
    std::string out;
    for (int i = 1; i <= 7; i++) {   // Monday first, Sunday last
        int d = i % 7;
        out += fmt::format("{}: {}", ui::day_name(d), ui::fmt_minutes(days[d]));
        if (i < 7) out += "\n";
    }
    return out;
}

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
    per_day->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new PlayTimerPerDayActivity());
        return true;
    });
    remove->registerClickAction([this](brls::View*) { this->remove_limit(); return true; });
    profile_save->registerClickAction([this](brls::View*) { this->save_profile(); return true; });
    profile_load->registerClickAction([this](brls::View*) { this->load_profile(); return true; });
    profile_delete->registerClickAction([this](brls::View*) { this->delete_profile(); return true; });

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
    // Diagnostic shortcut for PROBE builds only; everyone else uses Tools.
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
    const bool writable = fw_ok && !app::read_only_build();
    const bool advanced = writable && config::get().advanced;
    const bool probe    = fw_ok && app::probe_build();

    ui::show_unlock_banner(unlocked_banner, this->pt.temporary_unlocked_valid && this->pt.temporary_unlocked);
    ui::set_visible(fw_note.getView(), !fw_ok);
    for (brls::View* v : { (brls::View*)limit_header.getView(), (brls::View*)quick.getView(),
                           (brls::View*)per_day.getView(), (brls::View*)remove.getView(),
                           (brls::View*)profiles_header.getView(), (brls::View*)profile_save.getView(),
                           (brls::View*)profile_load.getView(), (brls::View*)profile_delete.getView() })
        ui::set_visible(v, writable);
    for (brls::View* v : { (brls::View*)bedtime_header.getView(), (brls::View*)bedtime.getView(),
                           (brls::View*)bedtime_reset.getView(), (brls::View*)bedtime_note.getView() })
        ui::set_visible(v, fw_ok);
    for (brls::View* v : { (brls::View*)alarm.getView(), (brls::View*)pause.getView(), (brls::View*)resume.getView() })
        ui::set_visible(v, advanced);
    ui::set_visible(diag.getView(), probe);
    ui::set_visible(adv_header.getView(), advanced || probe);

    if (!fw_ok) return;

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

void PlayTimerTab::apply_days(const uint16_t days_in[7], const std::string& body)
{
    std::array<uint16_t, 7> days;
    for (int i = 0; i < 7; i++) days[i] = days_in[i];
    pt_flow::confirm_write(body, "playguard/play_timer/confirm_set"_i18n, [this, days](bool did_unlock) {
        Result rc = pctl_play_timer_set_days(days.data());
        pt_flow::finish_write(rc, did_unlock, "playguard/play_timer/written_days"_i18n,
                              "playguard/play_timer/write_err"_i18n, [this]() { this->refresh(); });
    });
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

void PlayTimerTab::save_profile()
{
    if (!this->pt.valid) {
        ui::notify(ui::rc_text(NXM_RC_STATE_UNKNOWN));
        return;
    }
    std::array<uint16_t, 7> days;
    for (int i = 0; i < 7; i++) days[i] = this->pt.day_min[i];
    ui::prompt_text("playguard/play_timer/profile_name"_i18n, "", 32, [days](std::string typed) {
        profiles::Profile p;
        p.name = profiles::sanitize_name(typed);
        p.days = days;
        if (p.name.empty()) {
            ui::notify("playguard/play_timer/profile_save_err"_i18n + " — " + ui::rc_text(NXM_RC_INVALID_ARGUMENT));
            return;
        }
        auto write = [p]() {
            std::string err;
            if (profiles::save(p, &err)) ui::notify(brls::getStr("playguard/play_timer/profile_saved", p.name));
            else ui::notify("playguard/play_timer/profile_save_err"_i18n + ": " + err);
        };
        for (const auto& existing : profiles::list()) {
            if (existing.name == p.name) {
                ui::confirm(brls::getStr("playguard/play_timer/profile_replace", p.name),
                            "playguard/play_timer/profile_replace_confirm"_i18n, write);
                return;
            }
        }
        write();
    });
}

void PlayTimerTab::load_profile()
{
    auto list = profiles::list();
    if (list.empty()) {
        ui::info("playguard/play_timer/profile_none"_i18n);
        return;
    }
    std::vector<std::string> names;
    for (auto& p : list) names.push_back(p.name);
    ui::pick("playguard/play_timer/profile_load"_i18n, names, 0, [this, list](int index) {
        const auto& p = list[index];
        this->apply_days(p.days.data(),
                         brls::getStr("playguard/play_timer/profile_apply", p.name, days_summary(p.days.data())));
    });
}

void PlayTimerTab::delete_profile()
{
    auto list = profiles::list();
    if (list.empty()) {
        ui::info("playguard/play_timer/profile_none"_i18n);
        return;
    }
    std::vector<std::string> names;
    for (auto& p : list) names.push_back(p.name);
    ui::pick("playguard/play_timer/profile_delete"_i18n, names, 0, [names](int index) {
        std::string name = names[index];
        ui::confirm_danger(brls::getStr("playguard/play_timer/profile_delete_body", name),
                           "playguard/common/delete"_i18n, [name]() {
                               if (profiles::remove(name))
                                   ui::notify(brls::getStr("playguard/play_timer/profile_deleted", name));
                               else
                                   ui::notify(brls::getStr("playguard/play_timer/profile_delete_err", name));
                           });
    });
}

brls::View* PlayTimerTab::create()
{
    return new PlayTimerTab();
}
