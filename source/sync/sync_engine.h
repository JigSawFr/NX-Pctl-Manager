// sync_engine — the remote link's session, shared by PlayGuard and the agent
// sysmodule: connects to the broker (last will "offline"), subscribes to the
// orders, publishes the state, today's activity and Home Assistant's
// discovery, and hands each order to the host, which carries it out on the
// console. Single-threaded and allocation-free: the host owns one SyncEngine
// (about 40 KiB) and calls sync_engine_step() in a loop.
//
// An order's life: received on `<base>/<entity>/set` (retained by whoever
// sent it, so it waits on the broker while the console sleeps) -> parsed ->
// refused at once (unknown, out of range, policy, remote_timer_writes) or
// handed to the host -> an `event` says applied or why not -> the retained
// order is cleared (an empty retained publish), so it is used once -> the
// state is published again. Several limit orders arriving together (Home
// Assistant's sliders) are merged into one write after a short pause.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "mqtt_client.h"
#include "sync_apply.h"
#include "sync_conf.h"
#include "sync_exec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYNC_QUEUE_MAX       16
#define SYNC_RX_SIZE         4096
#define SYNC_SCRATCH_SIZE    16384
#define SYNC_LIMIT_SETTLE_MS 1500
#define SYNC_KEEPALIVE_S     30

typedef enum {
    SyncLink_Off = 0,     // disabled, or not configured
    SyncLink_Waiting,     // waiting before the next attempt
    SyncLink_Connecting,
    SyncLink_Online,
} SyncLinkState;

typedef struct {
    SyncLinkState state;
    char          error[128];         // the last failure ("" none)
    uint64_t      online_since_ms;
    uint64_t      last_publish_ms;
    uint64_t      next_attempt_ms;
    uint32_t      connects, publishes, orders, rejected, dropped;
    char          last_result[SYNC_ENTITY_MAX + 40];   // "limit_mon: applied"
} SyncStatus;

typedef struct {
    void *ctx;
    // The state document (sync_state_build). 0: nothing to publish now.
    size_t (*state)(void *ctx, char *out, size_t cap);
    // Today's activity (sync_activity_build). 0: none now. May be NULL.
    size_t (*activity)(void *ctx, char *out, size_t cap);
    // An order for the console. true: `out` is final. false: the host took it
    // (another thread, PlayGuard through the agent) and calls
    // sync_engine_order_done(id, …) later.
    bool (*order)(void *ctx, uint32_t id, const SyncIntent *intent, const char *entity, const char *payload,
                  bool retained, SyncOutcome *out);
    // Export a diagnostic report: saves it and returns its text (published
    // when publish_report is on). 0: failed. May be NULL.
    size_t (*report)(void *ctx, char *out, size_t cap);
    // The saved profiles, for the discovery's select. May be NULL.
    size_t (*profiles)(void *ctx, char (*names)[SYNC_PROFILE_MAX], size_t max);
    // An order changed the settings (ha_discovery): save them. May be NULL.
    void (*conf_changed)(void *ctx, const SyncConf *conf);
    // PlayGuard's read-only mode. May be NULL (never read-only).
    bool (*read_only)(void *ctx);
    // The console's user clock, POSIX seconds (events' ts).
    uint64_t (*now_posix)(void *ctx);
    // One line for the log. May be NULL.
    void (*log)(void *ctx, const char *line);
} SyncHost;

typedef struct {
    char     entity[SYNC_ENTITY_MAX];
    char     payload[SYNC_PAYLOAD_MAX];
    bool     retained;
    uint32_t id;          // 0: free slot
    uint32_t group;       // the order whose console call carries this one
    bool     in_flight;
    uint64_t received_ms;
} SyncQueued;

typedef struct {
    SyncConf    conf;
    SyncHost    host;
    MqttClient  mqtt;
    uint64_t  (*now_ms)(void);
    const char *source;          // "agent" or "app"
    const char *sw_version;
    char        base[96];        // <prefix>/<id>
    char        client_id[24];

    uint64_t    backoff_ms;
    bool        want_state, want_activity, want_discovery, want_names;
    uint64_t    next_state_ms, next_activity_ms;
    bool        discovery_published;

    SyncQueued  queue[SYNC_QUEUE_MAX];
    uint32_t    next_id;
    // Orders already reported as waiting (no new event when the broker
    // resends them): entity + payload hash.
    uint32_t    waiting[SYNC_QUEUE_MAX];

    SyncStatus  status;
    char        profiles[16][SYNC_PROFILE_MAX];
    uint8_t     rx[SYNC_RX_SIZE];
    char        scratch[SYNC_SCRATCH_SIZE];
    char        line[320];
} SyncEngine;

void sync_engine_init(SyncEngine *e, const SyncConf *conf, SyncIo io, const SyncHost *host, uint64_t (*now_ms)(void),
                      const char *source, const char *sw_version);
// New settings: reconnects when the broker or the identity changed, else
// publishes the discovery again.
void sync_engine_reconfigure(SyncEngine *e, const SyncConf *conf);
// One turn: connects when due, handles orders, publishes what is due, then
// waits for the broker for up to `max_wait_ms`. Returns how long the caller
// may sleep before the next turn (0: call again now).
int  sync_engine_step(SyncEngine *e, int max_wait_ms);
// Publish everything again at the next turn (Sync now, Home Assistant restarted).
void sync_engine_sync_now(SyncEngine *e);
void sync_engine_state_changed(SyncEngine *e);       // the state, at the next turn
void sync_engine_discovery_changed(SyncEngine *e);   // the profiles changed
// The outcome of an order the host took (SyncHost.order returned false).
void sync_engine_order_done(SyncEngine *e, uint32_t id, const SyncOutcome *out);
// Asks the broker for the retained orders again (re-subscribing resends
// them): the orders kept waiting for PlayGuard come back.
void sync_engine_replay_orders(SyncEngine *e);
// Publishes `<base>/<sub>` (retained or not). 0 or negative.
int  sync_engine_publish(SyncEngine *e, const char *sub, const char *payload, size_t len, bool retained);
// An event (`event_type`, optional entity) on `<base>/event`.
void sync_engine_event(SyncEngine *e, const char *event_type, const char *entity);
// "offline", DISCONNECT, closed. sync_engine_step() reconnects later unless
// the settings turn the link off.
void sync_engine_stop(SyncEngine *e);
bool sync_engine_online(const SyncEngine *e);
const SyncStatus *sync_engine_status(const SyncEngine *e);
size_t sync_engine_pending(const SyncEngine *e);

#ifdef __cplusplus
}
#endif
