// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/play_days.hpp"

#include <algorithm>
#include <fmt/format.h>

#include "ui/ui.hpp"

using namespace brls::literals;

namespace
{
constexpr float BAR_MAX   = 56.0f;
constexpr float BAR_MIN   = 4.0f;
constexpr float BAR_WIDTH = 30.0f;
constexpr float LANE_WIDTH = BAR_WIDTH + 16.0f;   // the limit line overhangs the bar
constexpr float LABEL_FONT = 20.0f;   // as big as the text around it
constexpr float LEGEND_FONT = 18.0f;

float bar_height(uint64_t seconds, uint64_t top)
{
    return seconds ? BAR_MIN + (BAR_MAX - BAR_MIN) * (float)seconds / (float)top : BAR_MIN;
}
}   // namespace

PlayDaysView::PlayDaysView()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setAlignItems(brls::AlignItems::STRETCH);
    this->setFocusable(false);

    auto* row = new brls::Box(brls::Axis::ROW);
    row->setJustifyContent(brls::JustifyContent::SPACE_AROUND);
    row->setAlignItems(brls::AlignItems::FLEX_END);
    this->addView(row);

    for (auto& c : this->cols) {
        auto* col = new brls::Box(brls::Axis::COLUMN);
        col->setAlignItems(brls::AlignItems::CENTER);
        col->setJustifyContent(brls::JustifyContent::FLEX_END);
        col->setGrow(1.0f);

        c.value = new brls::Label();
        c.value->setFontSize(LABEL_FONT);
        c.value->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        c.value->setMarginBottom(8);   // clear of a limit line at the top of the lane
        // A fixed lane: the bar grows from its bottom, the limit line is
        // placed in it by height, whatever the bar.
        auto* lane = new brls::Box(brls::Axis::COLUMN);
        lane->setWidth(LANE_WIDTH);
        lane->setHeight(BAR_MAX);
        lane->setAlignItems(brls::AlignItems::CENTER);
        lane->setJustifyContent(brls::JustifyContent::FLEX_END);
        c.bar = new brls::Rectangle(ui::color_neutral());
        c.bar->setWidth(BAR_WIDTH);
        c.bar->setHeight(BAR_MIN);
        c.bar->setCornerRadius(4);
        c.limit = new brls::Rectangle(ui::color_text());
        c.limit->setWidth(LANE_WIDTH);
        c.limit->setHeight(2);
        c.limit->setPositionType(brls::PositionType::ABSOLUTE);
        c.limit->setPositionLeft(0);
        c.limit->setPositionBottom(0);
        c.limit->setVisibility(brls::Visibility::GONE);
        lane->addView(c.bar);
        lane->addView(c.limit);
        c.day = new brls::Label();
        c.day->setFontSize(LABEL_FONT);
        c.day->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        c.day->setMarginTop(6);
        c.mark = new brls::Rectangle(ui::color_neutral());
        c.mark->setWidth(BAR_WIDTH);
        c.mark->setHeight(3);
        c.mark->setCornerRadius(1.5f);
        c.mark->setMarginTop(3);

        col->addView(c.value);
        col->addView(lane);
        col->addView(c.day);
        col->addView(c.mark);
        row->addView(col);
    }

    this->legend = new brls::Label();
    this->legend->setFontSize(LEGEND_FONT);
    this->legend->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    this->legend->setTextColor(ui::color_note());
    this->legend->setMarginTop(10);
    this->addView(this->legend);
}

void PlayDaysView::show(const PlayStats& s, const uint16_t limits[7])
{
    uint32_t total[7] = {};
    for (uint32_t i = 0; i < s.count; i++)
        for (int k = 0; k < 7; k++) total[k] += s.games[i].day_s[k];
    this->show(total, s.day_wday, limits);
}

void PlayDaysView::show(const uint32_t day_s[7], const uint8_t day_wday[7], const uint16_t limits[7])
{
    uint64_t top = 3600;   // the busiest day (or the highest limit) sets the scale, at least 1 h
    for (int k = 0; k < 7; k++) {
        top = std::max<uint64_t>(top, day_s[k]);
        if (limits && limits[k] != PT_DAY_NOLIMIT) top = std::max<uint64_t>(top, (uint64_t)limits[k] * 60);
    }

    bool any_over = false;
    for (int col = 0; col < 7; col++) {
        const int k = 6 - col;   // days back
        Column& c = this->cols[col];
        const bool today = k == 0;
        const bool has_limit = limits && limits[k] != PT_DAY_NOLIMIT;
        const bool over = has_limit && day_s[k] > (uint64_t)limits[k] * 60;
        c.bar->setHeight(bar_height(day_s[k], top));
        NVGcolor colour = over ? ui::color_warn() : ui::color_neutral();
        if (!day_s[k]) colour = nvgTransRGBA(colour, 40);   // nothing played: a faint stub
        else if (!today && !over) colour = nvgTransRGBA(colour, 120);
        c.bar->setColor(colour);
        c.limit->setVisibility(has_limit ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        if (has_limit) {
            // 0 min (no play) sits on the floor of the lane.
            const float h = limits[k] ? bar_height((uint64_t)limits[k] * 60, top) : 0.0f;
            c.limit->setPositionBottom(std::max(0.0f, h - 1.0f));
            c.limit->setColor(over ? ui::color_warn() : nvgTransRGBA(ui::color_text(), 150));
        }
        any_over = any_over || over;
        // Over the limit is marked by "!", not by the colour alone.
        const std::string played = day_s[k] ? ui::fmt_play_time(day_s[k]) : "—";
        c.value->setText(over ? "! " + played : played);
        c.value->setTextColor(over ? ui::color_warn() : today ? ui::color_text() : ui::color_note());
        c.day->setText(brls::getStr(fmt::format("playguard/days_short/{}", (int)day_wday[k])));
        c.day->setTextColor(today ? ui::color_neutral() : ui::color_note());
        c.mark->setColor(today ? ui::color_neutral() : nvgTransRGBA(ui::color_neutral(), 0));
    }

    // The order differs from the play timer's week (Monday first): say so,
    // and what the line and the amber mean when they are shown.
    std::string text = "playguard/chart/last_days"_i18n;
    if (limits) text += "\n" + (any_over ? "playguard/chart/limit_over"_i18n : "playguard/chart/limit_line"_i18n);
    this->legend->setText(text);
}

brls::View* PlayDaysView::create()
{
    return new PlayDaysView();
}
