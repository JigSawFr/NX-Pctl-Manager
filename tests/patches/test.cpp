// Host tests for source/util/patches.cpp: sys-patch log parsing, stale logs,
// sigpatch file detection and the overall verdict, on fake SD card trees.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/paths.hpp"
#include "util/patches.hpp"

using namespace patches;

static std::string g_base;
static int g_n = 0;

static std::string fresh_root()
{
    std::string root = g_base + "/sd" + std::to_string(g_n++);
    assert(paths::ensure_dir(root));
    return root;
}

static void put(const std::string& root, const std::string& rel, const std::string& content = "x")
{
    assert(paths::atomic_write(root + "/" + rel, content));
}

static const char* LOG_OK =
    "[fs]\n"
    "noncasigchk_old=Unpatched\n"
    "noncasigchk_new=Patched (sys-patch) (0x7A1B0)\n"
    "nocntchk=Patched (sys-patch) (0x8C2D4)\n"
    "[ldr]\n"
    "noacidsigchk=Patched (sys-patch) (0x6E8)\n"
    "[es]\n"
    "es1=Unpatched\n"
    "es2=Patched (file)\n"
    "[nifm]\n"
    "ctest=Skipped\n"
    "[stats]\n"
    "version=v1.5.9\n"
    "fw_version=23.0.1\n"
    "is_emummc=1\n";

static void install_syspatch(const std::string& root, bool at_boot)
{
    put(root, "atmosphere/contents/420000000000000B/exefs.nsp");
    if (at_boot) put(root, "atmosphere/contents/420000000000000B/flags/boot2.flag", "");
}

int main()
{
    char tmpl[] = "/tmp/playguard_patches_XXXXXX";
    assert(mkdtemp(tmpl));
    g_base = tmpl;

    // Parser: sections, comments, blank lines, spaces, CRLF, no trailing newline.
    {
        Ini ini = parse_ini("; comment\r\n[a]\r\n k = v w \r\n\r\n[b]\nx=1\ny");
        assert(ini["a"]["k"] == "v w" && ini["b"]["x"] == "1" && ini["b"].count("y") == 0);
        assert(parse_ini("").empty());
    }

    // Module aggregation.
    {
        Ini ini = parse_ini(LOG_OK);
        assert(module_state(ini, "fs") == Module::SysPatch);
        assert(module_state(ini, "ldr") == Module::SysPatch);
        assert(module_state(ini, "es") == Module::File);
        assert(module_state(ini, "nifm") == Module::Skipped);
        assert(module_state(ini, "nim") == Module::Missing);
        Ini other = parse_ini("[a]\np=Unpatched\nq=Disabled\n[b]\np=Disabled\n[c]\np=Failed (svcWriteDebugProcessMemory)\nq=Unpatched\n");
        assert(module_state(other, "a") == Module::Unpatched);
        assert(module_state(other, "b") == Module::Disabled);
        assert(module_state(other, "c") == Module::Failed);
    }

    // Nothing at all.
    {
        Report r = detect(fresh_root(), "23.0.1", true);
        assert(!r.syspatch_installed && r.files.empty() && !r.log_present);
        assert(status(r) == Status::None && issue(r) == Issue::None);
    }

    // Sigpatch files only (Atmosphère folders + Hekate), upper-case extension too.
    {
        std::string root = fresh_root();
        put(root, "atmosphere/exefs_patches/es_patches/ABCDEF.ips");
        put(root, "atmosphere/kip_patches/fs_patches/0123.IPS");
        put(root, "atmosphere/kip_patches/loader_patches/readme.txt");   // not a patch
        put(root, "bootloader/patches.ini");
        Report r = detect(root, "23.0.1", false);
        assert((r.files == std::vector<std::string>{ "es", "fs", "hekate" }));
        assert(status(r) == Status::FilesOnly);
    }

    // sys-patch working on this boot (emuMMC), plus leftover files.
    {
        std::string root = fresh_root();
        install_syspatch(root, true);
        put(root, "config/sys-patch/log.ini", LOG_OK);
        put(root, "atmosphere/exefs_patches/es_patches/ABCDEF.ips");
        Report r = detect(root, "23.0.1", true);
        assert(r.log_present && r.log_current && r.log_fw == "23.0.1" && r.log_emummc);
        assert(status(r) == Status::SysPatch && issue(r) == Issue::None);
        assert(r.files.size() == 1 && unpatched(r).empty());

        // Same log seen from sysMMC, or after a firmware update: stale.
        r = detect(root, "23.0.1", false);
        assert(!r.log_current && issue(r) == Issue::StaleLog && status(r) == Status::SysPatchIncomplete);
        r = detect(root, "23.1.0", true);
        assert(issue(r) == Issue::StaleLog);
    }

    // Installed but not started at boot.
    {
        std::string root = fresh_root();
        install_syspatch(root, false);
        put(root, "config/sys-patch/log.ini", LOG_OK);
        Report r = detect(root, "23.0.1", true);
        assert(issue(r) == Issue::NotAtBoot && status(r) == Status::SysPatchIncomplete);
    }

    // No log (logging off, or never ran); empty log counts as none.
    {
        std::string root = fresh_root();
        install_syspatch(root, true);
        assert(issue(detect(root, "23.0.1", true)) == Issue::NoLog);
        put(root, "config/sys-patch/log.ini", "");
        assert(issue(detect(root, "23.0.1", true)) == Issue::NoLog);
    }

    // Turned off for this storage (patch_sysmmc=0).
    {
        std::string root = fresh_root();
        install_syspatch(root, true);
        put(root, "config/sys-patch/log.ini",
            "[fs]\na=Skipped\n[ldr]\nb=Skipped\n[es]\nc=Skipped\n[stats]\nfw_version=23.0.1\nis_emummc=0\n");
        Report r = detect(root, "23.0.1", false);
        assert(issue(r) == Issue::Skipped && status(r) == Status::SysPatchIncomplete);
    }

    // A module left unpatched (firmware newer than sys-patch's patterns).
    {
        std::string root = fresh_root();
        install_syspatch(root, true);
        put(root, "config/sys-patch/log.ini",
            "[fs]\na=Patched (sys-patch)\n[ldr]\nb=Patched (sys-patch)\n[es]\nc=Unpatched\nd=Failed (svcWriteDebugProcessMemory)\n"
            "[stats]\nfw_version=23.0.1\nis_emummc=0\n");
        Report r = detect(root, "23.0.1", false);
        assert(issue(r) == Issue::NotPatched);
        assert((unpatched(r) == std::vector<std::string>{ "es" }));
        assert(r.es == Module::Failed);
    }

    std::string cmd = "rm -rf '" + g_base + "'";
    assert(std::system(cmd.c_str()) == 0);
    std::puts("patches detection assertions passed");
    return 0;
}
