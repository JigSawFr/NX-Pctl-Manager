// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/patches.hpp"

#include <sys/stat.h>

#include "util/paths.hpp"

namespace patches
{

namespace
{
std::string trim(const std::string& s)
{
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool starts_with(const std::string& s, const char* prefix)
{
    return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

bool exists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool has_ips(const std::string& dir)
{
    return !paths::list_files(dir, ".ips").empty() || !paths::list_files(dir, ".IPS").empty();
}

std::string join(const std::string& root, const char* rel)
{
    if (root.empty() || root.back() == '/') return root + rel;
    return root + "/" + rel;
}
}   // namespace

Ini parse_ini(const std::string& text)
{
    Ini ini;
    std::string section;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            ini[section];
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        ini[section][trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return ini;
}

Module module_state(const Ini& ini, const std::string& name)
{
    auto it = ini.find(name);
    if (it == ini.end() || it->second.empty()) return Module::Missing;
    bool syspatch = false, file = false, failed = false;
    bool all_skipped = true, all_disabled = true;
    for (const auto& kv : it->second) {
        const std::string& v = kv.second;
        syspatch |= starts_with(v, "Patched (sys-patch)");
        file     |= starts_with(v, "Patched (file)");
        failed   |= starts_with(v, "Failed");
        all_skipped  &= starts_with(v, "Skipped");
        all_disabled &= starts_with(v, "Disabled");
    }
    if (syspatch) return Module::SysPatch;
    if (file) return Module::File;
    if (failed) return Module::Failed;
    if (all_skipped) return Module::Skipped;
    if (all_disabled) return Module::Disabled;
    return Module::Unpatched;
}

Report detect(const std::string& root, const std::string& fw, bool emummc)
{
    Report r;
    r.syspatch_installed = exists(join(root, "atmosphere/contents/420000000000000B/exefs.nsp"));
    r.syspatch_at_boot   = exists(join(root, "atmosphere/contents/420000000000000B/flags/boot2.flag"));

    std::string text;
    if (paths::read_file(join(root, "config/sys-patch/log.ini"), text) && !text.empty()) {
        Ini ini = parse_ini(text);
        r.log_present = true;
        auto stats = ini.find("stats");
        if (stats != ini.end()) {
            auto f = stats->second.find("fw_version");
            auto e = stats->second.find("is_emummc");
            if (f != stats->second.end()) r.log_fw = f->second;
            if (e != stats->second.end()) r.log_emummc = e->second == "1";
            r.log_current = f != stats->second.end() && e != stats->second.end() &&
                            r.log_fw == fw && r.log_emummc == emummc;
        }
        r.fs  = module_state(ini, "fs");
        r.ldr = module_state(ini, "ldr");
        r.es  = module_state(ini, "es");
    }

    if (has_ips(join(root, "atmosphere/exefs_patches/es_patches")))     r.files.push_back("es");
    if (has_ips(join(root, "atmosphere/kip_patches/fs_patches")))       r.files.push_back("fs");
    if (has_ips(join(root, "atmosphere/kip_patches/loader_patches")))   r.files.push_back("loader");
    if (has_ips(join(root, "atmosphere/exefs_patches/nfim_ctest")))     r.files.push_back("nifm");
    if (exists(join(root, "bootloader/patches.ini")))                   r.files.push_back("hekate");
    return r;
}

bool patched(Module m)
{
    return m == Module::SysPatch || m == Module::File;
}

std::vector<std::string> unpatched(const Report& r)
{
    std::vector<std::string> out;
    if (!patched(r.fs)) out.push_back("fs");
    if (!patched(r.ldr)) out.push_back("ldr");
    if (!patched(r.es)) out.push_back("es");
    return out;
}

Issue issue(const Report& r)
{
    if (!r.syspatch_installed) return Issue::None;
    if (!r.syspatch_at_boot) return Issue::NotAtBoot;
    if (!r.log_present) return Issue::NoLog;
    if (!r.log_current) return Issue::StaleLog;
    if (r.fs == Module::Skipped && r.ldr == Module::Skipped && r.es == Module::Skipped) return Issue::Skipped;
    if (!unpatched(r).empty()) return Issue::NotPatched;
    return Issue::None;
}

Status status(const Report& r)
{
    if (!r.syspatch_installed) return r.files.empty() ? Status::None : Status::FilesOnly;
    return issue(r) == Issue::None ? Status::SysPatch : Status::SysPatchIncomplete;
}

const char* module_name(Module m)
{
    switch (m) {
        case Module::Missing:   return "missing";
        case Module::SysPatch:  return "sys-patch";
        case Module::File:      return "file";
        case Module::Failed:    return "failed";
        case Module::Skipped:   return "skipped";
        case Module::Disabled:  return "disabled";
        case Module::Unpatched: return "unpatched";
    }
    return "?";
}

}   // namespace patches
