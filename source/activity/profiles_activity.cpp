// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/profiles_activity.hpp"

#include <algorithm>
#include <fmt/format.h>

#include "action/pt_flow.hpp"
#include "ui/ui.hpp"

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

// One line per profile: "2 h every day", "1 h to 3 h", "No limit".
std::string short_summary(const uint16_t days[7])
{
    bool uniform = true;
    int lo = -1, hi = -1;
    bool any_nolimit = false;
    for (int d = 0; d < 7; d++) {
        uniform &= days[d] == days[0];
        if (days[d] == PT_DAY_NOLIMIT) { any_nolimit = true; continue; }
        lo = lo < 0 ? days[d] : std::min<int>(lo, days[d]);
        hi = hi < 0 ? days[d] : std::max<int>(hi, days[d]);
    }
    if (uniform) return days[0] == PT_DAY_NOLIMIT ? "playguard/common/no_limit"_i18n
                                                  : brls::getStr("playguard/play_timer/state/every_day", ui::fmt_minutes(days[0]));
    if (lo < 0) return "playguard/common/no_limit"_i18n;
    const std::string top = any_nolimit ? "playguard/common/no_limit"_i18n : ui::fmt_minutes((uint16_t)hi);
    return brls::getStr("playguard/play_timer/profile_range", ui::fmt_minutes((uint16_t)lo), top);
}
}   // namespace

void ProfilesActivity::onContentAvailable()
{
    empty->setSingleLine(false);
    hint->setSingleLine(false);
    save_cell->registerClickAction([this](brls::View*) {
        this->save_current();
        return true;
    });
    pctl_play_timer_query(&this->live);
    this->rebuild();
}

void ProfilesActivity::rebuild(const std::string& focus_name)
{
    // Move the focus to a cell that survives before deleting the old list,
    // so borealis never holds a pointer to a deleted cell.
    brls::Application::giveFocus(save_cell);
    list->clearViews();

    const auto all = profiles::list();
    const std::string current = this->live.valid ? profiles::match(this->live.day_min) : "";
    brls::View* focus = nullptr;
    for (const auto& p : all) {
        auto* cell = new brls::DetailCell();
        cell->setText(p.name);
        const std::string summary = short_summary(p.days.data());
        if (p.name == current) {
            cell->setDetailText(brls::getStr("playguard/play_timer/profile_current", summary));
            cell->setDetailTextColor(ui::color_ok());
        } else {
            cell->setDetailText(summary);
        }
        cell->registerClickAction([this, p](brls::View*) {
            this->apply(p);
            return true;
        });
        const std::string name = p.name;
        // Y, not X: X refreshes everywhere else.
        cell->registerAction("playguard/common/delete"_i18n, brls::BUTTON_Y, [this, name](brls::View*) {
            this->remove(name);
            return true;
        });
        list->addView(cell);
        if (!focus || p.name == focus_name) focus = cell;
    }
    ui::set_visible(empty.getView(), all.empty());
    ui::set_visible(hint.getView(), !all.empty());
    brls::Application::giveFocus(focus ? focus : (brls::View*)save_cell.getView());
}

void ProfilesActivity::apply(const profiles::Profile& p)
{
    const std::string name = p.name;
    auto days = p.days;
    pt_flow::confirm_write(brls::getStr("playguard/play_timer/profile_apply", name, days_summary(days.data())),
                           "playguard/play_timer/confirm_set"_i18n, [this, name, days](bool did_unlock) {
                               Result rc = pctl_play_timer_set_days(days.data());
                               pt_flow::finish_write(rc, did_unlock, "playguard/play_timer/written_days"_i18n,
                                                     "playguard/play_timer/write_err"_i18n, [this, name]() {
                                                         pctl_play_timer_query(&this->live);
                                                         this->rebuild(name);
                                                     });
                           }, days.data());
}

void ProfilesActivity::remove(const std::string& name)
{
    ui::confirm_danger(brls::getStr("playguard/play_timer/profile_delete_body", name), "playguard/common/delete"_i18n,
                       [this, name]() {
                           if (profiles::remove(name))
                               ui::notify(brls::getStr("playguard/play_timer/profile_deleted", name));
                           else
                               ui::notify(brls::getStr("playguard/play_timer/profile_delete_err", name));
                           brls::sync([this]() { this->rebuild(); });
                       });
}

void ProfilesActivity::save_current()
{
    pctl_play_timer_query(&this->live);
    if (!this->live.valid) {
        ui::notify(ui::rc_text(NXM_RC_STATE_UNKNOWN));
        return;
    }
    std::array<uint16_t, 7> days;
    for (int i = 0; i < 7; i++) days[i] = this->live.day_min[i];
    ui::prompt_text("playguard/play_timer/profile_name"_i18n, "", 32, [this, days](std::string typed) {
        profiles::Profile p;
        p.name = profiles::sanitize_name(typed);
        p.days = days;
        if (p.name.empty()) {
            ui::notify("playguard/play_timer/profile_save_err"_i18n + " — " + ui::rc_text(NXM_RC_INVALID_ARGUMENT));
            return;
        }
        auto write = [this, p]() {
            std::string err;
            if (profiles::save(p, &err)) ui::notify(brls::getStr("playguard/play_timer/profile_saved", p.name));
            else ui::notify("playguard/play_timer/profile_save_err"_i18n + ": " + err);
            const std::string name = p.name;
            brls::sync([this, name]() { this->rebuild(name); });
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
