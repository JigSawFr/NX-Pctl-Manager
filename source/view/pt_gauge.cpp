// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "view/pt_gauge.hpp"

#include "ui/ui.hpp"

PtGauge::PtGauge()
{
    this->setHeight(14);
    this->setFocusable(false);
}

void PtGauge::setFraction(float value)
{
    if (value > 1.0f) value = 1.0f;
    this->used = value;
}

void PtGauge::draw(NVGcontext* vg, float x, float y, float width, float height,
                   brls::Style style, brls::FrameContext* ctx)
{
    (void)style;
    (void)ctx;
    const float radius = height / 2.0f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, radius);
    nvgFillColor(vg, ui::color_track());
    nvgFill(vg);

    if (this->used <= 0.0f) return;
    NVGcolor color = this->used >= 1.0f ? ui::color_bad()
                   : this->used >= 0.75f ? ui::color_warn()
                                         : ui::color_ok();
    float w = width * this->used;
    if (w < height) w = height;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, height, radius);
    nvgFillColor(vg, color);
    nvgFill(vg);
}

brls::View* PtGauge::create()
{
    return new PtGauge();
}
