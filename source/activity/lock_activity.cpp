// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/lock_activity.hpp"

#include "action/pin_lock.hpp"
#include "activity/main_activity.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

void LockActivity::onContentAvailable()
{
    enter->registerClickAction([this](brls::View*) {
        this->try_unlock();
        return true;
    });
    this->getContentView()->registerAction("hints/exit"_i18n, brls::BUTTON_B, [](brls::View*) {
        brls::Application::quit();
        return true;
    });
    // Straight to the PIN screen; this one stays behind it for a retry.
    brls::sync([this]() { this->try_unlock(); });
}

void LockActivity::try_unlock()
{
    if (!pin_lock::ask()) {
        ui::notify(ui::rc_text(NXM_RC_NOT_CONFIRMED));
        return;
    }
    // The main screen in place of this one (nothing to come back to).
    brls::Application::popActivity(brls::TransitionAnimation::NONE, []() {
        brls::Application::pushActivity(new MainActivity());
    });
}
