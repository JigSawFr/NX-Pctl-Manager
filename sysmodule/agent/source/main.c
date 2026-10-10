// playguard-agent — keeps PlayGuard's remote link (MQTT, Home Assistant) up
// while PlayGuard is closed: the same session engine as the app
// (source/sync/sync_engine.c), the console read and changed through the same
// service layer (source/core/), orders carried out through the same gate,
// unlock and relock (source/sync/sync_exec.c), and only under the "auto"
// policy; with "ask" they wait for PlayGuard.
//
// While PlayGuard runs it holds a pg:agent session (agent_ipc.c): the agent
// stays the only MQTT client, publishes what PlayGuard pushes, and hands it
// the orders; in the foreground PlayGuard is the only one to read pctl.
//
// Files (sd:/switch/playguard/): sync.conf (the settings, PlayGuard's Sync
// screen), sync/nro_state.txt (what PlayGuard last saved: records, read-only),
// sync/profiles.txt; written here: sync/agent_state.txt (the records of what
// the agent changed), sync/agent_events.log (its orders, imported into
// PlayGuard's history), logs/agent_*.txt (reports asked for).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "agent_core.h"
#include "calendar.h"
#include "pctl_ops.h"
#include "playstats.h"
#include "sync_engine.h"
#include "sync_net.h"
#include "sync_state.h"
#include "sync_tls.h"
#include "sysinfo.h"
#include "time_ops.h"
#include "write_guard.h"

#ifndef AGENT_VERSION
#define AGENT_VERSION "0.0.0"
#endif

#define DATA_DIR     "sdmc:/switch/playguard"
#define CONF_PATH    DATA_DIR "/sync.conf"
#define SYNC_DIR     DATA_DIR "/sync"
#define NRO_STATE    SYNC_DIR "/nro_state.txt"
#define AGENT_STATE  SYNC_DIR "/agent_state.txt"
#define AGENT_EVENTS SYNC_DIR "/agent_events.log"
#define PROFILES     SYNC_DIR "/profiles.txt"
#define NAMES        SYNC_DIR "/names.txt"
#define LOGS_DIR     DATA_DIR "/logs"

#define ACTIVITY_EVERY_MS (5 * 60 * 1000ULL)
#define CONF_CHECK_MS     (30 * 1000ULL)

// A sysmodule: no applet, one fs session, a heap of its own.
u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

#define INNER_HEAP_SIZE 0x180000
static char g_heap[INNER_HEAP_SIZE];

void __libnx_initheap(void)
{
    extern char *fake_heap_start;
    extern char *fake_heap_end;
    fake_heap_start = g_heap;
    fake_heap_end   = g_heap + sizeof(g_heap);
}

void __appInit(void)
{
    Result rc = smInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);
    // The firmware version (the service layer gates commands on it), and
    // Atmosphère's mark (bit 31), as hbloader gives an application.
    if (R_SUCCEEDED(setsysInitialize())) {
        SetSysFirmwareVersion fw;
        if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw))) {
            u32 v = MAKEHOSVERSION(fw.major, fw.minor, fw.micro);
            if (R_SUCCEEDED(splInitialize())) {
                u64 ams = 0;
                if (R_SUCCEEDED(splGetConfig((SplConfigItem)65000, &ams))) v |= BIT(31);
                splExit();
            }
            hosversionSet(v);
        }
        setsysExit();
    }
    rc = fsInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);
    fsdevMountSdmc();
    timeInitialize();
}

void __appExit(void)
{
    timeExit();
    fsdevUnmountAll();
    fsExit();
    smExit();
}

// ---- shared with the IPC thread (agent_ipc.c) ----

AgentShared g_shared;
Mutex       g_lock;
void        agent_ipc_start(void);

static void slog(const char *fmt, const char *a, const char *b)
{
    char line[AGENT_LOG_LINE];
    snprintf(line, sizeof(line), fmt, a ? a : "", b ? b : "");
    mutexLock(&g_lock);
    agent_log(&g_shared, line);
    mutexUnlock(&g_lock);
}

// ---- files ----

