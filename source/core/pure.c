// PlayGuard — console-free part of the service layer (see pure.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "pure.h"
#include "pctl_ops.h"
#include "sysinfo.h"
#include <string.h>

// The block as bytes, whatever the host's byte order: byte 2i is the low
// half of u16 i, as on the console.
static u8 pt_byte(const u16 c[PT_U16_COUNT], int at)
{
    return (u8)(at & 1 ? c[at / 2] >> 8 : c[at / 2] & 0xFF);
}

static void pt_set_byte(u16 c[PT_U16_COUNT], int at, u8 v)
{
    c[at / 2] = at & 1 ? (u16)((c[at / 2] & 0x00FF) | (v << 8)) : (u16)((c[at / 2] & 0xFF00) | v);
}

// Byte offsets inside a day (pure.h).
enum { PT_DAY0 = 12, PT_DAYSZ = 8, PT_BED_ON = 0, PT_BED_H, PT_BED_M, PT_END_H, PT_END_M, PT_LIMIT_ON };

static int pt_day(int n) { return PT_DAY0 + PT_DAYSZ * n; }

static bool pt_any_bedtime(const u16 c[PT_U16_COUNT])
{
    for (int n = 0; n < 7; n++)
        if (pt_byte(c, pt_day(n) + PT_BED_ON)) return true;
    return false;
}

// The limit flag alone: the low half of [+1] is the bedtime's allowed-again
// minute.
static bool pt_has_limit(const u16 c[PT_U16_COUNT], int n)
{
    return pt_byte(c, pt_day(n) + PT_LIMIT_ON) != 0;
}

void pt_decode(const u16 c[PT_U16_COUNT], u16 days_min[7])
{
    for (int n = 0; n < 7; n++)
        days_min[n] = pt_has_limit(c, n) ? c[7 + 4 * n + 2] : PT_DAY_NOLIMIT;
}

// One 8-byte rule at byte `d` (a day, or the header's): see pure.h.
static bool pt_rule_plausible(const u16 c[PT_U16_COUNT], int d)
{
    const u16 minutes = (u16)(pt_byte(c, d + 6) | (pt_byte(c, d + 7) << 8));
    return pt_byte(c, d + PT_BED_ON) <= 1 && pt_byte(c, d + PT_BED_H) < 24 && pt_byte(c, d + PT_BED_M) < 60 &&
           pt_byte(c, d + PT_END_H) < 24 && pt_byte(c, d + PT_END_M) < 60 && pt_byte(c, d + PT_LIMIT_ON) <= 1 &&
           (minutes <= 1440 || minutes == PT_DAY_NOLIMIT);
}

bool pt_plausible(const u16 c[PT_U16_COUNT])
{
    // The four mode bytes (00..03) are not decoded: an unseen companion-app
    // setting may live there, so any value passes.
    if (!pt_rule_plausible(c, 4)) return false;
    for (int n = 0; n < 7; n++)
        if (!pt_rule_plausible(c, pt_day(n))) return false;
    return true;
}

void pt_encode(u16 c[PT_U16_COUNT], const u16 days_min[7])
{
    bool any = false;
    for (int n = 0; n < 7; n++) if (days_min[n] != PT_DAY_NOLIMIT) any = true;
    if (!any && !pt_any_bedtime(c)) {
        memset(c, 0, PT_U16_COUNT * sizeof(u16));
        return;
    }
    if (c[0] == 0) {
        c[0] = 0x0101;
        c[1] = 0x0001;
    }
    for (int n = 0; n < 7; n++) {
        u16 *g = &c[7 + 4 * n];
        const bool had = pt_has_limit(c, n);
        // Its bedtime's times share [+0] and [+1] with the limit flag.
        const bool bed = pt_byte(c, pt_day(n) + PT_BED_ON) != 0;
        if (days_min[n] == PT_DAY_NOLIMIT) {
            if (had && bed) {
                pt_set_byte(c, pt_day(n) + PT_LIMIT_ON, 0);
                g[2] = 0;
            } else if (had) {
                g[0] = g[1] = g[2] = 0;
            }
            continue;
        }
        if (!had && bed) {
            pt_set_byte(c, pt_day(n) + PT_LIMIT_ON, 1);
        } else if (!had) {
            g[0] = 0x0600;
            g[1] = 0x0100;
        }
        g[2] = days_min[n];
    }
}

bool pt_bedtime_ok(const PtBedtime *b)
{
    if (!b->on) return true;
    const unsigned end = b->end_hour * 60u + b->end_minute;
    return b->hour >= 16 && b->hour <= 23 && b->minute < 60 && b->end_minute < 60 &&
           end >= 5 * 60 && end <= 9 * 60;
}

void pt_bedtime_decode(const u16 c[PT_U16_COUNT], PtBedtime out[7])
{
    for (int n = 0; n < 7; n++) {
        const int d = pt_day(n);
        out[n].on         = pt_byte(c, d + PT_BED_ON) != 0;
        out[n].hour       = pt_byte(c, d + PT_BED_H);
        out[n].minute     = pt_byte(c, d + PT_BED_M);
        out[n].end_hour   = pt_byte(c, d + PT_END_H);
        out[n].end_minute = pt_byte(c, d + PT_END_M);
    }
}

void pt_bedtime_encode(u16 c[PT_U16_COUNT], const PtBedtime in[7])
{
    PtBedtime now[7];
    pt_bedtime_decode(c, now);
    for (int n = 0; n < 7; n++) {
        const int d = pt_day(n);
        if (in[n].on == now[n].on && (!in[n].on || (in[n].hour == now[n].hour && in[n].minute == now[n].minute &&
                                                    in[n].end_hour == now[n].end_hour &&
                                                    in[n].end_minute == now[n].end_minute)))
            continue;   // unchanged: left exactly as read
        pt_set_byte(c, d + PT_BED_ON, in[n].on ? 1 : 0);
        pt_set_byte(c, d + PT_BED_H, in[n].on ? in[n].hour : 0);
        pt_set_byte(c, d + PT_BED_M, in[n].on ? in[n].minute : 0);
        if (in[n].on) {
            pt_set_byte(c, d + PT_END_H, in[n].end_hour);
            pt_set_byte(c, d + PT_END_M, in[n].end_minute);
        }
    }
    u16 days[7];
    pt_decode(c, days);
    bool any = pt_any_bedtime(c);
    for (int n = 0; n < 7; n++) if (days[n] != PT_DAY_NOLIMIT) any = true;
    if (!any) {
        memset(c, 0, PT_U16_COUNT * sizeof(u16));
    } else if (c[0] == 0) {
        c[0] = 0x0101;
        c[1] = 0x0001;
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
