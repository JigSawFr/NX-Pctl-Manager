// Simulated service layer for the desktop (PLATFORM_DESKTOP) build.
//
// Implements pctl_ops.h, time_ops.h, sysinfo.h and playstats.h with in-memory
// state so the whole borealis UI can be built, navigated and screenshotted on a
// PC. Never compiled for the Switch. Environment knobs:
//   PLAYGUARD_SIM_FW=20.5.0     pretend to run on another firmware
//   PLAYGUARD_SIM_NO_CFW=1      make pctl_ops_init fail (init error screen)
//   PLAYGUARD_SIM_TIMER_OFF=1   start with no play timer configured
//   PLAYGUARD_SIM_UNLOCKED=1    start with parental controls temporarily unlocked
//   PLAYGUARD_SIM_UNPAIRED=1    start with no companion app linked
//   PLAYGUARD_SIM_ACCURATE=1    report the network clock as accurate
//   PLAYGUARD_SIM_EMUMMC=1      running on emuMMC (default: sysMMC)
//   PLAYGUARD_SIM_BLANK=1       PRODINFO blanked (serial XAW00000000000)
//   PLAYGUARD_SIM_NO_PDM=1      the play-data service (Activity tab) fails
// Game patches are read from ./playguard_data/sd/ (the simulated SD card root).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../core/pctl_ops.h"
#include "../core/playstats.h"
#include "../core/sysinfo.h"
#include "../core/time_ops.h"
#include "../core/write_guard.h"

static struct {
    bool init;
    u32  hos;
    u32  safety_level;
    u32  pin_length;
    bool restriction_enabled, temp_unlocked, pairing_active, stereo_restricted, alarm_disabled, paused;
    PctlCustomSettings custom;
    u16  day_min[7];
    s64  clock_offset;
} S;

static void sim_init(void)
{
    if (S.init) return;
    S.init = true;
    S.hos = MAKEHOSVERSION(23, 0, 1);
    const char *fw = getenv("PLAYGUARD_SIM_FW");
    unsigned a, b, c;
    if (fw && sscanf(fw, "%u.%u.%u", &a, &b, &c) == 3) S.hos = MAKEHOSVERSION(a, b, c);
    S.safety_level = PctlSafetyLevel_Child;
    S.pin_length = 4;
    S.restriction_enabled = true;
    S.pairing_active = getenv("PLAYGUARD_SIM_UNPAIRED") == NULL;
    S.temp_unlocked = getenv("PLAYGUARD_SIM_UNLOCKED") != NULL;
    S.custom.rating_age = 12;
    S.custom.sns_post_restriction = true;
    bool off = getenv("PLAYGUARD_SIM_TIMER_OFF") != NULL;
    for (int i = 0; i < 7; i++) S.day_min[i] = off ? PT_DAY_NOLIMIT : ((i == 0 || i == 6) ? 180 : 120);
}

static bool timer_enabled(void)
{
    if (S.temp_unlocked) return false;
    for (int i = 0; i < 7; i++) if (S.day_min[i] != PT_DAY_NOLIMIT) return true;
    return false;
}

// ---------------------------------------------------------------- sysinfo
void sysinfo_get(SysInfo *out)
{
    sim_init();
    memset(out, 0, sizeof(*out));
    out->hos_version = S.hos;
    out->is_atmosphere = true;
    out->ams_valid = true;
    out->ams_major = 1; out->ams_minor = 12; out->ams_micro = 0;
    out->emummc_valid = true;
    out->emummc = getenv("PLAYGUARD_SIM_EMUMMC") != NULL;
    out->blank_valid = true;
    out->blank = getenv("PLAYGUARD_SIM_BLANK") != NULL;
    out->serial_valid = true;
    snprintf(out->serial, sizeof(out->serial), "%s", out->blank ? SYSINFO_BLANK_SERIAL : "XAW10000000001");
}
SysCompat sysinfo_compat(const SysInfo *info)
{
    if (!info->is_atmosphere)                      return SysCompat_NotAtmosphere;
    if (info->hos_version < PCTL_FW_MIN_PLAYTIMER) return SysCompat_PlayTimerUnsupported;
    if (info->hos_version > PCTL_FW_TESTED_MAX)    return SysCompat_UntestedNewer;
    return SysCompat_Ok;
}
bool sysinfo_fw_at_least(u32 v) { sim_init(); return S.hos >= v; }
void sysinfo_version_string(u32 v, char *buf, size_t size)
{
    if (!v) { snprintf(buf, size, "?"); return; }
    snprintf(buf, size, "%u.%u.%u", (unsigned)HOSVER_MAJOR(v), (unsigned)HOSVER_MINOR(v), (unsigned)HOSVER_MICRO(v));
}

// ---------------------------------------------------------------- pctl
Result pctl_ops_init(void)   { sim_init(); return getenv("PLAYGUARD_SIM_NO_CFW") ? (Result)0x0C15 : 0; }
void   pctl_ops_exit(void)   {}
Result pctl_ops_reinit(void) { return pctl_ops_init(); }

