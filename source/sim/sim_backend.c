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
//   PLAYGUARD_SIM_NOT_SET_UP=1  parental controls never set up (no PIN, no restriction)
//   PLAYGUARD_SIM_APPLET=1      started from the album (applet mode)
//   PLAYGUARD_SIM_RESTRICTED=1  today's limit is reached (the game is suspended)
//   PLAYGUARD_SIM_AUTOSYNC_OFF=1 "Synchronise clock via Internet" is off
//   PLAYGUARD_SIM_NOW=1791471600  the console's time, frozen (POSIX seconds):
//                               the same screens at every run (visual check)
//   PLAYGUARD_SIM_IDLE=1        no game running: the timer reports no time left
//                               yet (the Overview falls back on the activity log)
//   PLAYGUARD_SIM_FAIL=a,b,...  make these fail: unlock (1201), unverified
//                               (1201 fine, 1006 still false), write (every
//                               setting write), relock (1007), timer (145601
//                               read), clock (network clock write), pin (1208),
//                               pin_entry (the PIN screen is cancelled)
// The play-timer limits are kept as the real 0x44 block (core/pure.c encodes
// and decodes it, as on the console); the read-only switch is core/write_guard.c.
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
#include "../core/pure.h"
#include "../core/time_ops.h"
#include "../core/write_guard.h"
#include "sim_icons.h"

static struct {
    bool init;
    u32  hos;
    u32  safety_level;
    u32  pin_length;
    bool restriction_enabled, temp_unlocked, pairing_active, stereo_restricted, alarm_disabled, paused;
    bool limit_reached, autosync_off;
    u32  rating_org;
    PctlCustomSettings custom;
    u16  block[PT_U16_COUNT];   // PlayTimerSettings, as 145601 returns it
    s64  clock_offset;
} S;

// Minutes played today in the made-up activity log below (45 + 20 + 15): the
// timer's time left is the day's limit minus this.
#define SIM_PLAYED_TODAY_MIN 80

// "Now" before any network-clock change: frozen by PLAYGUARD_SIM_NOW, else
// the host's.
static u64 base_now(void)
{
    const char *frozen = getenv("PLAYGUARD_SIM_NOW");
    if (frozen && *frozen) {
        char *end = NULL;
        const unsigned long long v = strtoull(frozen, &end, 10);
        if (end && !*end && v) return (u64)v;
    }
    return (u64)time(NULL);
}

// A service error the UI does not translate (pctl module 142), as the console
// would return for a refused command.
#define SIM_FAIL_RC ((Result)(142 | (100 << 9)))

// True when PLAYGUARD_SIM_FAIL lists `what` (comma separated).
static bool fails(const char *what)
{
    const char *list = getenv("PLAYGUARD_SIM_FAIL");
    const size_t n = strlen(what);
    for (const char *p = list; p && *p;) {
        const char *end = strchr(p, ',');
        const size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len == n && strncmp(p, what, n) == 0) return true;
        p = end ? end + 1 : NULL;
    }
    return false;
}

static void sim_init(void)
{
    if (S.init) return;
    S.init = true;
    S.hos = MAKEHOSVERSION(23, 0, 1);
    const char *fw = getenv("PLAYGUARD_SIM_FW");
    unsigned a, b, c;
    if (fw && sscanf(fw, "%u.%u.%u", &a, &b, &c) == 3) S.hos = MAKEHOSVERSION(a, b, c);
    const bool blank = getenv("PLAYGUARD_SIM_NOT_SET_UP") != NULL;
    S.safety_level = blank ? PctlSafetyLevel_None : PctlSafetyLevel_Child;
    S.pin_length = blank ? 0 : 4;
    S.restriction_enabled = !blank;
    S.pairing_active = !blank && getenv("PLAYGUARD_SIM_UNPAIRED") == NULL;
    S.temp_unlocked = getenv("PLAYGUARD_SIM_UNLOCKED") != NULL;
    S.custom.rating_age = 12;
    S.rating_org = 6;   // PEGI
    S.custom.sns_post_restriction = true;
    S.limit_reached = getenv("PLAYGUARD_SIM_RESTRICTED") != NULL;
    S.autosync_off = getenv("PLAYGUARD_SIM_AUTOSYNC_OFF") != NULL;
    S.alarm_disabled = getenv("PLAYGUARD_SIM_ALARM_OFF") != NULL;
    bool off = blank || getenv("PLAYGUARD_SIM_TIMER_OFF") != NULL;
    u16 days[7];
    for (int i = 0; i < 7; i++) days[i] = off ? PT_DAY_NOLIMIT : ((i == 0 || i == 6) ? 180 : 120);
    pt_encode(S.block, days);
}

