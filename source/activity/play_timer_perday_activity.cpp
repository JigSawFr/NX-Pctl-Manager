// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/play_timer_perday_activity.hpp"

#include <array>
#include <cstring>
#include <fmt/format.h>

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
    pt_copy_today->setText(brls::getStr("playguard/play_timer/perday/copy_today", ui::day_name(today)));
    pt_copy_today->registerClickAction([this, today](brls::View*) {
        u16 v = this->pending[today];
        for (auto& p : this->pending) p = v;
        this->rerender();
        return true;
    });
    pt_revert->registerClickAction([this](brls::View*) {
        if (!this->has_changes()) return true;
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
    pt_save->setDetailText(this->has_changes() ? "*" : "");
}

void PlayTimerPerDayActivity::edit_day(int d)
{
    if (!this->live.valid) {
        ui::notify("playguard/play_timer/perday/unavailable"_i18n);
        return;
    }
    std::vector<std::string> options = { "playguard/play_timer/perday/pick_minutes"_i18n,
                                         "playguard/play_timer/perday/pick_no_limit"_i18n };
    ui::pick(brls::getStr("playguard/play_timer/perday/pick_title", ui::day_name(d)), options,
             this->pending[d] == PT_DAY_NOLIMIT ? 1 : 0, [this, d](int index) {
                 if (index == 1) {
                     this->pending[d] = PT_DAY_NOLIMIT;
                     this->rerender();
                     return;
                 }
                 u16 seed = this->pending[d] == PT_DAY_NOLIMIT ? 60 : this->pending[d];
                 ui::prompt_minutes(brls::getStr("playguard/play_timer/perday/pick_title", ui::day_name(d)), seed,
                                    [this, d](uint16_t v) {
                                        this->pending[d] = v;
                                        this->rerender();
                                    });
             });
}

void PlayTimerPerDayActivity::fill_days(std::initializer_list<int> days, const std::string& title)
{
    if (!this->live.valid) {
        ui::notify("playguard/play_timer/perday/unavailable"_i18n);
        return;
    }
    std::vector<int> list(days);
    std::vector<std::string> options = { "playguard/play_timer/perday/pick_minutes"_i18n,
                                         "playguard/play_timer/perday/pick_no_limit"_i18n };
    ui::pick(title, options, 0, [this, list, title](int index) {
        if (index == 1) {
            for (int d : list) this->pending[d] = PT_DAY_NOLIMIT;
            this->rerender();
            return;
        }
        u16 seed = this->pending[list.front()] == PT_DAY_NOLIMIT ? 60 : this->pending[list.front()];
        ui::prompt_minutes(title, seed, [this, list](uint16_t v) {
            for (int d : list) this->pending[d] = v;
            this->rerender();
        });
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

    pt_flow::ready_to_write([this, snapshot](bool ok, bool did_unlock) {
        if (!ok) {
            pctl_play_timer_query(&this->live);
            this->rerender();
            return;
        }
        Result rc = pctl_play_timer_set_days(snapshot.data());
        this->reload_from_service();
        this->rerender();
        ui::notify_result(rc, "playguard/play_timer/written_days"_i18n, "playguard/play_timer/write_err"_i18n);
        if (R_SUCCEEDED(rc) && did_unlock) pt_flow::offer_relock([this]() {
            pctl_play_timer_query(&this->live);
            this->rerender();
        });
    });
}
