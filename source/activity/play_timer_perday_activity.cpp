// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/play_timer_perday_activity.hpp"

#include <array>
#include <cstring>
#include <fmt/format.h>
#include <vector>

#include "action/pt_flow.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

PlayTimerPerDayActivity::PlayTimerPerDayActivity(std::string title, const Days& days,
                                                 std::function<bool(const Days&)> on_save)
    : profile_mode(true), profile_title(std::move(title)), on_profile_save(std::move(on_save))
{
    // The profile stands in for the console's state: "unsaved" compares with it.
    this->live.valid = true;
    for (int i = 0; i < 7; i++) this->live.day_min[i] = this->pending[i] = days[i];
}

void PlayTimerPerDayActivity::onContentAvailable()
{
    unavailable->setSingleLine(false);
    week->set_on_pick([this](int d) { this->edit_day(d); });
    week->set_editable(true);
    pt_weekdays->registerClickAction([this](brls::View*) {
        this->fill_days({ 1, 2, 3, 4, 5 }, "playguard/play_timer/perday/weekdays"_i18n);
        return true;
    });
    pt_weekend->registerClickAction([this](brls::View*) {
        this->fill_days({ 0, 6 }, "playguard/play_timer/perday/weekend"_i18n);
        return true;
    });
    const int today = ui::today_weekday();
    pt_copy_today->setText(brls::getStr("playguard/play_timer/perday/copy_today", ui::day_name_in_text(today)));
    pt_copy_today->registerClickAction([this, today](brls::View*) {
        if (!this->live.valid) {
            ui::notify("playguard/play_timer/perday/unavailable"_i18n);
            return true;
        }
        const u16 v = this->pending[today];
        auto copy = [this, v]() {
            for (auto& p : this->pending) p = v;
            this->rerender();
        };
        bool changes_others = false;
        for (u16 p : this->pending) changes_others |= p != v;
        if (!changes_others) {
            ui::notify("playguard/play_timer/perday/already_same"_i18n);
            return true;
        }
        ui::confirm(brls::getStr("playguard/play_timer/perday/copy_today_body", ui::day_name_in_text(today), ui::fmt_minutes(v)),
                    "playguard/play_timer/perday/copy_today_confirm"_i18n, copy);
        return true;
    });
    pt_revert->registerClickAction([this](brls::View*) {
        if (!this->has_changes()) {
            ui::notify("playguard/play_timer/perday/nothing_to_discard"_i18n);
            return true;
        }
        ui::confirm("playguard/play_timer/perday/discard_body"_i18n, "playguard/play_timer/perday/discard_confirm"_i18n,
                    [this]() {
                        for (int i = 0; i < 7; i++) this->pending[i] = this->live.day_min[i];
                        this->rerender();
                    });
        return true;
    });
    pt_save->registerClickAction([this](brls::View*) {
        this->save();
        return true;
    });
    // + saves from anywhere on the screen (the cell stays for the eye). Not Y:
    // Y deleted a profile elsewhere, and a button must not mean both.
    this->getContentView()->registerAction("hints/save"_i18n, brls::BUTTON_START, [this](brls::View*) {
        this->save();
        return true;
    });

    // B with unsaved edits asks before leaving.
    // X re-reads the system state; edits that are not saved yet are kept.
    if (!this->profile_mode) this->getContentView()->registerAction("playguard/hints/refresh"_i18n, brls::BUTTON_X, [this](brls::View*) {
        if (this->live.valid) {
            // A read that fails keeps the last one: the edits stay comparable
            // with it (B still asks before dropping them).
            PtState now;
            pctl_play_timer_query(&now);
            if (now.valid) this->live = now;
            else ui::notify("playguard/play_timer/perday/unavailable"_i18n);
        } else {
            this->reload_from_service();
        }
        this->rerender();
        return true;
    });

    this->getContentView()->registerAction("hints/back"_i18n, brls::BUTTON_B, [this](brls::View*) {
        if (!this->has_changes()) {
            brls::Application::popActivity();
            return true;
        }
        ui::confirm("playguard/play_timer/perday/discard_body"_i18n, "playguard/play_timer/perday/discard_confirm"_i18n,
                    []() { brls::sync([]() { brls::Application::popActivity(); }); });
        return true;
    });

    if (this->profile_mode) {
        // The console's state is not what this screen edits: no header.
        ui::set_visible(state_header.getView(), false);
        if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView())) frame->setTitle(this->profile_title);
    }
    this->reload_from_service();
    this->rerender();
}

