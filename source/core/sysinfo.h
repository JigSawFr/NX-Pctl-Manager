// sysinfo — firmware / Atmosphère detection and the compatibility policy.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

// Firmware policy. PlayTimerSettings (cmds 145601 / 195101) has been 0x44 bytes
// since 21.0.0 — the only layout this tool knows how to read and write.
#define PCTL_FW_MIN_PLAYTIMER MAKEHOSVERSION(21, 0, 0)
// Newest firmware the command table was checked against (switchbrew, Oct 2026).
#define PCTL_FW_TESTED_MAX    MAKEHOSVERSION(23, 0, 1)
// Atmosphère release that supports PCTL_FW_TESTED_MAX.
#define PCTL_AMS_TESTED_MAJOR 1
#define PCTL_AMS_TESTED_MINOR 12
#define PCTL_AMS_TESTED_MICRO 0

typedef enum {
    SysCompat_Ok = 0,              // within the checked range
    SysCompat_UntestedNewer,       // newer than PCTL_FW_TESTED_MAX: reads fine, writes at own risk
    SysCompat_PlayTimerUnsupported,// older than 21.0.0: play-timer features disabled
    SysCompat_NotAtmosphere,       // not running under Atmosphère
} SysCompat;

// Serial number the system reports while Atmosphère blanks PRODINFO
// (ams_mitm, amsmitm_prodinfo_utils.cpp: BlankSerialNumberString).
#define SYSINFO_BLANK_SERIAL "XAW00000000000"

typedef struct {
    u32  hos_version;    // MAKEHOSVERSION layout; 0 if unknown
    bool is_atmosphere;
    bool ams_valid;
    u8   ams_major, ams_minor, ams_micro;
    bool applet_mode;    // running as a library applet (album launch): reduced memory
    bool emummc_valid;   // spl item 65007 (ExosphereEmummcType) was read
    bool emummc;         // emuMMC active (false when unknown)
    bool blank_valid;    // spl item 65005 (ExosphereBlankProdInfo) read, or deduced from the serial
    bool blank;          // PRODINFO blanked for this boot: the system sees SYSINFO_BLANK_SERIAL
    bool serial_valid;   // set:sys GetSerialNumber succeeded
    char serial[0x19];   // as the system reports it; NUL-terminated. Never written to reports.
} SysInfo;

typedef enum {
    SysStorage_Unknown = 0,   // Atmosphère not detected, or the emuMMC state could not be read
    SysStorage_SysMMC,        // internal NAND, Atmosphère running
    SysStorage_EmuMMC,        // emulated NAND on the SD card, Atmosphère running
} SysStorage;

void      sysinfo_get(SysInfo *out);           // cached after the first call
SysCompat sysinfo_compat(const SysInfo *info);

static inline SysStorage sysinfo_storage(const SysInfo *info)
{
    if (!info->is_atmosphere || !info->emummc_valid) return SysStorage_Unknown;
    return info->emummc ? SysStorage_EmuMMC : SysStorage_SysMMC;
}
bool      sysinfo_fw_at_least(u32 version);    // compares against hos_version
// "23.0.1" style string. buf must hold at least 16 bytes.
void      sysinfo_version_string(u32 version, char *buf, size_t size);
