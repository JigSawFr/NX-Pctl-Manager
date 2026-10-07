// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/play_timer_perday_activity.hpp"

#include <array>
#include <cstring>
#include <fmt/format.h>
#include <vector>

#include "action/pt_flow.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

brls::DetailCell* PlayTimerPerDayActivity::day_cell(int d)
{
    brls::DetailCell* cells[7] = { pt_d0, pt_d1, pt_d2, pt_d3, pt_d4, pt_d5, pt_d6 };
    return cells[d];
}

void PlayTimerPerDayActivity::onContentAvailable()
{
    for (int d = 0; d < 7; d++) {
        day_cell(d)->setText(ui::day_name(d));
        day_cell(d)->registerClickAction([this, d](brls::View*) {
            this->edit_day(d);
            return true;
        });
    }
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
        if (!changes_others) return true;
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

    // B with unsaved edits asks before leaving.
    // X re-reads the system state; edits that are not saved yet are kept.
    this->getContentView()->registerAction("playguard/hints/refresh"_i18n, brls::BUTTON_X, [this](brls::View*) {
        const bool had_state = this->live.valid;
        if (had_state) pctl_play_timer_query(&this->live);
        else this->reload_from_service();
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

    this->reload_from_service();
    this->rerender();
}

void PlayTimerPerDayActivity::reload_from_service()
{
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
    state_header->show(this->live);
    for (int d = 0; d < 7; d++) {
        if (!this->live.valid) {
            day_cell(d)->setDetailText("playguard/common/unavailable"_i18n);
            continue;
        }
        std::string text = ui::fmt_minutes(this->pending[d]);
        if (this->pending[d] != this->live.day_min[d]) text += "playguard/play_timer/perday/unsaved"_i18n;
        day_cell(d)->setDetailText(text);
        day_cell(d)->setDetailTextColor(this->pending[d] != this->live.day_min[d] ? ui::color_warn()
                                                                                  : ui::color_neutral());
    }
    int unsaved = 0;
    for (int d = 0; d < 7 && this->live.valid; d++) unsaved += this->pending[d] != this->live.day_min[d];
    pt_save->setDetailText(unsaved ? brls::getStr("playguard/play_timer/perday/unsaved_count", unsaved) : "");
    pt_save->setDetailTextColor(unsaved ? ui::color_warn() : ui::color_neutral());
}

void PlayTimerPerDayActivity::pick_limit(const std::string& title, u16 current, std::function<void(u16)> on_value)
{
    // Quick values first (as in "Same limit every day"), then any value, then
    // no limit; the current value is pre-selected.
    const auto& values = pt_flow::quick_values();
    std::vector<std::string> options;
    int selected = -1;
    for (size_t i = 0; i < values.size(); i++) {
        options.push_back(ui::fmt_minutes(values[i]));
        if (values[i] == current) selected = (int)i;
    }
    const int custom_index  = (int)options.size();
    const int nolimit_index = custom_index + 1;
    options.push_back("playguard/play_timer/perday/pick_minutes"_i18n);
    options.push_back("playguard/play_timer/perday/pick_no_limit"_i18n);
    if (current == PT_DAY_NOLIMIT) selected = nolimit_index;
    else if (selected < 0) selected = custom_index;

    ui::pick(title, options, selected, [title, current, on_value, custom_index, nolimit_index](int index) {
        if (index == nolimit_index) {
            on_value(PT_DAY_NOLIMIT);
        } else if (index == custom_index) {
            u16 seed = current == PT_DAY_NOLIMIT ? 60 : current;
            ui::prompt_minutes(title, seed, [on_value](uint16_t v) { on_value(v); });
        } else {
            on_value(pt_flow::quick_values()[index]);
        }
    });
}

void PlayTimerPerDayActivity::edit_day(int d)
{
    if (!this->live.valid) {
        ui::notify("playguard/play_timer/perday/unavailable"_i18n);
        return;
    }
    this->pick_limit(brls::getStr("playguard/play_timer/perday/pick_title", ui::day_name(d)), this->pending[d],
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
    this->pick_limit(title, this->pending[list.front()], [this, list](u16 v) {
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
    if (!this->has_changes()) {
        ui::notify("playguard/play_timer/perday/no_changes"_i18n);
        return;
    }
    std::array<u16, 7> snapshot;
    std::memcpy(snapshot.data(), this->pending, sizeof(this->pending));

    // "Save" is the explicit action: no extra question unless the timer is
    // counting down (then the one dialog explains the temporary unlock).
    pt_flow::confirm_write("", "playguard/play_timer/confirm_set"_i18n, [this, snapshot](bool did_unlock) {
        Result rc = pctl_play_timer_set_days(snapshot.data());
        pt_flow::finish_write(rc, did_unlock, "playguard/play_timer/written_days"_i18n,
                              "playguard/play_timer/write_err"_i18n, [this]() {
                                  this->reload_from_service();
                                  this->rerender();
                              });
    });
}
