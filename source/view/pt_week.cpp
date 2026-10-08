// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/pt_week.hpp"

#include <algorithm>
#include <fmt/format.h>

#include "ui/ui.hpp"

using namespace brls::literals;

namespace
{
constexpr float BAR_MAX   = 56.0f;
constexpr float BAR_MIN   = 4.0f;
constexpr float BAR_WIDTH = 30.0f;

std::string short_minutes(uint16_t m)
{
    if (m == PT_DAY_NOLIMIT) return "—";
    if (m == 0) return "0";
    return ui::fmt_minutes(m);   // "45 min", "2 h", "2 h 30"
}
}   // namespace

PtWeekView::PtWeekView()
{
    this->setAxis(brls::Axis::ROW);
    this->setJustifyContent(brls::JustifyContent::SPACE_AROUND);
    this->setAlignItems(brls::AlignItems::FLEX_END);
    this->setFocusable(false);

    for (int i = 1; i <= 7; i++) {   // Monday first, Sunday last
        const int d = i % 7;
        auto* col = new brls::Box(brls::Axis::COLUMN);
        col->setAlignItems(brls::AlignItems::CENTER);
        col->setJustifyContent(brls::JustifyContent::FLEX_END);
        col->setGrow(1.0f);
        // Room for the focus highlight around the whole column.
        col->setPadding(8, 6, 6, 6);
        col->setCornerRadius(10);
        col->setFocusable(false);
        col->registerClickAction([this, d](brls::View*) {
            if (this->on_pick) this->on_pick(d);
            return true;
        });

        Column& c = this->cols[d];
        c.box = col;
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
        c.day->setText(brls::getStr(fmt::format("playguard/days_short/{}", d)));
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

void PtWeekView::set_on_pick(std::function<void(int)> on_pick)
{
    this->on_pick = std::move(on_pick);
}

void PtWeekView::set_editable(bool editable)
{
    this->editable = editable && (bool)this->on_pick;
    for (int d = 0; d < 7; d++) this->cols[d].box->setFocusable(this->editable);
    // Today is where the eye is: the first press lands on it.
    const int today = ui::today_weekday();
    this->setDefaultFocusedIndex((today + 6) % 7);
}

void PtWeekView::show(const PtState& pt)
{
    this->render(pt.day_min, nullptr);
}

void PtWeekView::show(const uint16_t days[7], const uint16_t live[7])
{
    this->render(days, live);
}

void PtWeekView::render(const uint16_t days[7], const uint16_t* live)
{
    int top = 60;   // the tallest configured day sets the scale (at least 1 h)
    for (int d = 0; d < 7; d++)
        if (days[d] != PT_DAY_NOLIMIT) top = std::max<int>(top, days[d]);

    const int today = ui::today_weekday();
    for (int d = 0; d < 7; d++) {
        Column& c = this->cols[d];
        const uint16_t m = days[d];
        const bool is_today = d == today;
        const bool unsaved  = live && live[d] != m;
        NVGcolor colour = unsaved ? ui::color_warn() : ui::color_neutral();
        if (m == PT_DAY_NOLIMIT) {
            // An empty slot: a full bar read as "the most" rather than "none".
            c.bar->setHeight(BAR_MIN);
            colour = nvgTransRGBA(colour, 0);
        } else {
            c.bar->setHeight(BAR_MIN + (BAR_MAX - BAR_MIN) * (float)m / (float)top);
            if (!is_today && !unsaved) colour = nvgTransRGBA(colour, 120);
        }
        c.bar->setColor(colour);
        c.value->setText(short_minutes(m));
        c.value->setTextColor(unsaved ? ui::color_warn() : is_today ? ui::color_text() : ui::color_note());
        c.day->setTextColor(is_today ? ui::color_neutral() : ui::color_note());
        c.mark->setColor(is_today ? ui::color_neutral() : nvgTransRGBA(ui::color_neutral(), 0));
    }
}

brls::View* PtWeekView::create()
{
    return new PtWeekView();
}
