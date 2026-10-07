// PlayGuard — console-free part of the service layer (see pure.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "pure.h"
#include "pctl_ops.h"
#include "sysinfo.h"
#include <string.h>

void pt_decode(const u16 c[PT_U16_COUNT], u16 days_min[7])
{
    for (int n = 0; n < 7; n++)
        days_min[n] = c[7 + 4 * n + 1] ? c[7 + 4 * n + 2] : PT_DAY_NOLIMIT;
}

void pt_encode(u16 c[PT_U16_COUNT], const u16 days_min[7])
{
    bool any = false;
    for (int n = 0; n < 7; n++) if (days_min[n] != PT_DAY_NOLIMIT) any = true;
    if (!any) {
        memset(c, 0, PT_U16_COUNT * sizeof(u16));
        return;
    }
    if (c[0] == 0) {
        c[0] = 0x0101;
        c[1] = 0x0001;
    }
    for (int n = 0; n < 7; n++) {
        u16 *g = &c[7 + 4 * n];
        const bool had = g[1] != 0;
        if (days_min[n] == PT_DAY_NOLIMIT) {
            if (had) g[0] = g[1] = g[2] = 0;
            continue;
        }
        if (!had) {
            g[0] = 0x0600;
            g[1] = 0x0100;
        }
        g[2] = days_min[n];
    }
}

const char *pctl_safety_level_name(u32 level)
{
    switch (level) {
        case PctlSafetyLevel_None:       return "None";
        case PctlSafetyLevel_Custom:     return "Custom";
        case PctlSafetyLevel_YoungChild: return "Young Child";
        case PctlSafetyLevel_Child:      return "Child";
        case PctlSafetyLevel_Teen:       return "Teen";
        default:                         return "Unknown";
    }
}

const char *pctl_rating_org_name(u32 org)
{
    // nn::ns::RatingOrganization
    static const char *names[] = {
        "CERO", "GRAC", "GSRMR", "ESRB", "ClassInd", "USK", "PEGI",
        "PEGI Portugal", "PEGI BBFC", "Russian", "ACB", "OFLC", "IARC Generic",
    };
    return org < sizeof(names) / sizeof(names[0]) ? names[org] : "?";
}

SysCompat sysinfo_compat(const SysInfo *info)
{
    if (!info->is_atmosphere)                       return SysCompat_NotAtmosphere;
    if (info->hos_version < PCTL_FW_MIN_PLAYTIMER)  return SysCompat_PlayTimerUnsupported;
    if (info->hos_version > PCTL_FW_TESTED_MAX)     return SysCompat_UntestedNewer;
    return SysCompat_Ok;
}
