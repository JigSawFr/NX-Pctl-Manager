// rescue — app side (see rescue.hpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/rescue.hpp"

#include <borealis.hpp>
#include <cstdio>
#include <sys/stat.h>

#include "action/history_flow.hpp"
#include "util/pctl_ops_c.hpp"
#include "util/paths.hpp"

namespace rescue
{

namespace
{
std::string report_path()
{
    return paths::data_dir() + "/" + RESCUE_REPORT_NAME;
}
}   // namespace

bool pending()
{
    std::string ignore;
    return paths::read_file(report_path(), ignore);
}

std::optional<RescueReport> take()
{
    const std::string path = report_path();
    std::string text;
    if (!paths::read_file(path, text)) return std::nullopt;
    std::remove(path.c_str());
    std::remove((path + ".tmp").c_str());

    RescueReport r;
    if (!rescue_report_parse(text.c_str(), text.size(), &r)) {
        brls::Logger::warning("rescue report present but unreadable");
        return std::nullopt;
    }
    brls::Logger::info("rescue report: mode={} result={} rc=0x{:08X}",
                       rescue_mode_name(r.mode), rescue_result_name(r.result), (unsigned)r.rc);
    // A plain event so a parent sees recovery was used; the specifics are on
    // the recovery screen and in the log. The delete / PIN actions the parent
    // then takes record their own entries.
    history_flow::record_event("rescue");
    return r;
}

bool confirmed(const RescueReport& report)
{
    struct stat st;
    const bool installed = stat((paths::sd_root() + "/" + RESCUE_SYSMODULE_PATH).c_str(), &st) == 0;
    u32 pin_length = 0;
    bool unlocked = false;
    const Result rc = pctl_lock_state(&pin_length, &unlocked);
    const bool ok = installed && R_SUCCEEDED(rc) && rescue_report_confirmed(&report, pin_length, unlocked);
    brls::Logger::info("rescue report {} by the console (sysmodule {}, rc=0x{:08X}, PIN {}, unlocked={})",
                       ok ? "confirmed" : "not confirmed", installed ? "installed" : "missing", (unsigned)rc,
                       pin_length ? "set" : "none", unlocked);
    return ok;
}

}   // namespace rescue
