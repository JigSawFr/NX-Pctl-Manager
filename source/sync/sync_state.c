// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "sync_state.h"

#include <stdio.h>
#include <string.h>

#include "sync_json.h"

#define NOLIMIT PT_DAY_NOLIMIT

static const char *const DAY_KEYS[7] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat" };

int sync_played_today_min(const PtState *pt, int weekday)
{
    if (!pt->valid || !pt->enabled_valid || !pt->enabled || weekday < 0 || weekday > 6) return -1;
    const uint16_t limit = pt->day_min[weekday];
    if (limit == PT_DAY_NOLIMIT) return -1;
    if (pt->restricted_valid && pt->restricted) return limit;
    if (!pt->remaining_valid || pt->remaining_ns == 0) return -1;
    const uint64_t left = pt->remaining_ns / 60000000000ULL;
    return left >= limit ? 0 : (int)(limit - left);
}

const char *sync_level_name(uint32_t level)
{
    static const char *const names[5] = { "none", "custom", "young_child", "child", "teen" };
    return level < 5 ? names[level] : "";
}

// A limit on the wire: 1440 for "no limit".
static int64_t wire_limit(uint16_t v)
{
    return v == NOLIMIT ? 1440 : v;
}

static void hhmm(SyncJson *j, const char *key, bool ok, uint8_t hour, uint8_t minute)
{
    sync_json_key(j, key);
    if (!ok) {
        sync_json_null(j);
        return;
    }
    char t[12];
    snprintf(t, sizeof(t), "%02u:%02u:00", hour % 24u, minute % 60u);
    sync_json_str(j, t);
}

static void version(SyncJson *j, const char *key, const SysInfo *sys, bool ams)
{
    sync_json_key(j, key);
    char v[24];
    if (!sys || (ams && !sys->ams_valid) || (!ams && !sys->hos_version)) {
        sync_json_null(j);
        return;
    }
    if (ams) snprintf(v, sizeof(v), "%u.%u.%u", sys->ams_major, sys->ams_minor, sys->ams_micro);
    else snprintf(v, sizeof(v), "%u.%u.%u", (unsigned)((sys->hos_version >> 16) & 0xFF),
                  (unsigned)((sys->hos_version >> 8) & 0xFF), (unsigned)(sys->hos_version & 0xFF));
    sync_json_str(j, v);
}

static void console(SyncJson *j, const SyncSnapshot *s)
{
    sync_json_key(j, "console");
    sync_json_obj(j);
    sync_json_kstr_or_null(j, "id", s->conf ? s->conf->console_id : NULL);
    sync_json_kstr_or_null(j, "name", s->conf ? s->conf->console_name : NULL);
    version(j, "firmware", s->sys, false);
    version(j, "atmosphere", s->sys, true);
    sync_json_kstr_or_null(j, "app_version", s->app_version);
    sync_json_kstr_or_null(j, "agent_version", s->agent_version);
    sync_json_kbool_or_null(j, "emummc", s->sys && s->sys->emummc_valid, s->sys && s->sys->emummc);
    sync_json_kbool(j, "read_only", s->read_only);
    sync_json_obj_end(j);
}

static void controls(SyncJson *j, const SyncSnapshot *s)
{
    const PctlStatus *st = s->status;
    sync_json_key(j, "controls");
    sync_json_obj(j);
    sync_json_kbool_or_null(j, "enabled", st && st->restriction_enabled_ok, st && st->restriction_enabled);
    sync_json_kbool_or_null(j, "temp_unlocked", st && st->temp_unlocked_ok, st && st->temp_unlocked);
    sync_json_kbool_or_null(j, "pin_set", st && st->pin_length_ok, st && st->pin_length > 0);
    sync_json_key(j, "level");
    if (st && st->safety_level_ok && *sync_level_name(st->safety_level)) sync_json_str(j, sync_level_name(st->safety_level));
    else sync_json_null(j);
    sync_json_kint_or_null(j, "rating_age", st && st->settings_ok, st ? st->settings.rating_age : 0);
    sync_json_kbool_or_null(j, "sns_post_restricted", st && st->settings_ok, st && st->settings.sns_post_restriction);
    sync_json_kbool_or_null(j, "free_communication_restricted", st && st->settings_ok,
                            st && st->settings.free_communication_restriction);
    sync_json_kbool_or_null(j, "vr_restricted", st && st->stereo_vision_ok, st && st->stereo_vision_restricted);
    sync_json_kint_or_null(j, "rating_org", st && st->rating_org_ok, st ? st->rating_org : 0);
    sync_json_kbool_or_null(j, "companion_linked", st && st->pairing_active_ok, st && st->pairing_active);
    sync_json_obj_end(j);
}

