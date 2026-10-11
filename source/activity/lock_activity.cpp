// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/lock_activity.hpp"

#include "action/pin_lock.hpp"
#include "activity/main_activity.hpp"
#include "activity/rescue_activity.hpp"
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
        ui::notify(pin_lock::refusal_text());
        return;
    }
    // Back to the screens it covered, or the main screen in place of this
    // one (nothing to come back to).
    if (over) brls::Application::popActivity(brls::TransitionAnimation::NONE);
    else ui::replace_screen(new MainActivity());
}

void LockActivity::lock_again()
{
    // The recovery screen is where a forgotten PIN is fixed: never covered.
    for (brls::Activity* a : brls::Application::getActivitiesStack())
        if (dynamic_cast<LockActivity*>(a) || dynamic_cast<RescueActivity*>(a)) return;
    brls::Logger::info("pin_lock: back after a while away, locked again");
    brls::Application::pushActivity(new LockActivity(true), brls::TransitionAnimation::NONE);
}
