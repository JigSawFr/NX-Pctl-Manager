// MadeInFrance — a small French flag and "Made in France", for the About tab
// and the first steps. The flag is drawn (blue, white, red bands, with a thin
// outline so the white band shows on the light theme): the console's shared
// font has no emoji, so "🇫🇷" would not show.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class FrenchFlag : public brls::View
{
  public:
    FrenchFlag();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;
};

class MadeInFrance : public brls::Box
{
  public:
    MadeInFrance();
    static brls::View* create();
};
