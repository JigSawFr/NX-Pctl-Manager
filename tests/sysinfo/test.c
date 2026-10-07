// Host tests for source/core/sysinfo.c: Atmosphère version decoding, spl
// session release, applet detection and the compatibility policy.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sysinfo.h"

static struct { u32 hos; bool ams; int spl_refs; unsigned spl_inits; bool spl_fail; AppletType applet; } m;

u32 hosversionGet(void) { return m.hos; }
bool hosversionIsAtmosphere(void) { return m.ams; }
Result splInitialize(void) { m.spl_inits++; if (m.spl_fail) return 1; m.spl_refs++; return 0; }
void splExit(void) { assert(m.spl_refs == 1); m.spl_refs--; }
Result splGetConfig(SplConfigItem item, u64 *out)
{
    assert(m.spl_refs == 1);
    if (item == 65000) { *out = (1ULL << 56) | (12ULL << 48) | (0ULL << 40) | 0x17000000ULL; return 0; }
    if (item == 65007) { *out = 0; return 0; }
    return 1;
}
AppletType appletGetAppletType(void) { return m.applet; }

int main(void)
{
    SysInfo si;
    m.hos = MAKEHOSVERSION(23, 0, 1);
    m.ams = true;
    m.applet = AppletType_LibraryApplet;
    sysinfo_get(&si);
    assert(si.hos_version == MAKEHOSVERSION(23, 0, 1) && si.is_atmosphere);
    assert(si.ams_valid && si.ams_major == 1 && si.ams_minor == 12 && si.ams_micro == 0);
    assert(si.applet_mode && !si.emummc && m.spl_refs == 0);
    sysinfo_get(&si);
    assert(m.spl_inits == 1); /* cached */
    assert(sysinfo_fw_at_least(MAKEHOSVERSION(21, 0, 0)));

    char buf[16];
    sysinfo_version_string(MAKEHOSVERSION(22, 5, 0), buf, sizeof(buf));
    assert(strcmp(buf, "22.5.0") == 0);
    sysinfo_version_string(0, buf, sizeof(buf));
    assert(strcmp(buf, "?") == 0);

    SysInfo t = si;
    assert(sysinfo_compat(&t) == SysCompat_Ok);
    t.hos_version = MAKEHOSVERSION(22, 1, 0);  assert(sysinfo_compat(&t) == SysCompat_Ok);
    t.hos_version = MAKEHOSVERSION(23, 1, 0);  assert(sysinfo_compat(&t) == SysCompat_UntestedNewer);
    t.hos_version = MAKEHOSVERSION(20, 5, 0);  assert(sysinfo_compat(&t) == SysCompat_PlayTimerUnsupported);
    t.is_atmosphere = false;                   assert(sysinfo_compat(&t) == SysCompat_NotAtmosphere);
    puts("sysinfo assertions passed");
    return 0;
}
