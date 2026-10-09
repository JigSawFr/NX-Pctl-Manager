// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/made_in_france.hpp"

#include "ui/ui.hpp"

using namespace brls::literals;

FrenchFlag::FrenchFlag()
{
    this->setWidth(30);
    this->setHeight(20);
    this->setFocusable(false);
}

void FrenchFlag::draw(NVGcontext* vg, float x, float y, float width, float height,
                      brls::Style, brls::FrameContext*)
{
    // The official shades (Marianne's blue and red).
    const NVGcolor bands[] = { nvgRGB(0x00, 0x00, 0x91), nvgRGB(0xFF, 0xFF, 0xFF), nvgRGB(0xE1, 0x00, 0x0F) };
    const float w = width / 3;
    for (int i = 0; i < 3; i++) {
        nvgBeginPath(vg);
        nvgRect(vg, x + i * w, y, w, height);
        nvgFillColor(vg, bands[i]);
        nvgFill(vg);
    }
    nvgBeginPath(vg);
    nvgRect(vg, x + 0.5f, y + 0.5f, width - 1, height - 1);
    nvgStrokeColor(vg, nvgRGBA(0x80, 0x80, 0x80, 0x80));
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);
}

MadeInFrance::MadeInFrance()
    : brls::Box(brls::Axis::ROW)
{
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setFocusable(false);
    auto* label = new brls::Label();
    label->setText("playguard/about/made_in_france"_i18n);
    label->setFontSize(20);
    label->setTextColor(ui::color_note());
    label->setMarginLeft(10);
    this->addView(new FrenchFlag());
    this->addView(label);
}

brls::View* MadeInFrance::create()
{
    return new MadeInFrance();
}
