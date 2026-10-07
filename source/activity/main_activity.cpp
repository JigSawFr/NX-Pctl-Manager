// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/main_activity.hpp"

#include "action/pt_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

void MainActivity::onContentAvailable()
{
    if (app::read_only_build()) {
        if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView()))
            frame->setTitle("playguard/title_read_only"_i18n);
    }

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

    // Extra time added on an earlier day: offer to put the limit back.
    brls::sync([]() { pt_flow::offer_extra_time_restore(); });

    // Firmware newer than the last checked one: warn once per firmware version.
    SysInfo si;
    sysinfo_get(&si);
    char fw[16];
    sysinfo_version_string(si.hos_version, fw, sizeof(fw));
    if (sysinfo_compat(&si) == SysCompat_UntestedNewer && config::get().untested_fw_ack != fw) {
        std::string fw_s = fw;
        brls::sync([fw_s]() {
            char tested[16];
            sysinfo_version_string(PCTL_FW_TESTED_MAX, tested, sizeof(tested));
            auto* dialog = new brls::Dialog(brls::getStr("playguard/untested/body", fw_s, std::string(tested)));
            dialog->addButton("playguard/common/quit"_i18n, []() { brls::Application::quit(); });
            dialog->addButton("playguard/untested/continue"_i18n, [fw_s]() {
                config::get().untested_fw_ack = fw_s;
                config::save();
            });
            dialog->setCancelable(false);
            dialog->open();
        });
    }
}