static size_t read_file(const char *path, char *out, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    const size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    return n;
}

static bool write_file(const char *path, const char *text, size_t len)
{
    char tmp[160];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    const bool ok = fwrite(text, 1, len, f) == len;
    if (fclose(f) != 0 || !ok) {
        remove(tmp);
        return false;
    }
    remove(path);   // FAT: no rename over a file
    return rename(tmp, path) == 0;
}

static uint64_t mtime_of(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (uint64_t)st.st_mtime : 0;
}

// ---- the agent's state ----

static SyncConf      g_conf;
static bool          g_conf_ok;
static uint64_t      g_conf_mtime, g_nro_mtime, g_conf_checked;
static AgentNroState g_nro;
static SyncRecords   g_records;
static char          g_firmware[16];
static char          g_today[11];
static SyncEngine    g_engine;
static bool          g_engine_on;
static SyncTcp       g_tcp;
static SyncTls       g_tls;
static bool          g_sockets;
static bool          g_stopped;   // PrepareShutdown: offline until the process ends

static char     g_text[0x4000];          // files read
static char     g_doc[AGENT_DOC_LARGE];  // documents taken from the shared state
static char     g_activity[AGENT_DOC_STATE];
static size_t   g_activity_len;
static uint64_t g_activity_at;
static uint32_t g_activity_s;            // today's total, for the state
static bool     g_activity_ok;
static u64      g_playing, g_playing_since;
static PlayLogTotal g_totals[PLAYSTATS_MAX];
static size_t   g_n_totals;
static u8       g_day_wday[7];

static void load_conf(void)
{
    sync_conf_defaults(&g_conf);
    const size_t n = read_file(CONF_PATH, g_text, sizeof(g_text));
    g_conf_ok = n > 0 && sync_conf_parse(&g_conf, g_text, n);
    g_conf_mtime = mtime_of(CONF_PATH);
}

static void save_records(void)
{
    char out[512];
    const size_t n = sync_records_write(&g_records, out, sizeof(out));
    if (n) write_file(AGENT_STATE, out, n);
    mutexLock(&g_lock);
    g_shared.records.valid = 1;
    g_shared.records.records = g_records;
    mutexUnlock(&g_lock);
}

// PlayGuard's records when it saved after the agent last did, else the agent's.
static void load_records(void)
{
    const size_t n = read_file(NRO_STATE, g_text, sizeof(g_text));
    agent_nro_parse(&g_nro, n ? g_text : NULL, n);
    g_nro_mtime = mtime_of(NRO_STATE);
    sync_records_clear(&g_records);
    if (g_nro.known && g_nro_mtime >= mtime_of(AGENT_STATE)) {
        g_records = g_nro.records;
    } else {
        const size_t m = read_file(AGENT_STATE, g_text, sizeof(g_text));
        if (m) sync_records_parse(&g_records, g_text, m);
    }
    mutexLock(&g_lock);
    g_shared.records.valid = 1;
    g_shared.records.records = g_records;
    mutexUnlock(&g_lock);
}

static bool may_write(void)
{
    return agent_may_write(&g_nro, g_firmware);
}

// ---- the console ----

// "YYYY-MM-DD" of a local time (`out` holds 11 bytes).
static void date_of(const LocalTime *t, char out[11])
{
    char text[24];
    snprintf(text, sizeof(text), "%04u-%02u-%02u", (unsigned)t->year % 10000u, (unsigned)t->month % 100u,
             (unsigned)t->day % 100u);
    memcpy(out, text, 10);
    out[10] = '\0';
}

static void today_text(char out[11], int *weekday)
{
    LocalTime t;
    u64 now = 0;
    if (time_local_now(&now, &t)) {
        date_of(&t, out);
        if (weekday) *weekday = t.wday;
    } else {
        out[0] = '\0';
        if (weekday) *weekday = -1;
    }
}

