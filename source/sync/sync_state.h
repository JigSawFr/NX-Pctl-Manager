// sync_state — the JSON documents the console publishes: the state
// snapshot (`<prefix>/<id>/state`), today's activity and a finished day's
// totals (`activity`, `activity/<date>`), the names table (`names`) and the
// events (`event`). See docs/sync-protocol.md. Every object of the state is
// always present; only its leaves are null when the console could not say,
// so a Home Assistant template never fails on a missing level.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "sync_conf.h"
#include "sync_exec.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/pctl_ops.h"
#include "../core/sysinfo.h"

#define SYNC_SCHEMA 1

typedef struct {
    const char        *source;          // "agent" or "app"
    uint64_t           ts;              // user clock, POSIX seconds
    const char        *local_date;      // "YYYY-MM-DD", NULL or "" when unknown
    int                weekday;         // 0 = Sunday, -1 unknown
    bool               clock_accurate_ok, clock_accurate;

    const SyncConf    *conf;            // id, name, policy, switches
    const SysInfo     *sys;             // NULL: unknown
    const char        *app_version;     // NULL or "": unknown
    const char        *agent_version;   // NULL or "": no agent
    bool               read_only;

    const PctlStatus  *status;          // NULL: not read
    const PtState     *timer;           // NULL: not read
    bool               spent_ok;        // 1952
    uint64_t           spent_ns;
    const SyncRecords *records;         // NULL: none

    bool               activity_ok;     // today's play-log total
    uint32_t           activity_s;
    uint64_t           now_playing;     // application id, 0: none
    uint64_t           now_playing_since;
    const char        *now_playing_name;

    bool               agent;           // the agent sysmodule runs (and publishes)
    const char        *last_result;     // "limit_mon: applied", NULL or "": none
} SyncSnapshot;

size_t sync_state_build(const SyncSnapshot *s, char *out, size_t cap);

// Time played per application (today's, or a finished day's).
typedef struct {
    uint64_t app_id;
    uint32_t seconds;
} SyncAppTime;

// Time played per user account.
typedef struct {
    uint64_t uid[2];
    uint32_t seconds;
} SyncAccountTime;

typedef struct {
    const char            *source;
    uint64_t               ts;
    const char            *local_date;
    bool                   final;           // a finished day (activity/<date>)
    const SyncAppTime     *apps;
    size_t                 n_apps;
    const SyncAccountTime *accounts;        // NULL: not known (the agent does not read accounts)
    size_t                 n_accounts;
    uint64_t               now_playing;     // not for a finished day
    uint64_t               now_playing_since;
} SyncActivity;

size_t sync_activity_build(const SyncActivity *a, char *out, size_t cap);

typedef struct {
    uint64_t    app_id;
    const char *name;
} SyncAppName;

typedef struct {
    uint64_t    uid[2];
    const char *nickname;
} SyncAccountName;

size_t sync_names_build(uint64_t ts, const SyncAppName *apps, size_t n_apps, const SyncAccountName *accounts,
                        size_t n_accounts, char *out, size_t cap);

// One event (`event_type`, the order's entity and payload, why not, the
// result code), as the `event` topic and HA's event entity take it.
size_t sync_event_build(const char *source, uint64_t ts, const char *event_type, const char *entity,
                        const char *payload, SyncReason reason, bool rc_set, uint32_t rc, char *out, size_t cap);

// Minutes played today by the console timer's own count (today's limit minus
// 1454), or -1 when it does not say (pt_logic::played_today_min).
int sync_played_today_min(const PtState *pt, int weekday);

// "none", "custom", "young_child", "child", "teen", or "" outside them.
const char *sync_level_name(uint32_t level);

#ifdef __cplusplus
}
#endif
