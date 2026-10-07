// patches — how this console patches signature checks for games: the
// sys-patch system module (patches fs / ldr / es in memory at boot, then
// exits) or sigpatch files loaded by Atmosphère / Hekate.
// Plain C++ (no borealis), so the host tests can run it on a fake SD card.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <map>
#include <string>
#include <vector>

namespace patches
{

// Best result sys-patch logged for one module (all its patterns).
enum class Module
{
    Missing,     // no section in the log
    SysPatch,    // "Patched (sys-patch)"
    File,        // "Patched (file)": already patched by sigpatch files
    Failed,      // "Failed (...)"
    Skipped,     // "Skipped": patching turned off for this storage (patch_sysmmc / patch_emummc)
    Disabled,    // "Disabled": every pattern turned off in config.ini
    Unpatched,   // "Unpatched": no pattern matched this firmware
};

struct Report
{
    bool syspatch_installed = false;   // atmosphere/contents/420000000000000B/exefs.nsp
    bool syspatch_at_boot   = false;   // .../flags/boot2.flag
    bool log_present        = false;   // config/sys-patch/log.ini
    bool log_current        = false;   // written for this firmware and storage
    std::string log_fw;                // [stats] fw_version
    bool log_emummc         = false;   // [stats] is_emummc
    Module fs  = Module::Missing;
    Module ldr = Module::Missing;
    Module es  = Module::Missing;
    std::vector<std::string> files;    // sigpatch sets found: "es", "fs", "loader", "nifm", "hekate"
};

enum class Status
{
    None,                // nothing detected
    SysPatch,            // sys-patch patched fs, ldr and es at this boot
    SysPatchIncomplete,  // sys-patch installed, but see issue()
    FilesOnly,           // sigpatch files only: sys-patch is the recommended way now
};

enum class Issue
{
    None,
    NotAtBoot,      // installed, no boot2.flag
    NoLog,          // no log.ini (logging turned off, or never ran)
    StaleLog,       // log from another firmware or storage: did not run at this boot
    Skipped,        // patching turned off for this storage
    NotPatched,     // fs, ldr or es left unpatched (see unpatched())
};

using Ini = std::map<std::string, std::map<std::string, std::string>>;

Ini    parse_ini(const std::string& text);
Module module_state(const Ini& ini, const std::string& section);

// `root` is the SD card root ("/" on the console). `fw` is "23.0.1".
Report detect(const std::string& root, const std::string& fw, bool emummc);

Status status(const Report& r);
Issue  issue(const Report& r);
std::vector<std::string> unpatched(const Report& r);   // of "fs", "ldr", "es"
bool   patched(Module m);                               // SysPatch or File

const char* module_name(Module m);   // for the diagnostic report

}   // namespace patches