static void timer(SyncJson *j, const SyncSnapshot *s)
{
    const PtState *pt = s->timer;
    const bool valid = pt && pt->fw_supported && pt->valid;
    const int wd = s->weekday;
    const bool today_ok = valid && wd >= 0 && wd <= 6;
    const bool enabled_ok = pt && pt->fw_supported && pt->enabled_valid;

    sync_json_key(j, "timer");
    sync_json_obj(j);
    sync_json_kbool(j, "supported", pt ? pt->fw_supported : false);
    sync_json_kbool_or_null(j, "enabled", enabled_ok, enabled_ok && pt->enabled);
    sync_json_kbool_or_null(j, "limit_reached", pt && pt->fw_supported && pt->restricted_valid,
                            pt && pt->restricted);

    sync_json_key(j, "limits");
    sync_json_obj(j);
    for (int d = 0; d < 7; d++) sync_json_kint_or_null(j, DAY_KEYS[d], valid, valid ? wire_limit(pt->day_min[d]) : 0);
    sync_json_obj_end(j);

    sync_json_key(j, "limits_min");
    if (valid) {
        sync_json_arr(j);
        for (int d = 0; d < 7; d++) sync_json_int(j, wire_limit(pt->day_min[d]));
        sync_json_arr_end(j);
    } else {
        sync_json_null(j);
    }

    bool uniform = valid;
    for (int d = 1; d < 7 && uniform; d++) uniform = pt->day_min[d] == pt->day_min[0];
    sync_json_kint_or_null(j, "uniform_min", uniform, uniform ? wire_limit(pt->day_min[0]) : 0);

    const uint16_t limit = today_ok ? pt->day_min[wd] : NOLIMIT;
    sync_json_kint_or_null(j, "limit_today_min", today_ok, wire_limit(limit));

    // Time left today: the console's count, or the whole limit while no game
    // has been counted yet (1454 reads 0 until then).
    bool left_ok = false;
    int64_t left = 0;
    if (today_ok && enabled_ok && pt->enabled && limit != NOLIMIT) {
        if (pt->restricted_valid && pt->restricted) {
            left_ok = true;
            left = 0;
        } else if (pt->remaining_valid) {
            left_ok = true;
            left = pt->remaining_ns ? (int64_t)((pt->remaining_ns + 30000000000ULL) / 60000000000ULL) : limit;
        }
    }
    sync_json_kint_or_null(j, "remaining_min", left_ok, left);
    const int played = pt ? sync_played_today_min(pt, wd) : -1;
    sync_json_kint_or_null(j, "used_min", played >= 0, played);
    sync_json_kint_or_null(j, "spent_raw_min", s->spent_ok, (int64_t)(s->spent_ns / 60000000000ULL));
    sync_json_kbool_or_null(j, "alarm_on", pt && pt->fw_supported && pt->alarm_disabled_valid,
                            pt && !pt->alarm_disabled);

    sync_json_key(j, "bedtime");
    sync_json_obj(j);
    const bool bed_ok = pt && pt->fw_supported && pt->bedtime_valid;
    sync_json_kbool_or_null(j, "enabled", bed_ok, bed_ok && pt->bedtime_enabled);
    hhmm(j, "start", bed_ok && pt->bedtime_enabled, bed_ok ? pt->bedtime_hour : 0, bed_ok ? pt->bedtime_minute : 0);
    // When play is allowed again: the console's own answer (20.0.0+), else
    // what the block holds for today.
    if (pt && pt->fw_supported && pt->bedtime_reset_valid) {
        hhmm(j, "end", true, pt->bedtime_reset_hour, pt->bedtime_reset_minute);
    } else if (today_ok && pt->bed[wd].on) {
        hhmm(j, "end", true, pt->bed[wd].end_hour, pt->bed[wd].end_minute);
    } else {
        sync_json_knull(j, "end");
    }
    sync_json_obj_end(j);

    // Extra time granted today and still on today's limit.
    int64_t extended = 0;
    const SyncRecords *r = s->records;
    if (r && today_ok && r->extra_weekday == wd && s->local_date && !strcmp(r->extra_date, s->local_date) &&
        pt->day_min[wd] == r->extra_value && r->extra_value > r->extra_base && r->extra_base != NOLIMIT)
        extended = r->extra_value - r->extra_base;
    sync_json_kint_or_null(j, "extended_today_min", today_ok, extended);
    sync_json_kbool(j, "console_locked", r && r->console_lock);
    sync_json_obj_end(j);
}

