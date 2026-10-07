// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "sysinfo.h"
#include <stdio.h>
#include <string.h>

static SysInfo s_info;
static bool    s_cached = false;

void sysinfo_get(SysInfo *out)
{
    if (!s_cached) {
        memset(&s_info, 0, sizeof(s_info));
        s_info.hos_version   = hosversionGet();
        s_info.is_atmosphere = hosversionIsAtmosphere();

        // Exosphère exposes its own version through spl config item 65000
        // (ExosphereApiVersion): major/minor/micro live in bits 56/48/40.
        if (s_info.is_atmosphere && R_SUCCEEDED(splInitialize())) {
            u64 v = 0;
            if (R_SUCCEEDED(splGetConfig((SplConfigItem)65000, &v))) {
                s_info.ams_major = (u8)((v >> 56) & 0xFF);
                s_info.ams_minor = (u8)((v >> 48) & 0xFF);
                s_info.ams_micro = (u8)((v >> 40) & 0xFF);
                s_info.ams_valid = true;
            }
            // 65007 == ExosphereEmummcType (0 == sysMMC).
            u64 emu = 0;
            if (R_SUCCEEDED(splGetConfig((SplConfigItem)65007, &emu))) {
                s_info.emummc       = emu != 0;
                s_info.emummc_valid = true;
            }
            // 65005 == ExosphereBlankProdInfo: blank_prodinfo_{sys,emu}mmc for this boot.
            u64 blank = 0;
            if (R_SUCCEEDED(splGetConfig((SplConfigItem)65005, &blank))) {
                s_info.blank       = blank != 0;
                s_info.blank_valid = true;
            }
            splExit();
        }

        // The serial number as the system sees it (blanked or real).
        if (R_SUCCEEDED(setsysInitialize())) {
            SetSysSerialNumber sn;
            memset(&sn, 0, sizeof(sn));
            if (R_SUCCEEDED(setsysGetSerialNumber(&sn))) {
                memcpy(s_info.serial, sn.number, sizeof(s_info.serial) - 1);
                s_info.serial[sizeof(s_info.serial) - 1] = '\0';
                s_info.serial_valid = true;
            }
            setsysExit();
        }
        // Older Atmosphère without 65005: the blanked serial gives it away.
        if (!s_info.blank_valid && s_info.is_atmosphere && s_info.serial_valid) {
            s_info.blank       = strcmp(s_info.serial, SYSINFO_BLANK_SERIAL) == 0;
            s_info.blank_valid = true;
        }
        s_info.applet_mode = appletGetAppletType() == AppletType_LibraryApplet;
        s_cached = true;
    }
    *out = s_info;
}

#ifdef NX_HOST_TEST
void sysinfo_reset_cache(void) { s_cached = false; }
#endif

SysCompat sysinfo_compat(const SysInfo *info)
{
    if (!info->is_atmosphere)                       return SysCompat_NotAtmosphere;
    if (info->hos_version < PCTL_FW_MIN_PLAYTIMER)  return SysCompat_PlayTimerUnsupported;
    if (info->hos_version > PCTL_FW_TESTED_MAX)     return SysCompat_UntestedNewer;
    return SysCompat_Ok;
}

bool sysinfo_fw_at_least(u32 version)
{
    SysInfo info;
    sysinfo_get(&info);
    return info.hos_version >= version;
}

void sysinfo_version_string(u32 version, char *buf, size_t size)
{
    if (!version) { snprintf(buf, size, "?"); return; }
    snprintf(buf, size, "%u.%u.%u", (unsigned)HOSVER_MAJOR(version),
             (unsigned)HOSVER_MINOR(version), (unsigned)HOSVER_MICRO(version));
}