void PlayTimerPerDayActivity::reload_from_service()
{
    if (this->profile_mode) return;   // the profile's days were set at construction
    pctl_play_timer_query(&this->live);
    if (this->live.valid)
        for (int i = 0; i < 7; i++) this->pending[i] = this->live.day_min[i];
}

bool PlayTimerPerDayActivity::has_changes() const
{
    if (!this->live.valid) return false;
    for (int i = 0; i < 7; i++)
        if (this->pending[i] != this->live.day_min[i]) return true;
    return false;
}

void PlayTimerPerDayActivity::rerender()
{
    if (!this->profile_mode) state_header->show(this->live);
    ui::set_visible(unavailable.getView(), !this->live.valid);
    ui::set_visible(week.getView(), this->live.valid);
    if (this->live.valid) week->show(this->pending, this->live.day_min);
    int unsaved = 0;
    for (int d = 0; d < 7 && this->live.valid; d++) unsaved += this->pending[d] != this->live.day_min[d];
    pt_save->setDetailText(unsaved ? brls::getStr("playguard/play_timer/perday/unsaved_count", unsaved) : "");
    pt_save->setDetailTextColor(unsaved ? ui::color_warn() : ui::color_neutral());
    // The footer's + hint carries the count too: it is visible from any row.
    this->getContentView()->updateActionHint(brls::BUTTON_START, unsaved ? brls::getStr("playguard/play_timer/perday/save_hint", unsaved)
                                                                     : "hints/save"_i18n);
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void PlayTimerPerDayActivity::edit_day(int d)
{
    if (!this->live.valid) {
        ui::notify("playguard/play_timer/perday/unavailable"_i18n);
        return;
    }
    pt_flow::pick_limit(brls::getStr("playguard/play_timer/perday/pick_title", ui::day_name(d)), this->pending[d],
                        [this, d](u16 v) {
                            this->pending[d] = v;
                            this->rerender();
                        });
}

void PlayTimerPerDayActivity::fill_days(std::initializer_list<int> days, const std::string& title)
{
    if (!this->live.valid) {
        ui::notify("playguard/play_timer/perday/unavailable"_i18n);
        return;
    }
    std::vector<int> list(days);
    pt_flow::pick_limit(title, this->pending[list.front()], [this, list](u16 v) {
        for (int d : list) this->pending[d] = v;
        this->rerender();
    });
}

void PlayTimerPerDayActivity::save()
{
    if (!this->live.valid) {
        ui::notify("playguard/play_timer/perday/unavailable"_i18n);
        return;
    }
    std::array<u16, 7> snapshot;
    std::memcpy(snapshot.data(), this->pending, sizeof(this->pending));
    if (this->profile_mode) {
        // Saved as it is, changed or not (a new profile starts unchanged).
        if (this->on_profile_save && this->on_profile_save(snapshot)) {
            for (int i = 0; i < 7; i++) this->live.day_min[i] = snapshot[i];
            brls::sync([]() { brls::Application::popActivity(); });
        }
        return;
    }
    if (!this->has_changes()) {
        ui::notify("playguard/play_timer/perday/no_changes"_i18n);
        return;
    }

    // "Save" is the explicit action: no extra question unless the timer is
    // counting down (then the one dialog explains the temporary unlock).
    pt_flow::confirm_write("", "playguard/play_timer/confirm_set"_i18n, [this, snapshot](bool did_unlock) {
        Result rc = pt_flow::write_days(snapshot.data(), "per_day");
        pt_flow::finish_write(rc, did_unlock, "playguard/play_timer/written_days"_i18n,
                              "playguard/play_timer/write_err"_i18n, [this]() {
                                  this->reload_from_service();
                                  this->rerender();
                              });
    }, snapshot.data());
}
