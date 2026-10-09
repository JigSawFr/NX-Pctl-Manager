// ScrollView — the scrolling frame of every PlayGuard screen (<ScrollView> in
// the XML, with brls:ScrollingFrame's attributes).
//
// borealis' default ("natural") scrolling moves a frame's worth of pixels per
// D-pad press once the next line is partly off screen, so a short press barely
// scrolls and the focus can sit on the frame itself, with no highlight; its
// "centered" mode keeps the focused line mid-screen, which hides a section's
// header and the notes under its last line. Here each press moves the focus to
// the next line and scrolls just enough to show it with the text around it
// (the header above, the notes below, the top or the end of the screen), and a
// text taller than the screen is scrolled half a screen per press before the
// focus moves on.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class ScrollView : public brls::ScrollingFrame
{
  public:
    ScrollView();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;
    void willAppear(bool resetState) override;
    void onChildFocusGained(brls::View* directChild, brls::View* focusedView) override;
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;
    brls::View* getDefaultFocus() override;
    void onFocusLost() override;

    static brls::View* create();

  private:
    struct Span
    {
        float top, bottom;
        float height() const { return bottom - top; }
    };

    bool reveal_pending = false;
    bool parked = false;   // the focus waits on the frame: see step()

    bool owns(brls::View* view) const;
    Span span_of(brls::View* view) const;
    Span context_of(brls::View* view) const;
    brls::View* neighbour(brls::FocusDirection direction) const;
    float bottom_limit();
    brls::View* visible_line(bool first);
    void reveal(brls::View* focused, bool animated);
    bool step(brls::FocusDirection direction);
};
