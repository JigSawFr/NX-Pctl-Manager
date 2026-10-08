// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/play_days.hpp"

#include <algorithm>
#include <fmt/format.h>

#include "ui/ui.hpp"

namespace
{
constexpr float BAR_MAX   = 56.0f;
constexpr float BAR_MIN   = 4.0f;
constexpr float BAR_WIDTH = 30.0f;
}   // namespace

PlayDaysView::PlayDaysView()
{
    this->setAxis(brls::Axis::ROW);
    this->setJustifyContent(brls::JustifyContent::SPACE_AROUND);
    this->setAlignItems(brls::AlignItems::FLEX_END);
    this->setFocusable(false);

    for (auto& c : this->cols) {
        auto* col = new brls::Box(brls::Axis::COLUMN);
        col->setAlignItems(brls::AlignItems::CENTER);
        col->setJustifyContent(brls::JustifyContent::FLEX_END);
        col->setGrow(1.0f);

        c.value = new brls::Label();
        c.value->setFontSize(16);
        c.value->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        c.value->setMarginBottom(4);
        c.bar = new brls::Rectangle(ui::color_neutral());
        c.bar->setWidth(BAR_WIDTH);
        c.bar->setHeight(BAR_MIN);
        c.bar->setCornerRadius(4);
        c.day = new brls::Label();
        c.day->setFontSize(16);
        c.day->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        c.day->setMarginTop(6);
        c.mark = new brls::Rectangle(ui::color_neutral());
        c.mark->setWidth(BAR_WIDTH);
        c.mark->setHeight(3);
        c.mark->setCornerRadius(1.5f);
        c.mark->setMarginTop(3);

        col->addView(c.value);
        col->addView(c.bar);
        col->addView(c.day);
        col->addView(c.mark);
        this->addView(col);
    }
}

void PlayDaysView::show(const PlayStats& s)
{
    uint32_t total[7] = {};
    for (uint32_t i = 0; i < s.count; i++)
        for (int k = 0; k < 7; k++) total[k] += s.games[i].day_s[k];
    this->show(total, s.day_wday);
}

void PlayDaysView::show(const uint32_t day_s[7], const uint8_t day_wday[7])
{
    uint64_t top = 3600;   // the busiest day sets the scale (at least 1 h)
    for (int k = 0; k < 7; k++) top = std::max<uint64_t>(top, day_s[k]);

    for (int col = 0; col < 7; col++) {
        const int k = 6 - col;   // days back
        Column& c = this->cols[col];
        const bool today = k == 0;
        c.bar->setHeight(day_s[k] ? BAR_MIN + (BAR_MAX - BAR_MIN) * (float)day_s[k] / (float)top : BAR_MIN);
        NVGcolor colour = ui::color_neutral();
        if (!day_s[k]) colour = nvgTransRGBA(colour, 40);   // nothing played: a faint stub
        else if (!today) colour = nvgTransRGBA(colour, 120);
        c.bar->setColor(colour);
        c.value->setText(day_s[k] ? ui::fmt_play_time(day_s[k]) : "—");
        c.value->setTextColor(today ? ui::color_text() : ui::color_note());
        c.day->setText(brls::getStr(fmt::format("playguard/days_short/{}", (int)day_wday[k])));
        c.day->setTextColor(today ? ui::color_neutral() : ui::color_note());
        c.mark->setColor(today ? ui::color_neutral() : nvgTransRGBA(ui::color_neutral(), 0));
    }
}

brls::View* PlayDaysView::create()
{
    return new PlayDaysView();
}