static void read_activity(uint64_t now_ms, bool force)
{
    if (!force && g_activity_at && now_ms - g_activity_at < ACTIVITY_EVERY_MS) return;
    g_activity_at = now_ms;
    u64 now = 0;
    Result rc = 0;
    g_n_totals = playstats_days(g_totals, PLAYSTATS_MAX, &now, g_day_wday, &g_playing, &g_playing_since, &rc);
    g_activity_ok = R_SUCCEEDED(rc);
    g_activity_s = 0;
    for (size_t i = 0; i < g_n_totals; i++) g_activity_s += g_totals[i].today_s;
    if (!g_activity_ok) {
        g_activity_len = 0;
        return;
    }
    // Today's document (k = 0).
    static SyncAppTime apps[PLAYSTATS_MAX];
    size_t n = 0;
    for (size_t i = 0; i < g_n_totals; i++)
        if (g_totals[i].today_s) apps[n++] = (SyncAppTime){ g_totals[i].app_id, g_totals[i].today_s };
    char date[11];
    today_text(date, NULL);
    SyncActivity a;
    memset(&a, 0, sizeof(a));
    a.source = "agent";
    a.ts = now;
    a.local_date = date;
    a.apps = apps;
    a.n_apps = n;
    a.now_playing = g_playing;
    a.now_playing_since = g_playing_since;
    g_activity_len = sync_activity_build(&a, g_activity, sizeof(g_activity));
}

// A finished day (k days back) from the last read.
static size_t final_doc(int k, const char *date, char *out, size_t cap)
{
    static SyncAppTime apps[PLAYSTATS_MAX];
    size_t n = 0;
    for (size_t i = 0; i < g_n_totals; i++)
        if (g_totals[i].day_s[k]) apps[n++] = (SyncAppTime){ g_totals[i].app_id, g_totals[i].day_s[k] };
    u64 now = 0;
    time_local_now(&now, NULL);
    SyncActivity a;
    memset(&a, 0, sizeof(a));
    a.source = "agent";
    a.ts = now;
    a.local_date = date;
    a.final = true;
    a.apps = apps;
    a.n_apps = n;
    return sync_activity_build(&a, out, cap);
}

// The game's name PlayGuard last wrote (names.txt), "" when unknown.
static void name_of(u64 app_id, char *out, size_t cap)
{
    out[0] = '\0';
    if (!app_id) return;
    char id[17];
    snprintf(id, sizeof(id), "%016llX", (unsigned long long)app_id);
    const size_t n = read_file(NAMES, g_text, sizeof(g_text));
    for (size_t at = 0; at < n;) {
        const char *line = g_text + at;
        const char *end = memchr(line, '\n', n - at);
        const size_t len = end ? (size_t)(end - line) : n - at;
        at += len + 1;
        if (len < 18 || line[16] != '=' || strncmp(line, id, 16)) continue;
        size_t name_len = len - 17;
        if (name_len && line[17 + name_len - 1] == '\r') name_len--;
        if (name_len >= cap) name_len = cap - 1;
        memcpy(out, line + 17, name_len);
        out[name_len] = '\0';
        return;
    }
}

static size_t own_state(char *out, size_t cap)
{
    static PctlStatus st;
    static PtState pt;
    static PtSample sample;
    static SysInfo si;
    pctl_status_fetch(&st);
    pctl_play_timer_query(&pt);
    pctl_play_timer_sample(&sample);
    sysinfo_get(&si);
    bool accurate = false;
    const bool accurate_ok = R_SUCCEEDED(time_network_accuracy(&accurate));
    char date[11];
    int weekday = -1;
    today_text(date, &weekday);
    u64 now = 0;
    time_local_now(&now, NULL);
    const SyncStatus *ls = sync_engine_status(&g_engine);

    SyncSnapshot s;
    memset(&s, 0, sizeof(s));
    s.source = "agent";
    s.ts = now;
    s.local_date = date;
    s.weekday = weekday;
    s.clock_accurate_ok = accurate_ok;
    s.clock_accurate = accurate;
    s.conf = &g_conf;
    s.sys = &si;
    s.app_version = NULL;
    s.agent_version = AGENT_VERSION;
    s.read_only = !may_write();
    s.status = &st;
    s.timer = &pt;
    s.spent_ok = R_SUCCEEDED(sample.session_rc) && R_SUCCEEDED(sample.spent_rc);
    s.spent_ns = sample.spent_ns;
    s.records = &g_records;
    s.activity_ok = g_activity_ok;
    s.activity_s = g_activity_s;
    s.now_playing = g_playing;
    s.now_playing_since = g_playing_since;
    char playing_name[129];
    name_of(g_playing, playing_name, sizeof(playing_name));
    s.now_playing_name = playing_name[0] ? playing_name : NULL;
    s.agent = true;
    s.last_result = ls->last_result;
    return sync_state_build(&s, out, cap);
}

