// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/play_timer_tab.hpp"

#include <array>
#include <fmt/format.h>

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
const uint16_t QUICK_VALUES[] = { 30, 45, 60, 90, 120, 180, 240, 0 };

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
}   // namespace

PlayTimerTab::PlayTimerTab()
    : TabBase("xml/tab/play_timer.xml")
{
    fw_note->setSingleLine(false);
    bedtime_note->setSingleLine(false);

    quick->registerClickAction([this](brls::View*) { this->choose_uniform(); return true; });
    per_day->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new PlayTimerPerDayActivity());
        return true;
    });
    remove->registerClickAction([this](brls::View*) { this->remove_limit(); return true; });
    profile_save->registerClickAction([this](brls::View*) { this->save_profile(); return true; });
    profile_load->registerClickAction([this](brls::View*) { this->load_profile(); return true; });
    profile_delete->registerClickAction([this](brls::View*) { this->delete_profile(); return true; });

    alarm->init("nx_pctl/play_timer/alarm"_i18n, true, [this](bool on) {
        Result rc = pctl_play_timer_set_alarm_disabled(!on);
        if (R_FAILED(rc)) this->alarm->setOn(!on, false);
        ui::notify_result(rc, "nx_pctl/common/applied"_i18n, "nx_pctl/play_timer/write_err"_i18n);
    });
    pause->registerClickAction([this](brls::View*) {
        ui::confirm("nx_pctl/play_timer/pause_body"_i18n, "nx_pctl/play_timer/pause"_i18n, [this]() {
            ui::notify_result(pctl_play_timer_stop(), "nx_pctl/common/applied"_i18n, "nx_pctl/play_timer/write_err"_i18n);
            this->refresh();
        });
        return true;
    });
    resume->registerClickAction([this](brls::View*) {
        ui::confirm("nx_pctl/play_timer/resume_body"_i18n, "nx_pctl/play_timer/resume"_i18n, [this]() {
            ui::notify_result(pctl_play_timer_start(), "nx_pctl/common/applied"_i18n, "nx_pctl/play_timer/write_err"_i18n);
            this->refresh();
        });
        return true;
    });
    diag->registerClickAction([](brls::View*) {
        std::string err;
        std::string path = diagnostic::save(diagnostic::current_report(), &err);
        if (path.empty()) ui::notify("nx_pctl/toast/diag_err"_i18n + ": " + err);
        else ui::notify(brls::getStr("nx_pctl/toast/diag_saved", path));
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
    ui::set_visible(diag.getView(), fw_ok && (advanced || app::probe_build()));
    ui::set_visible(adv_header.getView(), fw_ok && (advanced || app::probe_build()));

    if (!fw_ok) return;

    // "Same limit every day": show the current value when all days agree.
    bool uniform = this->pt.valid;
    for (int i = 1; i < 7 && uniform; i++) uniform = this->pt.day_min[i] == this->pt.day_min[0];
    quick->setDetailText(uniform ? ui::fmt_minutes(this->pt.day_min[0]) : "—");

    const std::string na = "nx_pctl/common/unavailable"_i18n;
    if (this->pt.bedtime_valid) {
        std::string hm = fmt::format("{:02d}:{:02d}", this->pt.bedtime_hour, this->pt.bedtime_minute);
        bedtime->setDetailText(brls::getStr(this->pt.bedtime_enabled ? "nx_pctl/play_timer/bedtime_value_on"
                                                                     : "nx_pctl/play_timer/bedtime_value_off", hm));
    } else {
        bedtime->setDetailText(na);
    }
    bedtime_reset->setDetailText(this->pt.bedtime_reset_valid
        ? fmt::format("{:02d}:{:02d}", this->pt.bedtime_reset_hour, this->pt.bedtime_reset_minute) : na);

    if (this->pt.alarm_disabled_valid) alarm->setOn(!this->pt.alarm_disabled, false);
}

void PlayTimerTab::choose_uniform()
{
    // Pre-select the current value only when every day shares it.
    bool uniform = this->pt.valid;
    for (int i = 1; i < 7 && uniform; i++) uniform = this->pt.day_min[i] == this->pt.day_min[0];
    std::vector<std::string> labels;
    int selected = -1;
    for (size_t i = 0; i < sizeof(QUICK_VALUES) / sizeof(QUICK_VALUES[0]); i++) {
        labels.push_back(ui::fmt_minutes(QUICK_VALUES[i]));
        if (uniform && this->pt.day_min[0] == QUICK_VALUES[i]) selected = (int)i;
    }
    labels.push_back("nx_pctl/common/custom"_i18n);

    ui::pick("nx_pctl/play_timer/quick_title"_i18n, labels, selected < 0 ? 0 : selected, [this](int index) {
        const size_t count = sizeof(QUICK_VALUES) / sizeof(QUICK_VALUES[0]);
        if ((size_t)index < count) {
            this->apply_uniform(QUICK_VALUES[index]);
            return;
        }
        uint16_t seed = (this->pt.valid && this->pt.day_min[0] != PT_DAY_NOLIMIT) ? this->pt.day_min[0] : 60;
        ui::prompt_minutes("nx_pctl/play_timer/quick_title"_i18n, seed,
                           [this](uint16_t v) { this->apply_uniform(v); });
    });
}

void PlayTimerTab::apply_uniform(uint16_t minutes)
{
    std::string body = minutes == 0 ? "nx_pctl/play_timer/confirm_uniform_zero"_i18n
                                    : brls::getStr("nx_pctl/play_timer/confirm_uniform", ui::fmt_minutes(minutes));
    ui::confirm(body, "nx_pctl/play_timer/confirm_set"_i18n, [this, minutes]() {
        uint16_t days[7];
        for (auto& d : days) d = minutes;
        this->apply_days(days, brls::getStr("nx_pctl/play_timer/written_uniform", ui::fmt_minutes(minutes)));
    });
}

void PlayTimerTab::apply_days(const uint16_t days_in[7], const std::string& ok_text)
{
    std::array<uint16_t, 7> days;
    for (int i = 0; i < 7; i++) days[i] = days_in[i];
    pt_flow::ready_to_write([this, days, ok_text](bool ok, bool did_unlock) {
        if (!ok) return;
        Result rc = pctl_play_timer_set_days(days.data());
        this->refresh();
        ui::notify_result(rc, ok_text, "nx_pctl/play_timer/write_err"_i18n);
        if (R_SUCCEEDED(rc) && did_unlock) pt_flow::offer_relock([this]() { this->refresh(); });
    });
}

void PlayTimerTab::remove_limit()
{
    ui::confirm("nx_pctl/play_timer/remove_body"_i18n, "nx_pctl/play_timer/remove_confirm"_i18n, [this]() {
        pt_flow::ready_to_write([this](bool ok, bool did_unlock) {
            if (!ok) return;
            Result rc = pctl_play_timer_clear();
            this->refresh();
            ui::notify_result(rc, "nx_pctl/play_timer/removed"_i18n, "nx_pctl/play_timer/remove_err"_i18n);
            if (R_SUCCEEDED(rc) && did_unlock) pt_flow::offer_relock([this]() { this->refresh(); });
        });
    });
}

void PlayTimerTab::save_profile()
{
    if (!this->pt.valid) {
        ui::notify(ui::rc_text(NXM_RC_STATE_UNKNOWN));
        return;
    }
    ui::prompt_text("nx_pctl/play_timer/profile_name"_i18n, "", 32, [this](std::string name) {
        profiles::Profile p;
        p.name = profiles::sanitize_name(name);
        for (int i = 0; i < 7; i++) p.days[i] = this->pt.day_min[i];
        std::string err;
        if (profiles::save(p, &err)) ui::notify(brls::getStr("nx_pctl/play_timer/profile_saved", p.name));
        else ui::notify("nx_pctl/toast/diag_err"_i18n + ": " + err);
    });
}

void PlayTimerTab::load_profile()
{
    auto list = profiles::list();
    if (list.empty()) {
        ui::info("nx_pctl/play_timer/profile_none"_i18n);
        return;
    }
    std::vector<std::string> names;
    for (auto& p : list) names.push_back(p.name);
    ui::pick("nx_pctl/play_timer/profile_load"_i18n, names, 0, [this, list](int index) {
        const auto& p = list[index];
        std::string name = p.name;
        auto days = p.days;
        ui::confirm(brls::getStr("nx_pctl/play_timer/profile_apply", name, days_summary(days.data())),
                    "nx_pctl/play_timer/confirm_set"_i18n,
                    [this, days]() { this->apply_days(days.data(), "nx_pctl/play_timer/written_days"_i18n); });
    });
}

void PlayTimerTab::delete_profile()
{
    auto list = profiles::list();
    if (list.empty()) {
        ui::info("nx_pctl/play_timer/profile_none"_i18n);
        return;
    }
    std::vector<std::string> names;
    for (auto& p : list) names.push_back(p.name);
    ui::pick("nx_pctl/play_timer/profile_delete"_i18n, names, 0, [names](int index) {
        std::string name = names[index];
        ui::confirm(name, "nx_pctl/common/delete"_i18n, [name]() {
            if (profiles::remove(name)) ui::notify(brls::getStr("nx_pctl/play_timer/profile_deleted", name));
        });
    });
}

brls::View* PlayTimerTab::create()
{
    return new PlayTimerTab();
}