void pctl_status_fetch(PctlStatus *o)
{
    memset(o, 0, sizeof(*o));
    if (R_FAILED(o->session_rc = pctl_ops_init())) return;
    o->safety_level_ok = true;        o->safety_level = S.safety_level;
    o->pin_length_ok = true;          o->pin_length = S.pin_length;
    o->restriction_enabled_ok = true; o->restriction_enabled = S.restriction_enabled;
    o->temp_unlocked_ok = true;       o->temp_unlocked = S.temp_unlocked;
    o->pairing_active_ok = true;      o->pairing_active = S.pairing_active;
    o->settings_ok = true;            o->settings = S.custom;
    o->rating_org_ok = true;          o->rating_org = 6;
    o->stereo_vision_ok = true;       o->stereo_vision_restricted = S.stereo_restricted;
    o->free_comm_count_ok = true;     o->free_comm_count = 3;
    o->last_updated_ok = true;        o->last_updated = (u64)time(NULL) - 3600 * 26;
}

const char *pctl_safety_level_name(u32 level)
{
    static const char *n[] = {"None", "Custom", "Young Child", "Child", "Teen"};
    return level < 5 ? n[level] : "Unknown";
}
const char *pctl_rating_org_name(u32 org)
{
    static const char *names[] = {"CERO", "GRAC", "GSRMR", "ESRB", "ClassInd", "USK", "PEGI",
        "PEGI Portugal", "PEGI BBFC", "Russian", "ACB", "OFLC", "IARC Generic"};
    return org < 13 ? names[org] : "?";
}

// write_guard.h (core/write_guard.c is not compiled on desktop).
static bool s_read_only = false;
void core_set_read_only(bool on) { s_read_only = on; }
bool core_read_only(void)        { return s_read_only; }

#define RO_GUARD() do { if (s_read_only) return NXM_RC_READ_ONLY; } while (0)

Result pctl_set_pin(void)                         { RO_GUARD(); S.pin_length = 4; return 0; }
Result pctl_unlock_restriction_temporarily(void)  { RO_GUARD(); if (!S.pin_length) return 0x1A08E; S.temp_unlocked = true; return 0; }
Result pctl_get_pin(char *out, size_t out_size)
{
    if (out && out_size) memset(out, 0, out_size);
    RO_GUARD();
    if (!out || out_size < 5) return NXM_RC_INVALID_ARGUMENT;
    if (!S.pin_length) return NXM_RC_STATE_UNKNOWN;
    snprintf(out, out_size, "1234");
    return 0;
}
Result pctl_relock(void)                          { RO_GUARD(); S.temp_unlocked = false; return 0; }
Result pctl_delete_parental_controls(void)
{
    RO_GUARD();
    S.pin_length = 0; S.restriction_enabled = false; S.safety_level = 0; S.temp_unlocked = false;
    for (int i = 0; i < 7; i++) S.day_min[i] = PT_DAY_NOLIMIT;
    return 0;
}
Result pctl_delete_pairing(void)                  { RO_GUARD(); S.pairing_active = false; return 0; }
Result pctl_set_safety_level(u32 l)               { RO_GUARD(); if (l > 4) return NXM_RC_INVALID_ARGUMENT; S.safety_level = l; return 0; }
Result pctl_set_custom_settings(const PctlCustomSettings *c)
{
    RO_GUARD();
    if (S.safety_level != PctlSafetyLevel_Custom) return NXM_RC_NOT_CUSTOM;
    S.custom = *c;
    return 0;
}
Result pctl_set_stereo_vision_restricted(bool r)  { RO_GUARD(); S.stereo_restricted = r; return 0; }
Result pctl_play_timer_set_alarm_disabled(bool d) { RO_GUARD(); S.alarm_disabled = d; return 0; }
Result pctl_play_timer_start(void)                { RO_GUARD(); S.paused = false; return 0; }
Result pctl_play_timer_stop(void)                 { RO_GUARD(); S.paused = true; return 0; }

void pctl_play_timer_query(PtState *o)
{
    sim_init();
    memset(o, 0, sizeof(*o));
    for (int i = 0; i < 7; i++) o->day_min[i] = PT_DAY_NOLIMIT;
    o->fw_supported = S.hos >= PCTL_FW_MIN_PLAYTIMER;
    if (!o->fw_supported) return;
    if (R_FAILED(o->session_rc = pctl_ops_init())) return;
    o->session_valid = true;
    o->valid = true;
    memcpy(o->day_min, S.day_min, sizeof(S.day_min));
    o->enabled_valid = true;  o->enabled = timer_enabled();
    o->temporary_unlocked_valid = true; o->temporary_unlocked = S.temp_unlocked;
    o->remaining_valid = true;
    o->remaining_ns = o->enabled ? 45ULL * 60 * 1000000000ULL : 0;
    o->restricted_valid = true; o->restricted = false;
    o->alarm_disabled_valid = true; o->alarm_disabled = S.alarm_disabled;
    o->bedtime_valid = true; o->bedtime_enabled = true; o->bedtime_hour = 21; o->bedtime_minute = 0;
    o->bedtime_reset_valid = true; o->bedtime_reset_hour = 6; o->bedtime_reset_minute = 0;
}