// ---- the engine's host ----

static size_t host_state(void *ctx, char *out, size_t cap)
{
    (void)ctx;
    // PlayGuard in front: its document (it is the one reading pctl now).
    mutexLock(&g_lock);
    const bool front = g_shared.app_session && g_shared.app_foreground;
    size_t n = 0;
    if (front && g_shared.state_info.len && g_shared.state_info.len < cap) {
        n = g_shared.state_info.len;
        memcpy(out, g_shared.state, n);
    }
    mutexUnlock(&g_lock);
    if (front) return n;
    return own_state(out, cap);
}

static size_t host_activity(void *ctx, char *out, size_t cap)
{
    (void)ctx;
    mutexLock(&g_lock);
    const bool session = g_shared.app_session;
    size_t n = 0;
    if (session && g_shared.activity_info.len && g_shared.activity_info.len < cap) {
        n = g_shared.activity_info.len;
        memcpy(out, g_shared.activity, n);
    }
    mutexUnlock(&g_lock);
    if (session) return n;
    read_activity(sync_now_ms(), false);
    if (!g_activity_len || g_activity_len >= cap) return 0;
    memcpy(out, g_activity, g_activity_len);
    return g_activity_len;
}

static bool profile_days(void *ctx, const char *name, uint16_t days[7])
{
    (void)ctx;
    const size_t n = read_file(PROFILES, g_text, sizeof(g_text));
    for (size_t at = 0; at < n;) {
        const char *line = g_text + at;
        const char *end = memchr(line, '\n', n - at);
        const size_t len = end ? (size_t)(end - line) : n - at;
        at += len + 1;
        const char *eq = memchr(line, '=', len);
        if (!eq || line[0] == '#') continue;
        size_t name_len = len - (size_t)(eq + 1 - line);
        if (name_len && eq[name_len] == '\r') name_len--;
        if (name_len != strlen(name) || memcmp(eq + 1, name, name_len)) continue;
        unsigned v[7];
        if (sscanf(line, "%u,%u,%u,%u,%u,%u,%u", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]) != 7) return false;
        for (int d = 0; d < 7; d++) days[d] = (uint16_t)v[d];
        return true;
    }
    return false;
}

static void append_event(const char *entity, const char *payload, const SyncOutcome *o)
{
    // One line per order the agent carried out itself, for PlayGuard's
    // history: tab-separated (agent_events.log, docs/sync-protocol.md).
    char line[400];
    u64 now = 0;
    time_local_now(&now, NULL);
    int at = snprintf(line, sizeof(line), "%llu\t%s\t%s\t%d\t%s\t%d\t%d\t%s\t", (unsigned long long)now, entity,
                      payload, o->applied ? 1 : 0, sync_reason_name(o->reason), (int)o->change, o->n,
                      o->source ? o->source : "remote");
    for (int i = 0; i < o->n && at > 0 && at < (int)sizeof(line); i++)
        at += snprintf(line + at, sizeof(line) - (size_t)at, i ? ",%d" : "%d", o->before[i]);
    if (at > 0 && at < (int)sizeof(line)) at += snprintf(line + at, sizeof(line) - (size_t)at, "\t");
    for (int i = 0; i < o->n && at > 0 && at < (int)sizeof(line); i++)
        at += snprintf(line + at, sizeof(line) - (size_t)at, i ? ",%d" : "%d", o->after[i]);
    if (at > 0 && at < (int)sizeof(line)) at += snprintf(line + at, sizeof(line) - (size_t)at, "\t%d", o->console_lock_after);
    if (at <= 0 || at >= (int)sizeof(line) - 1) return;
    line[at++] = '\n';
    FILE *f = fopen(AGENT_EVENTS, "ab");
    if (!f) return;
    fwrite(line, 1, (size_t)at, f);
    fclose(f);
}

