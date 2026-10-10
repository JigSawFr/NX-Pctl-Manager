// Host tests for source/core/sysinfo.c: Atmosphère version decoding, emuMMC
// and PRODINFO-blank detection, the serial number, session release, applet
// detection and the compatibility policy.
#include "check.h"
#include <stdio.h>
#include <string.h>
#include "sysinfo.h"

void sysinfo_reset_cache(void);

static struct {
    u32 hos; bool ams; AppletType applet;
    int spl_refs; unsigned spl_inits; bool spl_fail;
    int set_refs; bool set_fail; bool serial_fail;
    bool emummc_fail; u64 emummc;
    bool blank_fail; u64 blank;
    const char *serial;
} m;

u32 hosversionGet(void) { return m.hos; }
bool hosversionIsAtmosphere(void) { return m.ams; }
Result splInitialize(void) { m.spl_inits++; if (m.spl_fail) return 1; m.spl_refs++; return 0; }
void splExit(void) { CHECK(m.spl_refs == 1); m.spl_refs--; }
Result splGetConfig(SplConfigItem item, u64 *out)
{
    CHECK(m.spl_refs == 1);
    if (item == 65000) { *out = (1ULL << 56) | (12ULL << 48) | (0ULL << 40) | 0x17000000ULL; return 0; }
    if (item == 65007) { if (m.emummc_fail) return 1; *out = m.emummc; return 0; }
    if (item == 65005) { if (m.blank_fail) return 1; *out = m.blank; return 0; }
    return 1;
}
AppletType appletGetAppletType(void) { return m.applet; }
Result setsysInitialize(void) { if (m.set_fail) return 1; m.set_refs++; return 0; }
void setsysExit(void) { CHECK(m.set_refs == 1); m.set_refs--; }
Result setsysGetSerialNumber(SetSysSerialNumber *out)
{
    CHECK(m.set_refs == 1);
    if (m.serial_fail) return 1;
    memset(out, 0, sizeof(*out));
    strncpy(out->number, m.serial, sizeof(out->number));
    return 0;
}

static void reset(void)
{
    memset(&m, 0, sizeof(m));
    m.hos = MAKEHOSVERSION(23, 0, 1);
    m.ams = true;
    m.applet = AppletType_Application;
    m.serial = "XAW10000000001";
    sysinfo_reset_cache();
}

static SysInfo get(void)
{
    SysInfo si;
    sysinfo_get(&si);
    CHECK(m.spl_refs == 0 && m.set_refs == 0);   // every session released
    return si;
}

int main(void)
{
    SysInfo si;

    // Version, applet, cache; sysMMC with the real serial.
    reset();
    m.applet = AppletType_LibraryApplet;
    si = get();
    CHECK(si.hos_version == MAKEHOSVERSION(23, 0, 1) && si.is_atmosphere);
    CHECK(si.ams_valid && si.ams_major == 1 && si.ams_minor == 12 && si.ams_micro == 0);
    CHECK(si.applet_mode);
    CHECK(si.emummc_valid && !si.emummc && sysinfo_storage(&si) == SysStorage_SysMMC);
    CHECK(si.blank_valid && !si.blank);
    CHECK(si.serial_valid && strcmp(si.serial, "XAW10000000001") == 0);
    sysinfo_get(&si);
    CHECK(m.spl_inits == 1); /* cached */
    CHECK(sysinfo_fw_at_least(MAKEHOSVERSION(21, 0, 0)));

    // emuMMC, PRODINFO blanked.
    reset();
    m.emummc = 1; m.blank = 1; m.serial = SYSINFO_BLANK_SERIAL;
    si = get();
    CHECK(sysinfo_storage(&si) == SysStorage_EmuMMC);
    CHECK(si.blank_valid && si.blank && strcmp(si.serial, SYSINFO_BLANK_SERIAL) == 0);

    // 65005 unknown (older Atmosphère): deduced from the serial, both ways.
    reset();
    m.blank_fail = true; m.serial = SYSINFO_BLANK_SERIAL;
    si = get();
    CHECK(si.blank_valid && si.blank);
    reset();
    m.blank_fail = true;
    si = get();
    CHECK(si.blank_valid && !si.blank);

    // Neither 65005 nor the serial: unknown, never "not blanked".
    reset();
    m.blank_fail = true; m.serial_fail = true;
    si = get();
    CHECK(!si.blank_valid && !si.serial_valid && si.serial[0] == '\0');

    // set:sys unavailable.
    reset();
    m.set_fail = true;
    si = get();
    CHECK(!si.serial_valid && si.blank_valid);

    // 65007 unknown -> storage unknown.
    reset();
    m.emummc_fail = true;
    si = get();
    CHECK(!si.emummc_valid && sysinfo_storage(&si) == SysStorage_Unknown);

    // No Atmosphère: spl is not asked, storage unknown, blank not deduced.
    reset();
    m.ams = false;
    si = get();
    CHECK(m.spl_inits == 0 && !si.ams_valid && !si.blank_valid);
    CHECK(sysinfo_storage(&si) == SysStorage_Unknown && si.serial_valid);

    // spl unavailable.
    reset();
    m.spl_fail = true;
    si = get();
    CHECK(!si.ams_valid && !si.emummc_valid && sysinfo_storage(&si) == SysStorage_Unknown);

    char buf[16];
    sysinfo_version_string(MAKEHOSVERSION(22, 5, 0), buf, sizeof(buf));
    CHECK(strcmp(buf, "22.5.0") == 0);
    sysinfo_version_string(0, buf, sizeof(buf));
    CHECK(strcmp(buf, "?") == 0);

    reset();
    SysInfo t = get();
    CHECK(sysinfo_compat(&t) == SysCompat_UntestedNewer);   /* 23.0.1 */
    t.hos_version = MAKEHOSVERSION(22, 1, 0);  CHECK(sysinfo_compat(&t) == SysCompat_Ok);
    t.hos_version = MAKEHOSVERSION(23, 1, 0);  CHECK(sysinfo_compat(&t) == SysCompat_UntestedNewer);
    // The command table was checked up to 23.0.1, but no console trace yet:
    // 23.x asks first, the newest verified firmware does not.
    t.hos_version = MAKEHOSVERSION(23, 0, 1);  CHECK(sysinfo_compat(&t) == SysCompat_UntestedNewer);
    t.hos_version = MAKEHOSVERSION(23, 0, 0);  CHECK(sysinfo_compat(&t) == SysCompat_UntestedNewer);
    t.hos_version = MAKEHOSVERSION(22, 5, 0);  CHECK(sysinfo_compat(&t) == SysCompat_Ok);
    CHECK(PCTL_FW_TESTED_MAX <= PCTL_FW_TABLE_CHECKED);
    t.hos_version = MAKEHOSVERSION(20, 5, 0);  CHECK(sysinfo_compat(&t) == SysCompat_PlayTimerUnsupported);
    t.is_atmosphere = false;                   CHECK(sysinfo_compat(&t) == SysCompat_NotAtmosphere);
    return CHECK_DONE("sysinfo assertions passed");
}
