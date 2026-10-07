// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/main_activity.hpp"

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

void MainActivity::onContentAvailable()
{
    if (app::read_only_build()) {
        if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView()))
            frame->setTitle("nx_pctl/title_read_only"_i18n);
    }

    // B on the sidebar quits (the tabs themselves send B back to the sidebar).
    this->getContentView()->registerAction("hints/exit"_i18n, brls::BUTTON_B, [](brls::View*) {
        brls::Application::quit();
        return true;
    });

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
            auto* dialog = new brls::Dialog(brls::getStr("nx_pctl/untested/body", fw_s, std::string(tested)));
            dialog->addButton("nx_pctl/common/quit"_i18n, []() { brls::Application::quit(); });
            dialog->addButton("nx_pctl/untested/continue"_i18n, [fw_s]() {
                config::get().untested_fw_ack = fw_s;
                config::save();
            });
            dialog->setCancelable(false);
            dialog->open();
        });
    }
}