static void exec_order(const SyncIntent *in, const char *entity, const char *payload, SyncOutcome *out)
{
    char today[11];
    int weekday = -1;
    today_text(today, &weekday);
    SyncExecCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.remote_timer_writes = g_conf.remote_timer_writes;
    ctx.weekday = weekday;
    ctx.today = today;
    ctx.rec = &g_records;
    ctx.profile_days = profile_days;
    core_set_read_only(!may_write());
    sync_exec(in, &ctx, out);
    if (out->records_changed) save_records();
    if (out->applied && out->changed) append_event(entity, payload, out);
}

static bool host_order(void *ctx, uint32_t id, const SyncIntent *intent, const char *entity, const char *payload,
                       bool retained, SyncOutcome *out)
{
    (void)ctx;
    mutexLock(&g_lock);
    const bool session = g_shared.app_session;
    bool handed = false;
    if (session) {
        AgentOrder o;
        memset(&o, 0, sizeof(o));
        o.id = id;
        o.retained = retained;
        snprintf(o.entity, sizeof(o.entity), "%s", entity);
        snprintf(o.payload, sizeof(o.payload), "%s", payload);
        o.intent = *intent;
        handed = agent_forward(&g_shared, &o);
    }
    mutexUnlock(&g_lock);
    if (session) {
        if (handed) return false;   // PlayGuard answers it (OrderResult)
        out->reason = SyncReason_Busy;
        return true;
    }
    // On its own the agent only carries out orders under "auto"; with "ask"
    // they wait (retained) for PlayGuard to confirm them on the console.
    if (g_conf.policy != SyncPolicy_Auto) {
        out->reason = SyncReason_Waiting;
        return true;
    }
    exec_order(intent, entity, payload, out);
    slog("order %s: %s", entity, out->applied ? "applied" : sync_reason_name(out->reason));
    return true;
}

static size_t host_report(void *ctx, char *out, size_t cap)
{
    (void)ctx;
    int at = snprintf(out, cap, "=== PlayGuard agent report ===\nagent version: %s\nfirmware: %s\n\n", AGENT_VERSION,
                      g_firmware);
    if (at < 0 || (size_t)at >= cap) return 0;
    time_clock_dump(out + at, cap - (size_t)at);
    at += (int)strlen(out + at);
    if ((size_t)at < cap - 1) pctl_dump(out + at, cap - (size_t)at);
    const size_t n = strlen(out);
    // Kept on the SD card as PlayGuard's reports are.
    mkdir(LOGS_DIR, 0777);
    char path[128];
    u64 now = 0;
    time_local_now(&now, NULL);
    snprintf(path, sizeof(path), LOGS_DIR "/agent_%llu.txt", (unsigned long long)now);
    write_file(path, out, n);
    return n;
}

static size_t host_profiles(void *ctx, char (*names)[SYNC_PROFILE_MAX], size_t max)
{
    (void)ctx;
    const size_t n = read_file(PROFILES, g_text, sizeof(g_text));
    size_t count = 0;
    for (size_t at = 0; at < n && count < max;) {
        const char *line = g_text + at;
        const char *end = memchr(line, '\n', n - at);
        const size_t len = end ? (size_t)(end - line) : n - at;
        at += len + 1;
        const char *eq = memchr(line, '=', len);
        if (!eq || line[0] == '#') continue;
        size_t name_len = len - (size_t)(eq + 1 - line);
        if (name_len && eq[name_len] == '\r') name_len--;
        if (!name_len || name_len >= SYNC_PROFILE_MAX) continue;
        memcpy(names[count], eq + 1, name_len);
        names[count][name_len] = '\0';
        count++;
    }
    return count;
}

