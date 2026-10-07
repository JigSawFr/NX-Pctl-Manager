// PtGauge — horizontal bar showing how much of today's play time is used.
// The colour goes green → orange (≥ 75 %) → red (limit reached); the exact
// numbers are always also written in text next to it (never colour only).
// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class PtGauge : public brls::View
{
  public:
    PtGauge();

    // used: 0..1 fraction of today's limit already played. Negative == unknown
    // (an empty grey bar is drawn).
    void setFraction(float used);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    static brls::View* create();

  private:
    float used = -1.0f;
};
