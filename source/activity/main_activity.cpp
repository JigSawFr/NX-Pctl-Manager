// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/main_activity.hpp"

#include "action/fw_gate.hpp"
#include "action/pt_flow.hpp"
#include "app.hpp"

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
}

void MainActivity::update_title()
{
    if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView()))
        frame->setTitle(app::read_only() ? "playguard/title_read_only"_i18n : "playguard/title"_i18n);
}