// The header is non-zero while any day has a limit (pure.h).
static bool timer_enabled(void)
{
    return !S.temp_unlocked && S.block[0] != 0;
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
    out->applet_mode = getenv("PLAYGUARD_SIM_APPLET") != NULL;
    out->serial_valid = true;
    snprintf(out->serial, sizeof(out->serial), "%s", out->blank ? SYSINFO_BLANK_SERIAL : "XAW10000000001");
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
    o->rating_org_ok = true;          o->rating_org = S.rating_org;
    o->stereo_vision_ok = true;       o->stereo_vision_restricted = S.stereo_restricted;
    o->free_comm_count_ok = true;     o->free_comm_count = 3;
    o->last_updated_ok = true;        o->last_updated = base_now() - 3600 * 26;
}

// Read-only, then the change check (write_guard.h), as pctl_ops.c does.
#define RO_GUARD() do { Result g_ = core_change_allowed(); if (R_FAILED(g_)) return g_; } while (0)
// Refuses like the console when PLAYGUARD_SIM_FAIL lists `what`.
#define FAIL_IF(what) do { if (fails(what)) return SIM_FAIL_RC; } while (0)

Result pctl_set_pin(void)                         { RO_GUARD(); S.pin_length = 4; return 0; }
Result pctl_unlock_restriction_temporarily(void)
{
    RO_GUARD();
    FAIL_IF("unlock");
    if (!S.pin_length) return 0x1A08E;
    if (fails("unverified")) return NXM_RC_UNLOCK_NOT_EFFECTIVE;
    S.temp_unlocked = true;
    return 0;
}
Result pctl_get_pin(char *out, size_t out_size)
{
    if (out && out_size) memset(out, 0, out_size);
    RO_GUARD();
    FAIL_IF("pin");
    if (!out || out_size < 5) return NXM_RC_INVALID_ARGUMENT;
    if (!S.pin_length) return NXM_RC_STATE_UNKNOWN;
    snprintf(out, out_size, "1234");
    return 0;
}
// Locking again never asks for the PIN (write_guard.h): read-only only.
Result pctl_lock_state(u32 *pin_length, bool *unlocked)
{
    Result rc = pctl_ops_init();
    if (R_FAILED(rc)) return rc;
    if (pin_length) *pin_length = S.pin_length;
    if (unlocked) *unlocked = S.temp_unlocked;
    return 0;
}
Result pctl_relock(void)                          { if (core_read_only()) return NXM_RC_READ_ONLY; FAIL_IF("relock"); S.temp_unlocked = false; return 0; }
// PLAYGUARD_SIM_FAIL=pin_entry: the PIN screen is cancelled.
Result pctl_ask_pin(void)                         { sim_init(); if (!S.pin_length) return NXM_RC_NO_PIN; FAIL_IF("pin_entry"); return 0; }
Result pctl_delete_parental_controls(void)
{
    RO_GUARD();
    FAIL_IF("write");
    S.pin_length = 0; S.restriction_enabled = false; S.safety_level = 0; S.temp_unlocked = false;
    memset(S.block, 0, sizeof(S.block));
    return 0;
}
Result pctl_delete_pairing(void)                  { RO_GUARD(); FAIL_IF("write"); S.pairing_active = false; return 0; }
Result pctl_set_safety_level(u32 l)               { RO_GUARD(); FAIL_IF("write"); if (l > 4) return NXM_RC_INVALID_ARGUMENT; S.safety_level = l; return 0; }
Result pctl_set_custom_settings(const PctlCustomSettings *c)
{
    RO_GUARD();
    FAIL_IF("write");
    if (S.safety_level != PctlSafetyLevel_Custom) return NXM_RC_NOT_CUSTOM;
    S.custom = *c;
    return 0;
}
Result pctl_set_stereo_vision_restricted(bool r)  { RO_GUARD(); FAIL_IF("write"); S.stereo_restricted = r; return 0; }
Result pctl_set_rating_org(u32 org)               { RO_GUARD(); FAIL_IF("write"); if (org >= 13) return NXM_RC_INVALID_ARGUMENT; S.rating_org = org; return 0; }
Result pctl_get_level_settings(u32 level, PctlCustomSettings *out)
{
    // Made-up presets in the spirit of the console's.
    static const PctlCustomSettings presets[5] = { { 0, false, false }, { 0, false, false },
                                                   { 6, true, true }, { 12, true, true }, { 16, false, false } };
    if (!out || level > 4) return NXM_RC_INVALID_ARGUMENT;
    *out = level == PctlSafetyLevel_Custom ? S.custom : presets[level];
    return 0;
}
// The same gate as pctl_ops.c: no play-timer write while it counts down.
#define PT_GATE() do { if ((timer_enabled() || S.limit_reached) && !S.temp_unlocked) return NXM_RC_WRITE_GATED; } while (0)
Result pctl_play_timer_set_alarm_disabled(bool d) { RO_GUARD(); PT_GATE(); FAIL_IF("write"); S.alarm_disabled = d; return 0; }
Result pctl_play_timer_start(void)                { RO_GUARD(); PT_GATE(); FAIL_IF("write"); S.paused = false; return 0; }
Result pctl_play_timer_stop(void)                 { RO_GUARD(); PT_GATE(); FAIL_IF("write"); S.paused = true; return 0; }