static void activity_today(SyncJson *j, const SyncSnapshot *s)
{
    sync_json_key(j, "activity_today");
    sync_json_obj(j);
    sync_json_kint_or_null(j, "used_min", s->activity_ok, (s->activity_s + 30) / 60);
    sync_json_key(j, "now_playing");
    sync_json_obj(j);
    sync_json_key(j, "app_id");
    if (s->now_playing) sync_json_hex64(j, s->now_playing);
    else sync_json_null(j);
    sync_json_kstr_or_null(j, "name", s->now_playing ? s->now_playing_name : NULL);
    sync_json_kint_or_null(j, "since", s->now_playing && s->now_playing_since, (int64_t)s->now_playing_since);
    sync_json_obj_end(j);
    sync_json_obj_end(j);
}

static void link(SyncJson *j, const SyncSnapshot *s)
{
    sync_json_key(j, "link");
    sync_json_obj(j);
    sync_json_kstr(j, "policy", sync_policy_name(s->conf ? s->conf->policy : SyncPolicy_Ask));
    sync_json_kbool(j, "remote_timer_writes", s->conf && s->conf->remote_timer_writes);
    sync_json_kbool(j, "ha_discovery", s->conf && s->conf->ha_discovery);
    sync_json_kbool(j, "agent", s->agent);
    sync_json_kstr_or_null(j, "last_result", s->last_result);
    sync_json_obj_end(j);
}

size_t sync_state_build(const SyncSnapshot *s, char *out, size_t cap)
{
    SyncJson j;
    sync_json_init(&j, out, cap);
    sync_json_obj(&j);
    sync_json_kint(&j, "schema", SYNC_SCHEMA);
    sync_json_kstr(&j, "source", s->source ? s->source : "app");
    sync_json_kint(&j, "ts", (int64_t)s->ts);
    sync_json_kstr_or_null(&j, "local_date", s->local_date);
    sync_json_kint_or_null(&j, "weekday", s->weekday >= 0 && s->weekday <= 6, s->weekday);
    sync_json_kbool_or_null(&j, "clock_accurate", s->clock_accurate_ok, s->clock_accurate);
    console(&j, s);
    controls(&j, s);
    timer(&j, s);
    activity_today(&j, s);
    link(&j, s);
    sync_json_obj_end(&j);
    return sync_json_end(&j);
}

static void uid_hex(SyncJson *j, const uint64_t uid[2])
{
    char t[33];
    snprintf(t, sizeof(t), "%016llX%016llX", (unsigned long long)uid[0], (unsigned long long)uid[1]);
    sync_json_str(j, t);
}