Result pctl_play_timer_set_days(const u16 d[7])
{
    RO_GUARD();
    sim_init();
    if (S.hos < PCTL_FW_MIN_PLAYTIMER) return NXM_RC_FW_UNSUPPORTED;
    for (int i = 0; i < 7; i++) if (d[i] != PT_DAY_NOLIMIT && d[i] > 1440) return NXM_RC_INVALID_ARGUMENT;
    if (timer_enabled() && !S.temp_unlocked) return NXM_RC_WRITE_GATED;
    memcpy(S.day_min, d, sizeof(S.day_min));
    return 0;
}
Result pctl_play_timer_set_uniform(u16 m) { u16 d[7]; for (int i = 0; i < 7; i++) d[i] = m; return pctl_play_timer_set_days(d); }
Result pctl_play_timer_clear(void)        { u16 d[7]; for (int i = 0; i < 7; i++) d[i] = PT_DAY_NOLIMIT; return pctl_play_timer_set_days(d); }

void pctl_dump(char *buf, size_t n)
{
    snprintf(buf, n, "=== pctl state (simulated desktop backend) ===\nsafety_level=%u pin_length=%u enabled=%d temp_unlocked=%d\n",
             (unsigned)S.safety_level, (unsigned)S.pin_length, (int)S.restriction_enabled, (int)S.temp_unlocked);
}

// ---------------------------------------------------------------- time
void time_clock_snapshot(TimeSnapshot *o)
{
    memset(o, 0, sizeof(*o));
    u64 now = (u64)time(NULL);
    o->user_time = now + S.clock_offset;
    o->network_time = now + S.clock_offset;
    o->local_time = now;
    o->automatic = true;
    o->accuracy = S.clock_offset != 0 || getenv("PLAYGUARD_SIM_ACCURATE");
    snprintf(o->location, sizeof(o->location), "Europe/Paris");
}
void time_clock_apply(u64 utc, TimeApply *o)
{
    memset(o, 0, sizeof(*o));
    time_clock_snapshot(&o->before);
    if (s_read_only) {
        o->open_rc = NXM_RC_READ_ONLY;
    } else {
        S.clock_offset = (s64)utc - (s64)time(NULL);
        if (!S.clock_offset) S.clock_offset = 1;
        o->write_attempted = o->verify_attempted = o->verified = true;
        o->readback = utc;
    }
    time_clock_snapshot(&o->after);
}
void time_clock_dump(char *buf, size_t n) { snprintf(buf, n, "=== System clocks (simulated) ===\n"); }
void time_format_utc(u64 posix, char *buf, size_t size)
{
    time_t t = (time_t)posix; struct tm tmv;
    if (!gmtime_r(&t, &tmv) || !strftime(buf, size, "%Y-%m-%d %H:%M:%S UTC", &tmv)) snprintf(buf, size, "%llu", (unsigned long long)posix);
}
void time_format_local(u64 posix, char *buf, size_t size)
{
    time_t t = (time_t)posix; struct tm tmv;
    if (!localtime_r(&t, &tmv) || !strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tmv)) time_format_utc(posix, buf, size);
}

// ---------------------------------------------------------------- playstats
void playstats_fetch(PlayStats *out)
{
    memset(out, 0, sizeof(*out));
    out->now = (u64)time(NULL);
    if (getenv("PLAYGUARD_SIM_NO_PDM")) {
        out->stats_rc = out->events_rc = (Result)0x1A0C;
        return;
    }
    // Made-up games; the last one was deleted since (no name, no totals).
    static const struct {
        u64 id; const char *name; u32 total_min, launches, today_min, week_min, days_ago;
    } games[] = {
        { 0x0100A1B2C3D40000ULL, "Star Kart Racers",      3650, 300, 45, 310, 0 },
        { 0x0100A1B2C3D41000ULL, "Island Builders",       2980, 160, 20, 140, 0 },
        { 0x0100A1B2C3D42000ULL, "Pixel Quest Deluxe",    1210,  45,  0,  95, 2 },
        { 0x0100A1B2C3D43000ULL, "Dragon Valley Legends", 6100, 120,  0,   0, 12 },
        { 0x0100A1B2C3D44000ULL, "Puzzle Garden",          380,  52,  0,  30, 5 },
        { 0x0100A1B2C3D45000ULL, "",                          0,   0, 15,  15, 0 },
    };
    out->windows_ok = true;
    for (size_t i = 0; i < sizeof(games) / sizeof(games[0]); i++) {
        GameStat *g = &out->games[out->count++];
        g->app_id = games[i].id;
        snprintf(g->name, sizeof(g->name), "%s", games[i].name);
        g->totals_ok = games[i].total_min > 0;
        g->total_s = (u64)games[i].total_min * 60;
        g->launches = games[i].launches;
        g->last_played = g->totals_ok ? out->now - (u64)games[i].days_ago * 86400 - 3600 : 0;
        g->first_played = g->totals_ok ? out->now - 400ULL * 86400 : 0;
        g->today_s = games[i].today_min * 60;
        g->week_s = games[i].week_min * 60;
    }
}