void pctl_play_timer_query(PtState *o)
{
    sim_init();
    memset(o, 0, sizeof(*o));
    for (int i = 0; i < 7; i++) o->day_min[i] = PT_DAY_NOLIMIT;
    o->fw_supported = S.hos >= PCTL_FW_MIN_PLAYTIMER;
    if (!o->fw_supported) return;
    if (R_FAILED(o->session_rc = pctl_ops_init())) return;
    o->session_valid = true;
    if (fails("timer")) {
        o->config_rc = SIM_FAIL_RC;
    } else {
        o->valid = true;
        pt_decode(S.block, o->day_min);
        memcpy(o->block, S.block, sizeof(o->block));
    }
    o->enabled_valid = true;  o->enabled = timer_enabled();
    o->temporary_unlocked_valid = true; o->temporary_unlocked = S.temp_unlocked;
    // Today's limit against what the made-up log says was played today.
    LocalTime today;
    const int wd = time_local_now(NULL, &today) ? today.wday : 0;
    u16 days[7];
    pt_decode(S.block, days);
    const u16 limit = days[wd];
    const bool reached = o->enabled && (S.limit_reached || (limit != PT_DAY_NOLIMIT && limit <= SIM_PLAYED_TODAY_MIN));
    o->remaining_valid = true;
    o->remaining_ns = o->enabled && !reached && limit != PT_DAY_NOLIMIT && !getenv("PLAYGUARD_SIM_IDLE")
                          ? (u64)(limit - SIM_PLAYED_TODAY_MIN) * 60 * 1000000000ULL : 0;
    o->restricted_valid = true; o->restricted = reached;
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
    if ((timer_enabled() || S.limit_reached) && !S.temp_unlocked) return NXM_RC_WRITE_GATED;
    FAIL_IF("write");
    pt_encode(S.block, d);   // read-modify-write, as pctl_ops.c does
    S.limit_reached = false;
    return 0;
}
Result pctl_play_timer_set_uniform(u16 m) { u16 d[7]; for (int i = 0; i < 7; i++) d[i] = m; return pctl_play_timer_set_days(d); }
Result pctl_play_timer_clear(void)        { u16 d[7]; for (int i = 0; i < 7; i++) d[i] = PT_DAY_NOLIMIT; return pctl_play_timer_set_days(d); }

