// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/main_activity.hpp"

#include "action/fw_gate.hpp"
#include "action/pt_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

void MainActivity::onContentAvailable()
{
    this->update_title();

    // B on the sidebar quits (the tabs themselves send B back to the sidebar),
    // but only when pressed twice within 2 s: one stray B never closes the app.
    this->getContentView()->registerAction("hints/exit"_i18n, brls::BUTTON_B, [](brls::View*) {
        static brls::Time last_press = 0;
        const brls::Time now = brls::getCPUTimeUsec();
        if (last_press && now - last_press < 2000000) {
            brls::Application::quit();
            return true;
        }
        last_press = now;
        brls::Application::notify("playguard/hints/exit_again"_i18n);
        return true;
    });

    // Stopped in the middle of a change last time: lock again first. Then, for
    // extra time added on an earlier day, offer to put the limit back.
    brls::sync([]() {
        pt_flow::relock_if_interrupted();
        pt_flow::offer_extra_time_restore();
    });

    // Firmware newer than the checked one: the firmware screen (or the remembered choice).
    fw_gate::on_main_screen();

    this->day = ui::today_date();
    this->day_timer.setPeriod(30000);
    this->day_timer.setCallback([this]() {
        // Only on the main screen, not behind a dialog: the date is compared
        // again at the next tick otherwise.
        auto stack = brls::Application::getActivitiesStack();
        if (!app::in_focus() || stack.empty() || stack.back() != this) return;
        const std::string today = ui::today_date();
        if (today == this->day) return;
        this->day = today;
        pt_flow::offer_extra_time_restore();
    });
    this->day_timer.start();
}

void MainActivity::update_title()
{
    // "PlayGuard", then what the user must know whatever the tab: read-only,
    // temporarily unlocked (the play timer is not counting).
    std::string title = "playguard/title"_i18n;
    if (app::read_only()) title += " · " + "playguard/title_tags/read_only"_i18n;
    if (ui::known_unlocked()) title += " · " + "playguard/title_tags/unlocked"_i18n;
    if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView())) frame->setTitle(title);
}