static void host_conf_changed(void *ctx, const SyncConf *conf)
{
    (void)ctx;
    // Only Home Assistant's discovery switch changes this way.
    g_conf.ha_discovery = conf->ha_discovery;
    const size_t n = sync_conf_write(&g_conf, g_text, sizeof(g_text));
    if (n && write_file(CONF_PATH, g_text, n)) g_conf_mtime = mtime_of(CONF_PATH);
}

static bool host_read_only(void *ctx)
{
    (void)ctx;
    return !may_write();
}

static uint64_t host_now_posix(void *ctx)
{
    (void)ctx;
    u64 now = 0;
    time_local_now(&now, NULL);
    return now;
}

static void host_log(void *ctx, const char *line)
{
    (void)ctx;
    slog("%s%s", line, NULL);
}

static const SyncHost HOST = {
    NULL, host_state, host_activity, host_order, host_report, host_profiles, host_conf_changed,
    host_read_only, host_now_posix, host_log,
};

// ---- the loop ----

static bool sockets_up(void)
{
    if (g_sockets) return true;
    static const SocketInitConfig cfg = {
        .tcp_tx_buf_size = 0x2000,
        .tcp_rx_buf_size = 0x2000,
        .tcp_tx_buf_max_size = 0x8000,
        .tcp_rx_buf_max_size = 0x8000,
        .udp_tx_buf_size = 0,
        .udp_rx_buf_size = 0,
        .sb_efficiency = 1,
        .num_bsd_sessions = 1,
        .bsd_service_type = BsdServiceType_Auto,
    };
    g_sockets = R_SUCCEEDED(socketInitialize(&cfg));
    return g_sockets;
}

static void engine_start(void)
{
    g_tcp.fd = -1;
    memset(&g_tls, 0, sizeof(g_tls));
    g_tls.tcp.fd = -1;
    const SyncIo io = g_conf.tls ? sync_tls_io(&g_tls, g_conf.ca_file) : sync_tcp_io(&g_tcp);
    sync_engine_init(&g_engine, &g_conf, io, &HOST, sync_now_ms, "agent", AGENT_VERSION);
    g_engine_on = true;
    mutexLock(&g_lock);
    snprintf(g_shared.console_id, sizeof(g_shared.console_id), "%s", g_conf.console_id);
    mutexUnlock(&g_lock);
    slog("link started (%.64s)%s", g_conf.host, NULL);
}

static void engine_stop(void)
{
    if (!g_engine_on) return;
    sync_engine_stop(&g_engine);
    g_engine_on = false;
}

static void reload(bool force)
{
    const bool conf_changed = force || mtime_of(CONF_PATH) != g_conf_mtime;
    const SyncConf before = g_conf;
    if (conf_changed) load_conf();
    if (force || mtime_of(NRO_STATE) != g_nro_mtime) {
        // PlayGuard saved (it adopts the agent's records first): its view wins.
        const size_t n = read_file(NRO_STATE, g_text, sizeof(g_text));
        agent_nro_parse(&g_nro, n ? g_text : NULL, n);
        g_nro_mtime = mtime_of(NRO_STATE);
        if (g_nro.known) {
            g_records = g_nro.records;
            save_records();
        }
    }
    if (!conf_changed || !g_engine_on) return;
    // Another stream (TLS) needs another engine; else it reconnects if needed.
    if (before.tls != g_conf.tls || strcmp(before.ca_file, g_conf.ca_file)) {
        engine_stop();
        engine_start();
    } else {
        sync_engine_reconfigure(&g_engine, &g_conf);
    }
}