void pctl_dump(char *buf, size_t n)
{
    int used = snprintf(buf, n, "=== pctl state (simulated desktop backend) ===\n"
                        "safety_level=%u pin_length=%u enabled=%d temp_unlocked=%d\n145601 block:",
                        (unsigned)S.safety_level, (unsigned)S.pin_length, (int)S.restriction_enabled, (int)S.temp_unlocked);
    for (int i = 0; i < PT_U16_COUNT && used > 0 && (size_t)used < n; i++)
        used += snprintf(buf + used, n - (size_t)used, " %04X", S.block[i]);
    if (used > 0 && (size_t)used < n) snprintf(buf + used, n - (size_t)used, "\n");
}

// ---------------------------------------------------------------- time
void pctl_overview_fetch(PctlStatus *s, PtState *pt)
{
    pctl_status_fetch(s);
    pctl_play_timer_query(pt);
}

Result time_network_accuracy(bool *accurate)
{
    TimeSnapshot s;
    time_clock_snapshot(&s);
    *accurate = s.accuracy;
    return s.accuracy_rc;
}

void time_clock_snapshot(TimeSnapshot *o)
{
    memset(o, 0, sizeof(*o));
    u64 now = base_now();
    o->user_time = now + S.clock_offset;
    o->network_time = now + S.clock_offset;
    o->local_time = now;
    o->automatic = !S.autosync_off;
    o->accuracy = S.clock_offset != 0 || getenv("PLAYGUARD_SIM_ACCURATE");
    snprintf(o->location, sizeof(o->location), "Europe/Paris");
}
void time_clock_apply(u64 utc, TimeApply *o)
{
    memset(o, 0, sizeof(*o));
    time_clock_snapshot(&o->before);
    const Result gate = core_change_allowed();
    if (R_FAILED(gate)) {
        o->open_rc = gate;
    } else if (!o->before.automatic) {
        o->refused_automatic = true;   // as time_ops.c: the user clock would not follow
    } else if (fails("clock")) {
        o->write_attempted = true;
        o->write_rc = SIM_FAIL_RC;
    } else {
        S.clock_offset = (s64)utc - (s64)base_now();
        if (!S.clock_offset) S.clock_offset = 1;
        o->write_attempted = o->verify_attempted = o->verified = true;
        o->readback = utc;
    }
    time_clock_snapshot(&o->after);
}
void time_clock_dump(char *buf, size_t n) { snprintf(buf, n, "=== System clocks (simulated) ===\n"); }

// The host's time zone stands in for the console's (TZ= changes it).
static bool host_to_local(void *ctx, u64 posix, LocalTime *out)
{
    (void)ctx;
    time_t t = (time_t)posix;
    struct tm tmv;
    if (!localtime_r(&t, &tmv)) return false;
    out->year = (u16)(tmv.tm_year + 1900); out->month = (u8)(tmv.tm_mon + 1); out->day = (u8)tmv.tm_mday;
    out->hour = (u8)tmv.tm_hour; out->minute = (u8)tmv.tm_min; out->second = (u8)tmv.tm_sec;
    out->wday = (u8)tmv.tm_wday;
    return true;
}
static int host_to_posix(void *ctx, const LocalTime *w, u64 out[2])
{
    (void)ctx;
    int n = 0;
    for (int dst = 0; dst <= 1; dst++) {   // both readings of a repeated hour
        struct tm tmv;
        memset(&tmv, 0, sizeof(tmv));
        tmv.tm_year = w->year - 1900; tmv.tm_mon = w->month - 1; tmv.tm_mday = w->day;
        tmv.tm_hour = w->hour; tmv.tm_min = w->minute; tmv.tm_sec = w->second;
        tmv.tm_isdst = dst;
        const time_t t = mktime(&tmv);
        LocalTime back;
        if (t == (time_t)-1 || !host_to_local(NULL, (u64)t, &back)) continue;
        if (back.day != w->day || back.hour != w->hour || back.minute != w->minute) continue;   // in a gap
        if (n == 0 || out[0] != (u64)t) out[n++] = (u64)t;
    }
    if (n == 2 && out[1] < out[0]) { u64 x = out[0]; out[0] = out[1]; out[1] = x; }
    return n;
}
const TimeRule *time_console_rule(void)
{
    static const TimeRule rule = { host_to_local, host_to_posix, NULL };
    return &rule;
}
bool time_local_now(u64 *posix, LocalTime *local)
{
    const u64 now = (u64)((s64)base_now() + S.clock_offset);   // the user clock, as the snapshot says
    if (posix) *posix = now;
    return local ? host_to_local(NULL, now, local) : true;
}
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
// Two made-up accounts: Alice played 2/3 of everything, Léo 1/3.
static const PlayAccount SIM_ACCOUNTS[] = { { { 1, 1 }, "Alice" }, { { 2, 2 }, "Léo" } };

