// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/update_flow.hpp"

#include <borealis.hpp>

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/launcher.hpp"

using namespace brls::literals;

namespace update_flow
{

namespace
{
std::string store_name(launcher::Store store)
{
    switch (store) {
        case launcher::Store::Sphaira:  return "sphaira";
        case launcher::Store::AppStore: return "playguard/update/appstore"_i18n;
        default:                        return "";
    }
}

std::string firmware_string()
{
    SysInfo si;
    sysinfo_get(&si);
    char fw[16];
    sysinfo_version_string(si.hos_version, fw, sizeof(fw));
    return fw;
}

void show_result(const update::Result& r)
{
    switch (r.verdict) {
        case update::Verdict::UpdateSupports:
        case update::Verdict::UpdateNoSupport: {
            std::string body = brls::getStr("playguard/update/available", r.latest.version, app::version());
            if (r.verdict == update::Verdict::UpdateNoSupport)
                body += "\n\n" + brls::getStr("playguard/update/available_no_fw", firmware_string());
            ui::confirm(body, update_label(), []() { open_store(); });
            break;
        }
        case update::Verdict::UpToDate:
            ui::info(brls::getStr("playguard/update/up_to_date", app::version()));
            break;
        default:
            ui::info(brls::getStr("playguard/update/failed", r.error));
            break;
    }
}
}   // namespace

void check_now()
{
    ui::notify("playguard/update/checking"_i18n);
    SysInfo si;
    sysinfo_get(&si);
    const uint32_t fw = si.hos_version;
    brls::async([fw]() {
        const update::Result r = update::check(fw);
        brls::sync([r]() { show_result(r); });
    });
}

void open_store()
{
    const launcher::Target target = launcher::find(config::get().update_via);
    const std::string manual = brls::getStr("playguard/update/manual_body", std::string(app::repo_url()) + "/releases");
    if (target.store == launcher::Store::None || !launcher::can_launch()) {
        ui::info(manual);
        return;
    }
    const std::string name = store_name(target.store);
    ui::confirm(brls::getStr("playguard/update/open_body", name), brls::getStr("playguard/update/open_confirm", name),
                [target, manual]() {
                    if (launcher::launch(target)) brls::Application::quit();
                    else ui::info(manual);
                });
}

std::string status_text(const update::Result& r, const std::string& firmware)
{
    switch (r.verdict) {
        case update::Verdict::UpdateSupports:
            return brls::getStr("playguard/fw_gate/status_supports", r.latest.version, firmware);
        case update::Verdict::UpdateNoSupport:
            return brls::getStr("playguard/fw_gate/status_no_support", r.latest.version, firmware);
        case update::Verdict::UpToDate:
            return brls::getStr("playguard/fw_gate/status_latest", app::version());
        default:
            return brls::getStr("playguard/fw_gate/status_unknown", r.error);
    }
}

std::string update_label()
{
    const launcher::Target target = launcher::find(config::get().update_via);
    if (target.store == launcher::Store::None || !launcher::can_launch()) return "playguard/update/update_manual"_i18n;
    return brls::getStr("playguard/update/update_with", store_name(target.store));
}

}   // namespace update_flow