size_t sync_activity_build(const SyncActivity *a, char *out, size_t cap)
{
    SyncJson j;
    sync_json_init(&j, out, cap);
    sync_json_obj(&j);
    sync_json_kint(&j, "schema", SYNC_SCHEMA);
    sync_json_kstr(&j, "source", a->source ? a->source : "app");
    sync_json_kint(&j, "ts", (int64_t)a->ts);
    sync_json_kstr_or_null(&j, "local_date", a->local_date);
    sync_json_kbool(&j, "final", a->final);
    uint64_t total = 0;
    for (size_t i = 0; i < a->n_apps; i++) total += a->apps[i].seconds;
    sync_json_kint(&j, "total_s", (int64_t)total);
    sync_json_kint(&j, "total_min", (int64_t)((total + 30) / 60));
    sync_json_key(&j, "per_app");
    sync_json_arr(&j);
    for (size_t i = 0; i < a->n_apps; i++) {
        if (!a->apps[i].seconds) continue;
        sync_json_obj(&j);
        sync_json_key(&j, "app_id");
        sync_json_hex64(&j, a->apps[i].app_id);
        sync_json_kint(&j, "s", a->apps[i].seconds);
        sync_json_kint(&j, "min", (a->apps[i].seconds + 30) / 60);
        sync_json_obj_end(&j);
    }
    sync_json_arr_end(&j);
    sync_json_key(&j, "per_account");
    if (a->accounts) {
        sync_json_arr(&j);
        for (size_t i = 0; i < a->n_accounts; i++) {
            sync_json_obj(&j);
            sync_json_key(&j, "uid");
            uid_hex(&j, a->accounts[i].uid);
            sync_json_kint(&j, "s", a->accounts[i].seconds);
            sync_json_kint(&j, "min", (a->accounts[i].seconds + 30) / 60);
            sync_json_obj_end(&j);
        }
        sync_json_arr_end(&j);
    } else {
        sync_json_null(&j);
    }
    if (!a->final) {
        sync_json_key(&j, "now_playing");
        sync_json_obj(&j);
        sync_json_key(&j, "app_id");
        if (a->now_playing) sync_json_hex64(&j, a->now_playing);
        else sync_json_null(&j);
        sync_json_kint_or_null(&j, "since", a->now_playing && a->now_playing_since, (int64_t)a->now_playing_since);
        sync_json_obj_end(&j);
    }
    sync_json_obj_end(&j);
    return sync_json_end(&j);
}

size_t sync_names_build(uint64_t ts, const SyncAppName *apps, size_t n_apps, const SyncAccountName *accounts,
                        size_t n_accounts, char *out, size_t cap)
{
    SyncJson j;
    sync_json_init(&j, out, cap);
    sync_json_obj(&j);
    sync_json_kint(&j, "schema", SYNC_SCHEMA);
    sync_json_kint(&j, "ts", (int64_t)ts);
    sync_json_key(&j, "games");
    sync_json_obj(&j);
    for (size_t i = 0; i < n_apps; i++) {
        if (!apps[i].name || !*apps[i].name) continue;
        char key[17];
        snprintf(key, sizeof(key), "%016llX", (unsigned long long)apps[i].app_id);
        sync_json_kstr(&j, key, apps[i].name);
    }
    sync_json_obj_end(&j);
    sync_json_key(&j, "accounts");
    sync_json_obj(&j);
    for (size_t i = 0; i < n_accounts; i++) {
        char key[33];
        snprintf(key, sizeof(key), "%016llX%016llX", (unsigned long long)accounts[i].uid[0],
                 (unsigned long long)accounts[i].uid[1]);
        sync_json_kstr(&j, key, accounts[i].nickname ? accounts[i].nickname : "");
    }
    sync_json_obj_end(&j);
    sync_json_obj_end(&j);
    return sync_json_end(&j);
}

size_t sync_event_build(const char *source, uint64_t ts, const char *event_type, const char *entity,
                        const char *payload, SyncReason reason, bool rc_set, uint32_t rc, char *out, size_t cap)
{
    SyncJson j;
    sync_json_init(&j, out, cap);
    sync_json_obj(&j);
    sync_json_kint(&j, "schema", SYNC_SCHEMA);
    sync_json_kstr(&j, "source", source ? source : "app");
    sync_json_kint(&j, "ts", (int64_t)ts);
    sync_json_kstr(&j, "event_type", event_type);
    sync_json_kstr_or_null(&j, "entity", entity);
    sync_json_kstr_or_null(&j, "payload", payload);
    sync_json_kstr_or_null(&j, "reason", reason == SyncReason_None ? NULL : sync_reason_name(reason));
    sync_json_key(&j, "rc");
    if (rc_set) {
        char t[12];
        snprintf(t, sizeof(t), "0x%08X", (unsigned)rc);
        sync_json_str(&j, t);
    } else {
        sync_json_null(&j);
    }
    sync_json_obj_end(&j);
    return sync_json_end(&j);
}
