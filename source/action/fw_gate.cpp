// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/fw_gate.hpp"

#include <borealis.hpp>

#include "activity/firmware_gate_activity.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/pctl_ops_c.hpp"
#include "util/update.hpp"

namespace fw_gate
{

namespace
{
const char* const NAMES[] = { "read_only", "probe", "risk" };

std::string s_choice;            // applied this session ("" before any choice)
bool        s_remembered = false;

bool from_name(const std::string& name, Choice* out)
{
    for (int i = 0; i < 3; i++)
        if (name == NAMES[i]) {
            *out = (Choice)i;
            return true;
        }
    return false;
}

void apply_session(Choice c)
{
    app::set_read_only(c != Choice::Risk);
    if (c == Choice::Probe) app::set_dev_mode(true, false);
    s_choice = NAMES[(int)c];
}
}   // namespace

bool needed()
{
    SysInfo si;
    sysinfo_get(&si);
    return sysinfo_compat(&si) == SysCompat_UntestedNewer;
}

std::string firmware()
{
    SysInfo si;
    sysinfo_get(&si);
    char fw[16];
    sysinfo_version_string(si.hos_version, fw, sizeof(fw));
    return fw;
}

std::string tested_max()
{
    char fw[16];
    sysinfo_version_string(PCTL_FW_TESTED_MAX, fw, sizeof(fw));
    return fw;
}

void prepare()
{
    if (!needed()) return;
    app::set_read_only(true);
    const auto& cfg = config::get();
    Choice c;
    if (cfg.fw_gate_fw == firmware() && cfg.fw_gate_app == app::version() && from_name(cfg.fw_gate_choice, &c)) {
        apply_session(c);
        s_remembered = true;
    }
}

void on_main_screen()
{
    if (!needed()) return;
    if (!s_remembered) {
        brls::sync([]() { brls::Application::pushActivity(new FirmwareGateActivity()); });
        return;
    }
    SysInfo si;
    sysinfo_get(&si);
    const uint32_t hos = si.hos_version;
    brls::async([hos]() {
        const update::Result r = update::check(hos);
        if (r.verdict != update::Verdict::UpdateSupports) return;
        brls::sync([r]() {
            ui::notify(brls::getStr("playguard/fw_gate/update_toast", r.latest.version, firmware()));
        });
    });
}

void apply(Choice choice, bool remember)
{
    apply_session(choice);
    s_remembered = remember;
    auto& cfg = config::get();
    if (remember) {
        cfg.fw_gate_fw     = firmware();
        cfg.fw_gate_app    = app::version();
        cfg.fw_gate_choice = NAMES[(int)choice];
        config::save();
    } else {
        forget();
    }
}

void forget()
{
    s_remembered = false;
    auto& cfg = config::get();
    if (cfg.fw_gate_fw.empty() && cfg.fw_gate_app.empty() && cfg.fw_gate_choice.empty()) return;
    cfg.fw_gate_fw.clear();
    cfg.fw_gate_app.clear();
    cfg.fw_gate_choice.clear();
    config::save();
}

std::string summary()
{
    if (s_choice.empty()) return needed() ? "pending" : "none";
    return s_remembered ? s_choice + " (remembered)" : s_choice;
}

}   // namespace fw_gate
