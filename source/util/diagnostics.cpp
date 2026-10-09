// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/diagnostics.hpp"

#include <ctime>
#include <fmt/format.h>
#include <vector>

#include "action/fw_gate.hpp"
#include "action/sync_flow.hpp"
#include "app.hpp"
#include "util/patches.hpp"
#include "util/paths.hpp"
#include "util/pctl_ops_c.hpp"

namespace diagnostic
{

std::string current_report()
{
    SysInfo si;
    sysinfo_get(&si);
    char fw[16];
    sysinfo_version_string(si.hos_version, fw, sizeof(fw));

    // Game patches: sys-patch state and sigpatch files (no personal data).
    const patches::Report pr = patches::detect(paths::sd_root(), fw, si.emummc);
    std::string files;
    for (const auto& f : pr.files) files += (files.empty() ? "" : ",") + f;
    static const char* const STATUS[] = { "none", "sys-patch", "sys-patch incomplete", "sigpatch files only" };
    static const char* const ISSUE[]  = { "none", "not at boot", "no log", "stale log", "skipped", "not patched" };
    const std::string patch_line = fmt::format(
        "{} (issue: {}; sys-patch installed={} boot2={} log={}{}; fs={} ldr={} es={}; files={})",
        STATUS[(int)patches::status(pr)], ISSUE[(int)patches::issue(pr)],
        pr.syspatch_installed ? 1 : 0, pr.syspatch_at_boot ? 1 : 0,
        !pr.log_present ? "none" : (pr.log_current ? "current" : "stale"),
        pr.log_present ? fmt::format(" fw={} emummc={}", pr.log_fw, pr.log_emummc ? 1 : 0) : std::string(),
        patches::module_name(pr.fs), patches::module_name(pr.ldr), patches::module_name(pr.es),
        files.empty() ? std::string("none") : files);

    // The serial number itself is never written: reports are attached to
    // public bug reports.
    const SysStorage storage = sysinfo_storage(&si);
    std::string out = fmt::format(
        "=== PlayGuard diagnostic ===\n"
        "app version : {}{}\n"
        "modes       : read_only={} dev={} firmware_choice={} platform={}\n"
        "firmware    : {}\n"
        "atmosphere  : {} ({})\n"
        "launch mode : {}\n"
        "storage     : {}\n"
        "prodinfo    : {}\n"
        "game patches: {}\n"
        "compat      : {}\n\n",
        app::version(), app::commit().empty() ? std::string() : " (commit " + app::commit() + ")",
        app::read_only() ? 1 : 0, app::dev_mode() ? 1 : 0, fw_gate::summary(),
#ifdef __SWITCH__
        "switch",
#else
        "desktop-simulation",
#endif
        fw,
        si.ams_valid ? fmt::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro) : std::string("unknown"),
        si.is_atmosphere ? "detected" : "not detected",
        si.applet_mode ? "applet (album)" : "application",
        storage == SysStorage_EmuMMC ? "emuMMC" : storage == SysStorage_SysMMC ? "sysMMC" : "unknown",
        !si.blank_valid ? "unknown" : (si.blank ? "blanked" : "not blanked"),
        patch_line,
        (int)sysinfo_compat(&si));

    std::vector<char> buf(4096);
    time_clock_dump(buf.data(), buf.size());
    out += buf.data();

    buf.assign(24576, '\0');
    pctl_dump(buf.data(), buf.size());
    out += buf.data();
    if (std::string(buf.data()).size() >= buf.size() - 1) out += "\n(report truncated)\n";
    out += sync_flow::report_section();
    return out;
}

std::string save(const std::string& report, std::string* error)
{
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
    char stamp[32] = "unknown";
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tmv);

    for (unsigned i = 0; i < 1000; ++i) {
        std::string path = i == 0 ? fmt::format("{}/{}.txt", paths::logs_dir(), stamp)
                                  : fmt::format("{}/{}_{}.txt", paths::logs_dir(), stamp, i);
        std::string existing;
        if (paths::read_file(path, existing)) continue;
        if (paths::atomic_write(path, report, error)) return path;
        return "";
    }
    if (error) *error = "No free file name";
    return "";
}

}   // namespace diagnostic
