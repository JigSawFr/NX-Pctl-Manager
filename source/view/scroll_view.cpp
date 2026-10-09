// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/scroll_view.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

ScrollView::ScrollView()
{
    // "centered" turns borealis' natural scrolling off; the scrolling itself
    // is ours (onChildFocusGained, step).
    this->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    this->registerAction("", brls::BUTTON_NAV_DOWN,
                         [this](brls::View*) { return this->step(brls::FocusDirection::DOWN); },
                         true, true, brls::SOUND_NONE);
    this->registerAction("", brls::BUTTON_NAV_UP,
                         [this](brls::View*) { return this->step(brls::FocusDirection::UP); },
                         true, true, brls::SOUND_NONE);
}

brls::View* ScrollView::create()
{
    return new ScrollView();
}

void ScrollView::willAppear(bool resetState)
{
    brls::ScrollingFrame::willAppear(resetState);
    // Not borealis' centring: our reveal, once the first layout is known.
    this->updateScrollingOnNextFrame = false;
    if (resetState) this->reveal_pending = true;
}

void ScrollView::draw(NVGcontext* vg, float x, float y, float width, float height,
                      brls::Style style, brls::FrameContext* ctx)
{
    if (this->reveal_pending) {
        this->reveal_pending = false;
        brls::View* focus = brls::Application::getCurrentFocus();
        if (this->owns(focus)) this->reveal(focus, false);
    }
    brls::ScrollingFrame::draw(vg, x, y, width, height, style, ctx);
}

brls::View* ScrollView::getDefaultFocus()
{
    if (this->parked) return this;
    return brls::ScrollingFrame::getDefaultFocus();
}

void ScrollView::onFocusLost()
{
    brls::ScrollingFrame::onFocusLost();
    this->parked = false;
}

// The first (or last) line that can take the focus and is on screen in full.
brls::View* ScrollView::visible_line(bool first)
{
    const float top = this->getContentOffsetY();
    const float bottom = top + this->getScrollingAreaHeight();
    brls::View* found = nullptr;
    std::function<bool(brls::View*)> walk = [&](brls::View* view) {
        if (view->getVisibility() != brls::Visibility::VISIBLE) return false;
        if (view->isFocusable()) {
            const Span span = this->span_of(view);
            if (span.top >= top - 1 && span.bottom <= bottom + 1) {
                found = view;
                return first;   // the first one: stop; the last one: go on
            }
            return false;
        }
        if (auto* box = dynamic_cast<brls::Box*>(view))
            for (brls::View* child : box->getChildren())
                if (walk(child)) return true;
        return false;
    };
    walk(this->contentView);
    return found;
}

void ScrollView::onChildFocusGained(brls::View* directChild, brls::View* focusedView)
{
    // Not ScrollingFrame's: it would centre the focused line.
    brls::Box::onChildFocusGained(directChild, focusedView);
    this->childFocused = true;
    if (brls::Application::getInputType() == brls::InputType::GAMEPAD) this->reveal(focusedView, true);
}

brls::View* ScrollView::getNextFocus(brls::FocusDirection direction, brls::View* currentView)
{
    // ScrollingFrame's returns the frame itself while it is not scrolled to
    // the end (natural scrolling), which would send the focus back to the top.
    return brls::Box::getNextFocus(direction, currentView);
}

bool ScrollView::owns(brls::View* view) const
{
    if (!view || !this->contentView) return false;
    for (brls::View* v = view; v; v = v->getParent())
        if (v == this->contentView) return true;
    return false;
}

ScrollView::Span ScrollView::span_of(brls::View* view) const
{
    float y = 0;
    brls::View* v = view;
    for (; v && v != this->contentView; v = v->getParent()) y += v->getLocalY();
    return { y, y + view->getHeight() };
}

