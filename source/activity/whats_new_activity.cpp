// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/whats_new_activity.hpp"

#include "app.hpp"
#include "view/funding.hpp"
#include "view/release_notes.hpp"

using namespace brls::literals;

void WhatsNewActivity::onContentAvailable()
{
    if (auto* applet = dynamic_cast<brls::AppletFrame*>(this->getContentView()))
        applet->setTitle("playguard/title"_i18n + " " + app::version());
    std::string date;
    const auto lines = release_notes::current(&date);
    header->setTitle(brls::getStr("playguard/about/section_changelog", app::version()));
    if (!date.empty()) header->setSubtitle(date);
    release_notes::fill(notes, lines);
    // The notes take the first focus (with no highlight), so the screen opens
    // at its top rather than scrolled to Close.
    notes->setFocusable(true);
    notes->setHideHighlight(true);
    support_note->setSingleLine(false);
    funding_box->addView(funding::cards(180, true));
    close->registerClickAction([](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
}