// A new local day: the finished one, and extra time put back (PlayGuard does
// both itself while it runs).
static void new_day(bool app_session)
{
    char today[11];
    today_text(today, NULL);
    if (!today[0] || !strcmp(today, g_today)) return;
    const bool first = !g_today[0];
    memcpy(g_today, today, sizeof(g_today));
    if (app_session) return;
    if (!first && g_conf.publish_activity && sync_engine_online(&g_engine)) {
        read_activity(sync_now_ms(), true);
        // Yesterday, as the date it was.
        char date[11] = "";
        const TimeRule *rule = time_console_rule();
        u64 now = 0;
        time_local_now(&now, NULL);
        LocalTime t;
        if (rule && rule->to_local(rule->ctx, local_midnight(rule, now, 1), &t)) date_of(&t, date);
        if (date[0]) {
            const size_t n = final_doc(1, date, g_doc, sizeof(g_doc));
            char sub[32];
            snprintf(sub, sizeof(sub), "activity/%s", date);
            if (n) sync_engine_publish(&g_engine, sub, g_doc, n, true);
        }
    }
    if (may_write()) {
        int weekday = -1;
        today_text(today, &weekday);
        SyncExecCtx ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.remote_timer_writes = true;   // putting the usual limit back is not a remote order
        ctx.weekday = weekday;
        ctx.today = today;
        ctx.rec = &g_records;
        core_set_read_only(false);
        SyncOutcome out;
        if (sync_exec_restore_extra(&ctx, &out) || out.records_changed) {
            if (out.records_changed) save_records();
            if (out.applied && out.changed) append_event("restore_extra", "", &out);
            slog("usual limit put back: %s%s", out.applied ? "done" : sync_reason_name(out.reason), NULL);
        }
    }
}

static void hand_over(void)
{
    // Results PlayGuard sent, its requests, and what it pushed.
    AgentResult results[AGENT_ORDERS_MAX];
    uint32_t waiting[AGENT_ORDERS_MAX];
    size_t n_waiting = 0;
    mutexLock(&g_lock);
    const size_t n_results = agent_take_results(&g_shared, results, AGENT_ORDERS_MAX);
    const bool sync_now = g_shared.want_sync_now, want_reload = g_shared.want_reload;
    const bool session_changed = g_shared.session_changed, session = g_shared.app_session;
    const bool foreground_changed = g_shared.foreground_changed;
    g_shared.want_sync_now = g_shared.want_reload = g_shared.session_changed = g_shared.foreground_changed = false;
    const bool state_new = g_shared.state_info.fresh, activity_new = g_shared.activity_info.fresh;
    g_shared.state_info.fresh = g_shared.activity_info.fresh = false;
    char app_version[AGENT_VERSION_MAX];
    snprintf(app_version, sizeof(app_version), "%s", g_shared.app_version);
    mutexUnlock(&g_lock);

    for (size_t i = 0; i < n_results; i++) {
        SyncOutcome o;
        sync_outcome_init(&o);
        o.rc = results[i].rc;
        o.applied = results[i].applied;
        o.changed = results[i].changed;
        o.reason = (SyncReason)results[i].reason;
        o.relock_failed = results[i].relock_failed;
        sync_engine_order_done(&g_engine, results[i].id, &o);
    }
    if (want_reload) {
        // PlayGuard saved: sync.conf, the profiles, read-only mode.
        reload(true);
        if (g_engine_on) sync_engine_discovery_changed(&g_engine);
    }
    if (session_changed) {
        if (session) {
            slog("PlayGuard opened (%s)%s", app_version, NULL);
        } else {
            mutexLock(&g_lock);
            n_waiting = agent_take_closed(&g_shared, waiting, AGENT_ORDERS_MAX);
            mutexUnlock(&g_lock);
            for (size_t i = 0; i < n_waiting; i++) {
                SyncOutcome o;
                sync_outcome_init(&o);
                o.reason = SyncReason_Waiting;
                sync_engine_order_done(&g_engine, waiting[i], &o);
            }
            reload(true);   // what PlayGuard saved last
            slog("PlayGuard closed%s%s", NULL, NULL);
        }
        // The orders kept on the broker come again: to PlayGuard now that
        // it is there, or to the agent now that it is gone.
        sync_engine_replay_orders(&g_engine);
        sync_engine_state_changed(&g_engine);
        sync_engine_activity_changed(&g_engine);
    }
    if (foreground_changed || state_new) sync_engine_state_changed(&g_engine);
    if (activity_new) sync_engine_activity_changed(&g_engine);
    if (sync_now) sync_engine_sync_now(&g_engine);

    // Names, the week and finished days, published as they come.
    if (!sync_engine_online(&g_engine)) return;
    size_t n;
    mutexLock(&g_lock);
    n = agent_take_doc(g_shared.names, &g_shared.names_info, g_doc, sizeof(g_doc));
    mutexUnlock(&g_lock);
    if (n) sync_engine_publish(&g_engine, "names", g_doc, n, true);
    mutexLock(&g_lock);
    n = agent_take_doc(g_shared.week, &g_shared.week_info, g_doc, sizeof(g_doc));
    mutexUnlock(&g_lock);
    if (n) sync_engine_publish(&g_engine, "week", g_doc, n, true);
    for (int i = 0; i < AGENT_FINALS; i++) {
        char sub[32] = "";
        n = 0;
        mutexLock(&g_lock);
        AgentFinal *f = &g_shared.finals[i];
        if (f->fresh && f->len < sizeof(g_doc)) {
            snprintf(sub, sizeof(sub), "activity/%s", f->date);
            memcpy(g_doc, f->doc, f->len);
            n = f->len;
            f->fresh = false;
        }
        mutexUnlock(&g_lock);
        // An empty document clears the day (PlayGuard drops the old ones).
        if (sub[0]) sync_engine_publish(&g_engine, sub, g_doc, n, true);
    }
}