// The view and the text around it: the lines that cannot take the focus just
// before and after it (a section header, its notes), up to the top or the end
// of the screen when nothing focusable comes before or after it.
ScrollView::Span ScrollView::context_of(brls::View* view) const
{
    Span span = this->span_of(view);
    bool up = true, down = true;
    brls::View* node = view;
    while (node != this->contentView && node->getParent() && (up || down)) {
        brls::Box* parent = node->getParent();
        const auto& children = parent->getChildren();
        const auto at = std::find(children.begin(), children.end(), node);
        if (at == children.end()) break;
        const int index = (int)(at - children.begin());
        for (int i = index + 1; down && i < (int)children.size(); i++) {
            brls::View* child = children[i];
            if (child->getVisibility() == brls::Visibility::GONE) continue;
            if (child->getDefaultFocus()) down = false;
            else span.bottom = std::max(span.bottom, this->span_of(child).bottom);
        }
        for (int i = index - 1; up && i >= 0; i--) {
            brls::View* child = children[i];
            if (child->getVisibility() == brls::Visibility::GONE) continue;
            if (child->getDefaultFocus()) up = false;
            else span.top = std::min(span.top, this->span_of(child).top);
        }
        node = parent;
    }
    if (node == this->contentView) {
        if (up) span.top = 0;   // with the screen's top padding
        if (down) span.bottom = this->contentView->getHeight();
    }
    return span;
}

// The line the focus would move to, if it is on this screen.
brls::View* ScrollView::neighbour(brls::FocusDirection direction) const
{
    brls::View* focus = brls::Application::getCurrentFocus();
    if (!this->owns(focus) || !focus->getParent()) return nullptr;
    brls::View* next = focus->getParent()->getNextFocus(direction, focus);
    if (next) next = next->getDefaultFocus();
    return this->owns(next) ? next : nullptr;
}

float ScrollView::bottom_limit()
{
    return std::max(0.0f, this->getContentHeight() - this->getScrollingAreaHeight());
}

void ScrollView::reveal(brls::View* focused, bool animated)
{
    const Span line = this->span_of(focused);
    const Span around = this->context_of(focused);
    const float height = this->getScrollingAreaHeight();
    float target = this->getContentOffsetY();
    if (around.height() <= height) {
        if (around.bottom > target + height) target = around.bottom - height;
        if (around.top < target) target = around.top;
    } else if (!animated) {
        target = around.top;   // a screen opens on its first words
    }
    // The focused line itself always in full (unless the screen opened on a
    // text taller than itself: D-pad down scrolls to it).
    if (animated || around.height() <= height) {
        if (line.bottom > target + height) target = line.bottom - height;
        if (line.top < target) target = line.top;
    }
    target = std::min(std::max(target, 0.0f), this->bottom_limit());
    this->startScrolling(animated, target);
}

// A D-pad press: false lets borealis move the focus (then reveal() scrolls),
// true when the press only scrolled, through a text taller than the screen.
bool ScrollView::step(brls::FocusDirection direction)
{
    if (!this->contentView) return false;
    const bool down = direction == brls::FocusDirection::DOWN;
    const float height = this->getScrollingAreaHeight();
    const float page = std::round(height / 2);
    const float offset = this->getContentOffsetY();
    const float limit = this->bottom_limit();
    brls::View* focus = brls::Application::getCurrentFocus();

    auto scroll_to = [this, offset, limit, height, focus](float target) {
        target = std::min(std::max(target, 0.0f), limit);
        if (std::fabs(target - offset) < 1) return false;
        this->startScrolling(true, target);
        // The focused line scrolled out of sight: the focus waits on the frame
        // (no highlight, borealis would draw it over the header or the footer).
        if (focus != this) {
            const Span line = this->span_of(focus);
            if (line.bottom <= target || line.top >= target + height) {
                this->parked = true;
                brls::Application::giveFocus(this);
            }
        }
        return true;
    };

    if (focus == this) {
        // After scrolling through a text (or by touch): the first line on
        // screen in the direction of the press, else more of the text.
        if (brls::View* line = this->visible_line(down)) {
            brls::Application::giveFocus(line);
            return true;
        }
        return scroll_to(offset + (down ? page : -page));
    }
    if (!this->owns(focus)) return false;

    const Span line = this->span_of(focus);
    brls::View* next = this->neighbour(direction);
    if (down) {
        // Scrolled up past the focused line (a long text above it): back to it.
        if (line.bottom > offset + height + 1) return scroll_to(std::min(offset + page, line.bottom - height));
        if (!next) return scroll_to(offset + page);   // the text after the last line, if any
        if (this->context_of(next).height() <= height) return false;
        const Span to = this->span_of(next);
        if (to.top > offset + height) return scroll_to(std::min(offset + page, to.bottom - height));
        return false;
    }
    if (line.top < offset - 1) return scroll_to(std::max(offset - page, line.top));
    if (!next) return scroll_to(offset - page);
    if (this->context_of(next).height() <= height) return false;
    const Span to = this->span_of(next);
    if (to.bottom < offset) return scroll_to(std::max(offset - page, to.top));
    return false;
}
