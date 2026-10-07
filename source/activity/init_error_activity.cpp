// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/init_error_activity.hpp"

#include <fmt/format.h>

#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

void InitErrorActivity::onContentAvailable()
{
    this->error_code->setText(ui::rc_text(app::pctl_init_result()));

    SysInfo si;
    sysinfo_get(&si);
    char fw[16];
    sysinfo_version_string(si.hos_version, fw, sizeof(fw));
    std::string ams = si.ams_valid ? fmt::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro)
                                   : "playguard/common/unavailable"_i18n;
    this->error_fw->setText(brls::getStr("playguard/init_error/firmware", std::string(fw), ams));

    this->getContentView()->registerAction("hints/exit"_i18n, brls::BUTTON_B, [](brls::View*) {
        brls::Application::quit();
        return true;
    });
}
