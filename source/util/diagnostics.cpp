// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/diagnostics.hpp"

#include <ctime>
#include <fmt/format.h>
#include <vector>

#include "app.hpp"
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

    std::string out = fmt::format(
        "=== PlayGuard diagnostic ===\n"
        "app version : {}\n"
        "build flags : PROBE={} READ_ONLY={} platform={}\n"
        "firmware    : {}\n"
        "atmosphere  : {} ({})\n"
        "launch mode : {}\n"
        "storage     : {}\n"
        "compat      : {}\n\n",
        app::version(), app::probe_build() ? 1 : 0, app::read_only_build() ? 1 : 0,
#ifdef __SWITCH__
        "switch",
#else
        "desktop-simulation",
#endif
        fw,
        si.ams_valid ? fmt::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro) : std::string("unknown"),
        si.is_atmosphere ? "detected" : "not detected",
        si.applet_mode ? "applet (album)" : "application",
        si.emummc ? "emuMMC" : "sysMMC or unknown",
        (int)sysinfo_compat(&si));

    std::vector<char> buf(4096);
    time_clock_dump(buf.data(), buf.size());
    out += buf.data();

    buf.assign(24576, '\0');
    pctl_dump(buf.data(), buf.size());
    out += buf.data();
    if (std::string(buf.data()).size() >= buf.size() - 1) out += "\n(report truncated)\n";
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
