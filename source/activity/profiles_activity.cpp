// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/profiles_activity.hpp"

#include <fmt/format.h>

#include "action/pt_flow.hpp"
#include "activity/play_timer_perday_activity.hpp"
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
}   // namespace

void ProfilesActivity::onContentAvailable()
{
    empty->setSingleLine(false);
    hint->setSingleLine(false);
    save_cell->registerClickAction([this](brls::View*) {
        this->save_current();
        return true;
    });
    new_cell->registerClickAction([this](brls::View*) {
        this->create();
        return true;
    });
    pctl_play_timer_query(&this->live);
    this->rebuild();
}

void ProfilesActivity::rebuild(const std::string& focus_file)
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
        const std::string summary = ui::days_summary(p.days.data());
        if (p.name == current) {
            cell->setDetailText(brls::getStr("playguard/play_timer/profile_current", summary));
            cell->setDetailTextColor(ui::color_ok());
        } else {
            cell->setDetailText(summary);
        }
        cell->registerClickAction([this, p](brls::View*) {
            this->actions(p);
            return true;
        });
        // Y, not X: X refreshes everywhere else.
        cell->registerAction("playguard/common/delete"_i18n, brls::BUTTON_Y, [this, p](brls::View*) {
            this->remove(p);
            return true;
        });
        list->addView(cell);
        if (!focus || p.file == focus_file) focus = cell;
    }
    ui::set_visible(empty.getView(), all.empty());
    ui::set_visible(hint.getView(), !all.empty());
    brls::Application::giveFocus(focus ? focus : (brls::View*)save_cell.getView());
}

void ProfilesActivity::actions(const profiles::Profile& p)
{
    const std::vector<std::string> labels = {
        "playguard/play_timer/profile_action_apply"_i18n, "playguard/play_timer/profile_action_edit"_i18n,
        "playguard/play_timer/profile_action_rename"_i18n, "playguard/common/delete"_i18n,
    };
    ui::pick(p.name, labels, 0, [this, p](int index) {
        switch (index) {
            case 0: this->apply(p); break;
            case 1: this->edit_limits(p); break;
            case 2: this->rename(p); break;
            default: this->remove(p); break;
        }
    });
}

void ProfilesActivity::apply(const profiles::Profile& p)
{
    const std::string file = p.file;
    auto days = p.days;
    pt_flow::confirm_write(brls::getStr("playguard/play_timer/profile_apply", p.name, days_summary(days.data())),
                           "playguard/play_timer/confirm_set"_i18n, [this, file, days](bool did_unlock) {
                               Result rc = pctl_play_timer_set_days(days.data());
                               pt_flow::finish_write(rc, did_unlock, "playguard/play_timer/written_days"_i18n,
                                                     "playguard/play_timer/write_err"_i18n, [this, file]() {
                                                         pctl_play_timer_query(&this->live);
                                                         this->rebuild(file);
                                                     });
                           }, days.data());
}

bool ProfilesActivity::store(const profiles::Profile& p, const std::string& ok_text)
{
    std::string err;
    if (!profiles::save(p, &err)) {
        ui::error("playguard/play_timer/profile_save_err"_i18n + ": " + err);
        return false;
    }
    ui::notify(ok_text);
    const std::string file = profiles::file_stem(p.name);
    brls::sync([this, file]() {
        // The file kept its old case when only the case changed: find it again.
        std::string focus = file;
        for (const auto& q : profiles::list())
            if (profiles::file_stem(q.name) == file) focus = q.file;
        this->rebuild(focus);
    });
    return true;
}

void ProfilesActivity::edit_limits(const profiles::Profile& p)
{
    brls::Application::pushActivity(new PlayTimerPerDayActivity(
        brls::getStr("playguard/play_timer/profile_edit_title", p.name), p.days,
        [this, p](const PlayTimerPerDayActivity::Days& days) {
            profiles::Profile edited = p;
            edited.days = days;
            return this->store(edited, brls::getStr("playguard/play_timer/profile_saved", p.name));
        }));
}

void ProfilesActivity::rename(const profiles::Profile& p)
{
    this->ask_name(p.name, p.file, [this, p](std::string name) {
        if (name == p.name) return;
        profiles::Profile renamed = p;
        renamed.name = name;
        this->store(renamed, brls::getStr("playguard/play_timer/profile_renamed", name));
    });
}

void ProfilesActivity::remove(const profiles::Profile& p)
{
    const std::string name = p.name, file = p.file;
    ui::confirm_danger(brls::getStr("playguard/play_timer/profile_delete_body", name), "playguard/common/delete"_i18n,
                       [this, name, file]() {
                           if (profiles::remove(file))
                               ui::notify(brls::getStr("playguard/play_timer/profile_deleted", name));
                           else
                               ui::error(brls::getStr("playguard/play_timer/profile_delete_err", name));
                           brls::sync([this]() { this->rebuild(); });
                       });
}

void ProfilesActivity::ask_name(const std::string& initial, const std::string& except_file,
                                std::function<void(std::string)> done)
{
    ui::prompt_text("playguard/play_timer/profile_name"_i18n, initial, (int)profiles::MAX_NAME,
                    [except_file, done](std::string typed) {
        const std::string name = profiles::sanitize_name(typed);
        if (name.empty() || profiles::file_stem(name).empty()) {
            ui::notify("playguard/play_timer/profile_bad_name"_i18n);
            return;
        }
        profiles::Profile existing;
        if (profiles::find_same_file(name, except_file, &existing)) {
            // FAT ignores case (and the file name drops accents): this would
            // overwrite another profile.
            ui::confirm(brls::getStr("playguard/play_timer/profile_replace", existing.name),
                        "playguard/play_timer/profile_replace_confirm"_i18n, [done, name]() { done(name); });
            return;
        }
        done(name);
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
    this->ask_name("", "", [this, days](std::string name) {
        profiles::Profile p;
        p.name = name;
        p.days = days;
        this->store(p, brls::getStr("playguard/play_timer/profile_saved", name));
    });
}

void ProfilesActivity::create()
{
    // From the current limits when they can be read, else from no limit.
    pctl_play_timer_query(&this->live);
    std::array<uint16_t, 7> days;
    for (int i = 0; i < 7; i++) days[i] = this->live.valid ? this->live.day_min[i] : PT_DAY_NOLIMIT;
    this->ask_name("", "", [this, days](std::string name) {
        brls::Application::pushActivity(new PlayTimerPerDayActivity(
            brls::getStr("playguard/play_timer/profile_edit_title", name), days,
            [this, name](const PlayTimerPerDayActivity::Days& edited) {
                profiles::Profile p;
                p.name = name;
                p.days = edited;
                return this->store(p, brls::getStr("playguard/play_timer/profile_saved", name));
            }));
    });
}
