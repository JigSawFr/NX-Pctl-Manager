// ui — firmware, compatibility, storage, serial and game-patch lines as text.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include "util/paths.hpp"

#include <fmt/format.h>

using namespace brls::literals;

namespace ui
{

std::string fw_text(const SysInfo& info)
{
    char fw[16];
    sysinfo_version_string(info.hos_version, fw, sizeof(fw));
    std::string ams = info.ams_valid ? fmt::format("{}.{}.{}", info.ams_major, info.ams_minor, info.ams_micro)
                                     : std::string("?");
    return fmt::format("{} · Atmosphère {}", fw, ams);
}

std::string compat_text(const SysInfo& info, NVGcolor* color)
{
    char tested[16];
    sysinfo_version_string(PCTL_FW_TESTED_MAX, tested, sizeof(tested));
    switch (sysinfo_compat(&info)) {
        case SysCompat_Ok:
            if (color) *color = color_ok();
            return "playguard/compat/ok"_i18n;
        case SysCompat_UntestedNewer:
            if (color) *color = color_warn();
            return brls::getStr("playguard/compat/untested", std::string(tested));
        case SysCompat_PlayTimerUnsupported:
            if (color) *color = color_warn();
            return "playguard/compat/too_old"_i18n;
        default:
            if (color) *color = color_bad();
            return "playguard/compat/not_ams"_i18n;
    }
}

std::string storage_text(const SysInfo& info)
{
    switch (sysinfo_storage(&info)) {
        case SysStorage_EmuMMC: return "playguard/tools/storage_emummc"_i18n;
        case SysStorage_SysMMC: return "playguard/tools/storage_sysmmc"_i18n;
        default:                return "playguard/tools/storage_unknown"_i18n;
    }
}

std::string storage_short(const SysInfo& info)
{
    switch (sysinfo_storage(&info)) {
        case SysStorage_EmuMMC: return "emuMMC";
        case SysStorage_SysMMC: return "sysMMC";
        default:                return "—";
    }
}

bool serial_warning(const SysInfo& info)
{
    return sysinfo_storage(&info) == SysStorage_EmuMMC && info.blank_valid && !info.blank;
}

std::string blank_text(const SysInfo& info, NVGcolor* color)
{
    if (!info.blank_valid) {
        if (color) *color = color_neutral();
        return "playguard/common/unavailable"_i18n;
    }
    if (color) *color = info.blank ? color_ok() : (serial_warning(info) ? color_warn() : color_neutral());
    return info.blank ? "playguard/common/yes"_i18n : "playguard/common/no"_i18n;
}

std::string serial_text(const SysInfo& info, bool reveal)
{
    if (!info.serial_valid || !info.serial[0]) return "playguard/common/unavailable"_i18n;
    std::string s = info.serial;
    if (info.blank_valid && info.blank) return brls::getStr("playguard/tools/serial_blanked", s);
    if (reveal || s.size() < 8) return s;
    // Keep the prefix (model / region) and the last two digits.
    std::string masked = s.substr(0, 7);
    for (size_t i = 7; i + 2 < s.size(); i++) masked += "•";
    return masked + s.substr(s.size() - 2);
}

const patches::Report& patch_report()
{
    static bool read = false;
    static patches::Report report;
    if (!read) {
        SysInfo si;
        sysinfo_get(&si);
        char fw[16];
        sysinfo_version_string(si.hos_version, fw, sizeof(fw));
        report = patches::detect(paths::sd_root(), fw, si.emummc);
        read = true;
    }
    return report;
}

bool patches_warning(const patches::Report& report)
{
    const auto st = patches::status(report);
    return st == patches::Status::FilesOnly || st == patches::Status::SysPatchIncomplete;
}

static std::string join_list(const std::vector<std::string>& items)
{
    std::string out;
    for (const auto& i : items) out += (out.empty() ? "" : ", ") + i;
    return out;
}

std::string patches_text(const patches::Report& report, NVGcolor* color)
{
    switch (patches::status(report)) {
        case patches::Status::SysPatch:
            if (color) *color = color_ok();
            return "playguard/tools/patches_syspatch"_i18n;
        case patches::Status::SysPatchIncomplete:
            if (color) *color = color_warn();
            return "playguard/tools/patches_incomplete"_i18n;
        case patches::Status::FilesOnly:
            if (color) *color = color_warn();
            return brls::getStr("playguard/tools/patches_files", join_list(report.files));
        default:
            if (color) *color = color_neutral();
            return "playguard/tools/patches_none"_i18n;
    }
}

std::string patches_note(const patches::Report& report, const SysInfo& info, bool* warn)
{
    *warn = true;
    switch (patches::status(report)) {
        case patches::Status::FilesOnly:
            return "playguard/tools/patches_note_files"_i18n;
        case patches::Status::SysPatch:
            *warn = false;
            return report.files.empty() ? std::string()
                                        : brls::getStr("playguard/tools/patches_note_redundant", join_list(report.files));
        case patches::Status::SysPatchIncomplete:
            switch (patches::issue(report)) {
                case patches::Issue::NotAtBoot: return "playguard/tools/patches_note_not_at_boot"_i18n;
                case patches::Issue::NoLog:     return "playguard/tools/patches_note_no_log"_i18n;
                case patches::Issue::StaleLog:
                    return brls::getStr("playguard/tools/patches_note_stale",
                                        report.log_fw.empty() ? std::string("?") : report.log_fw,
                                        std::string(report.log_emummc ? "emuMMC" : "sysMMC"));
                case patches::Issue::Skipped:
                    return brls::getStr("playguard/tools/patches_note_skipped",
                                        std::string(info.emummc ? "patch_emummc" : "patch_sysmmc"));
                case patches::Issue::NotPatched:
                    return brls::getStr("playguard/tools/patches_note_unpatched", join_list(patches::unpatched(report)));
                default: break;
            }
            break;
        default: break;
    }
    *warn = false;
    return "";
}

}   // namespace ui