static void publish_status(void)
{
    mutexLock(&g_lock);
    g_shared.status.link = *sync_engine_status(&g_engine);
    g_shared.status.pending = (uint32_t)sync_engine_pending(&g_engine);
    g_shared.status.configured = g_conf.enabled && !sync_conf_problem(&g_conf);
    g_shared.status.read_only = !may_write();
    g_shared.status.uptime_ms = sync_now_ms();
    mutexUnlock(&g_lock);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    mutexInit(&g_lock);
    agent_shared_init(&g_shared, AGENT_VERSION);
    {
        char fw[16];
        sysinfo_version_string(hosversionGet(), fw, sizeof(fw));
        snprintf(g_firmware, sizeof(g_firmware), "%s", fw);
    }
    mkdir(SYNC_DIR, 0777);
    load_conf();
    load_records();
    agent_ipc_start();
    slog("agent %s started, firmware %s", AGENT_VERSION, g_firmware);

    for (;;) {
        const uint64_t now = sync_now_ms();
        if (now - g_conf_checked >= CONF_CHECK_MS) {
            g_conf_checked = now;
            reload(false);
        }
        mutexLock(&g_lock);
        const bool shutdown = g_shared.want_shutdown;
        const bool session = g_shared.app_session;
        mutexUnlock(&g_lock);
        if (shutdown && !g_stopped) {
            engine_stop();   // "offline", DISCONNECT
            g_stopped = true;
            slog("stopped for PlayGuard%s%s", NULL, NULL);
        } else if (!shutdown && g_stopped) {
            // PlayGuard said Hello again: the update or the stop did not happen.
            g_stopped = false;
            slog("resumed for PlayGuard%s%s", NULL, NULL);
        }
        if (g_stopped || !g_conf_ok || !g_conf.enabled || sync_conf_problem(&g_conf)) {
            if (!g_stopped) engine_stop();
            publish_status();
            svcSleepThread(1000000000ULL);
            // Requests still come (reload, a new session); results have
            // nowhere to go and wait.
            mutexLock(&g_lock);
            if (g_shared.want_reload) {
                g_shared.want_reload = false;
                mutexUnlock(&g_lock);
                reload(true);
            } else {
                mutexUnlock(&g_lock);
            }
            continue;
        }
        if (!sockets_up()) {
            svcSleepThread(5000000000ULL);
            continue;
        }
        if (!g_engine_on) engine_start();
        new_day(session);
        hand_over();
        const int sleep_ms = sync_engine_step(&g_engine, 500);
        publish_status();
        if (sleep_ms > 0) svcSleepThread((u64)(sleep_ms > 1000 ? 1000 : sleep_ms) * 1000000ULL);
    }
    return 0;
}