size_t playstats_accounts(PlayAccount *out, size_t max, Result *rc)
{
    if (rc) *rc = 0;
    size_t n = 0;
    for (; n < max && n < 2; n++) out[n] = SIM_ACCOUNTS[n];
    return n;
}

// The share of `v` that `account` played (NULL: all of it).
static u64 share(u64 v, const PlayAccount *account)
{
    if (!account) return v;
    return account->uid[0] == 1 ? v * 2 / 3 : v - v * 2 / 3;
}

void playstats_fetch(PlayStats *out)
{
    playstats_fetch_for(out, NULL);
}

void playstats_fetch_for(PlayStats *out, const PlayAccount *account)
{
    memset(out, 0, sizeof(*out));
    time_local_now(&out->now, NULL);
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
    LocalTime today;
    const int wday = host_to_local(NULL, out->now, &today) ? today.wday : 0;
    for (int k = 0; k < 7; k++) out->day_wday[k] = (u8)((wday - k + 7) % 7);
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
        // The rest of the week spread over the six days before, unevenly.
        static const u32 spread[6] = { 30, 0, 25, 15, 0, 30 };
        g->day_s[0] = g->today_s;
        const u32 rest = g->week_s - g->today_s;
        u32 given = 0;
        for (int k = 1; k < 7; k++) {
            g->day_s[k] = k < 6 ? rest * spread[k - 1] / 100 : rest - given;
            given += g->day_s[k];
        }
        // One account: its share of every figure; a game it never played goes.
        g->total_s  = share(g->total_s, account);
        g->launches = (u32)share(g->launches, account);
        g->today_s  = (u32)share(g->today_s, account);
        g->week_s   = (u32)share(g->week_s, account);
        for (int k = 0; k < 7; k++) g->day_s[k] = (u32)share(g->day_s[k], account);
        if (account && !g->week_s && !g->total_s) out->count--;
    }
}

void playstats_icons(PlayIcon *icons, size_t count)
{
    // The made-up games above, in order; the deleted one has no icon.
    for (size_t i = 0; i < count; i++) {
        icons[i].jpeg = NULL;
        icons[i].size = 0;
        const u64 base = 0x0100A1B2C3D40000ULL;
        if (icons[i].app_id < base || (icons[i].app_id - base) % 0x1000 || (icons[i].app_id - base) / 0x1000 >= 5) continue;
        const size_t k = (size_t)((icons[i].app_id - base) / 0x1000);
        icons[i].jpeg = (unsigned char *)malloc(SIM_ICON_SIZES[k]);
        if (!icons[i].jpeg) continue;
        memcpy(icons[i].jpeg, SIM_ICONS[k], SIM_ICON_SIZES[k]);
        icons[i].size = SIM_ICON_SIZES[k];
    }
}

size_t playstats_by_account(u64 app_id, AccountPlay *out, size_t max, Result *rc)
{
    if (rc) *rc = getenv("PLAYGUARD_SIM_NO_PDM") ? (Result)0x1A0C : 0;
    if (getenv("PLAYGUARD_SIM_NO_PDM") || max < 2) return 0;
    // Two made-up accounts sharing the play time 2:1 (none for a deleted game).
    PlayStats all;
    playstats_fetch(&all);
    for (u32 i = 0; i < all.count; i++) {
        if (all.games[i].app_id != app_id || !all.games[i].totals_ok) continue;
        const u64 t = all.games[i].total_s;
        snprintf(out[0].nickname, sizeof(out[0].nickname), "Alice");
        out[0].total_s = t * 2 / 3;
        out[0].launches = all.games[i].launches * 2 / 3;
        snprintf(out[1].nickname, sizeof(out[1].nickname), "Léo");
        out[1].total_s = t - out[0].total_s;
        out[1].launches = all.games[i].launches - out[0].launches;
        return 2;
    }
    return 0;
}
