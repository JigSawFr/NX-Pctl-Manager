// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "sync_engine.h"

#include <stdio.h>
#include <string.h>

#include "sync_discovery.h"
#include "sync_state.h"

#define CONNECT_TIMEOUT_MS   8000
#define BACKOFF_MIN_MS       5000ULL
#define BACKOFF_MAX_MS       300000ULL
#define ACTIVITY_PERIOD_MS   60000ULL
#define STATE_SOON_MS        300ULL

static void logf_(SyncEngine *e, const char *fmt, const char *a, const char *b)
{
    if (!e->host.log) return;
    snprintf(e->line, sizeof(e->line), fmt, a ? a : "", b ? b : "");
    e->host.log(e->host.ctx, e->line);
}

static uint64_t now_posix(SyncEngine *e)
{
    return e->host.now_posix ? e->host.now_posix(e->host.ctx) : 0;
}

static bool read_only(SyncEngine *e)
{
    return e->host.read_only && e->host.read_only(e->host.ctx);
}

static void on_message(void *user, const MqttPublish *msg);

static void identity(SyncEngine *e)
{
    snprintf(e->base, sizeof(e->base), "%s/%s", e->conf.topic_prefix, e->conf.console_id);
    snprintf(e->client_id, sizeof(e->client_id), "pg-%s-%s", e->conf.console_id, e->source);
}

void sync_engine_init(SyncEngine *e, const SyncConf *conf, SyncIo io, const SyncHost *host, uint64_t (*now_ms)(void),
                      const char *source, const char *sw_version)
{
    memset(e, 0, sizeof(*e));
    e->conf = *conf;
    e->host = *host;
    e->now_ms = now_ms;
    e->source = source;
    e->sw_version = sw_version;
    e->next_id = 1;
    identity(e);
    mqtt_client_init(&e->mqtt, io, e->rx, sizeof(e->rx), now_ms, on_message, e);
}

bool sync_engine_online(const SyncEngine *e)
{
    return e->mqtt.connected;
}

const SyncStatus *sync_engine_status(const SyncEngine *e)
{
    return &e->status;
}

size_t sync_engine_pending(const SyncEngine *e)
{
    size_t n = 0;
    for (size_t i = 0; i < SYNC_QUEUE_MAX; i++) n += e->queue[i].id != 0;
    return n;
}

void sync_engine_sync_now(SyncEngine *e)
{
    e->want_state = e->want_activity = e->want_discovery = e->want_names = true;
    // "online" again: another client of the same console (PlayGuard's own
    // session, handing over to the agent) may have left "offline" behind.
    e->want_online = true;
}

void sync_engine_state_changed(SyncEngine *e)
{
    e->want_state = true;
}

void sync_engine_activity_changed(SyncEngine *e)
{
    e->want_activity = true;
}

void sync_engine_discovery_changed(SyncEngine *e)
{
    e->want_discovery = true;
}

int sync_engine_publish(SyncEngine *e, const char *sub, const char *payload, size_t len, bool retained)
{
    if (!e->mqtt.connected) return -1;
    char topic[160];
    snprintf(topic, sizeof(topic), "%s/%s", e->base, sub);
    const int rc = mqtt_client_publish(&e->mqtt, topic, payload, len, retained);
    if (rc == 0) {
        e->status.publishes++;
        e->status.last_publish_ms = e->now_ms();
    }
    return rc;
}

static void lost(SyncEngine *e)
{
    snprintf(e->status.error, sizeof(e->status.error), "%s", e->mqtt.error[0] ? e->mqtt.error : "connection lost");
    e->status.state = SyncLink_Waiting;
    e->status.next_attempt_ms = e->now_ms() + e->backoff_ms;
    for (size_t i = 0; i < SYNC_QUEUE_MAX; i++)
        if (e->queue[i].id && !e->queue[i].in_flight) memset(&e->queue[i], 0, sizeof(e->queue[i]));
    logf_(e, "remote link: %s%s", e->status.error, NULL);
}

void sync_engine_stop(SyncEngine *e)
{
    if (e->mqtt.connected) {
        char topic[160];
        snprintf(topic, sizeof(topic), "%s/availability", e->base);
        mqtt_client_publish(&e->mqtt, topic, "offline", 7, true);
    }
    mqtt_client_close(&e->mqtt, true);
    e->status.state = SyncLink_Off;
    e->discovery_published = false;
}

void sync_engine_reconfigure(SyncEngine *e, const SyncConf *conf)
{
    const bool reconnect = strcmp(conf->host, e->conf.host) || conf->port != e->conf.port || conf->tls != e->conf.tls ||
                           strcmp(conf->ca_file, e->conf.ca_file) || strcmp(conf->username, e->conf.username) ||
                           strcmp(conf->password, e->conf.password) || strcmp(conf->console_id, e->conf.console_id) ||
                           strcmp(conf->topic_prefix, e->conf.topic_prefix) ||
                           strcmp(conf->discovery_prefix, e->conf.discovery_prefix) ||
                           conf->allow_anonymous != e->conf.allow_anonymous || conf->enabled != e->conf.enabled ||
                           conf->mqtt_version != e->conf.mqtt_version;
    const bool discovery_off = e->conf.ha_discovery && !conf->ha_discovery;
    if (reconnect || discovery_off) {
        // Clear the old discovery while still connected under the old identity.
        if (e->mqtt.connected && e->conf.ha_discovery && (discovery_off || !conf->enabled ||
                                                          strcmp(conf->console_id, e->conf.console_id) ||
                                                          strcmp(conf->discovery_prefix, e->conf.discovery_prefix))) {
            char topic[160];
            if (sync_discovery_topic(&e->conf, topic, sizeof(topic))) mqtt_client_publish(&e->mqtt, topic, "", 0, true);
            e->discovery_published = false;
        }
    }
    if (reconnect) {
        sync_engine_stop(e);
        e->proto_ok = 0;   // another broker, or another version asked: "auto" tries 5.0 again
    }
    e->conf = *conf;
    identity(e);
    e->backoff_ms = 0;
    e->status.next_attempt_ms = 0;
    if (e->status.state != SyncLink_Online) e->status.state = SyncLink_Waiting;
    sync_engine_sync_now(e);
}

// ---- receiving ----

static uint32_t hash(const char *a, const char *b)
{
    uint32_t h = 2166136261u;
    for (const char *p = a; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    h = (h ^ '/') * 16777619u;
    for (const char *p = b; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    return h ? h : 1;
}

static bool entity_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

static void on_message(void *user, const MqttPublish *msg)
{
    SyncEngine *e = (SyncEngine *)user;

    // Home Assistant (re)started: publish everything again.
    char status_topic[64];
    const int sn = snprintf(status_topic, sizeof(status_topic), "%s/status", e->conf.discovery_prefix);
    if (sn > 0 && (size_t)sn == msg->topic_len && !memcmp(msg->topic, status_topic, (size_t)sn)) {
        if (msg->payload_len == 6 && !memcmp(msg->payload, "online", 6)) sync_engine_sync_now(e);
        return;
    }

    // <base>/<entity>/set
    const size_t bl = strlen(e->base);
    if (msg->topic_len < bl + 6 || memcmp(msg->topic, e->base, bl) || msg->topic[bl] != '/') return;
    if (memcmp(msg->topic + msg->topic_len - 4, "/set", 4)) return;
    const size_t en = msg->topic_len - bl - 1 - 4;
    if (en == 0 || en >= SYNC_ENTITY_MAX) return;
    char entity[SYNC_ENTITY_MAX];
    memcpy(entity, msg->topic + bl + 1, en);
    entity[en] = '\0';
    for (size_t i = 0; i < en; i++)
        if (!entity_char(entity[i])) return;

    // An empty payload clears a retained order (ours, echoed back, or the
    // sender taking it back): drop it if still queued.
    if (msg->payload_len == 0) {
        for (size_t i = 0; i < SYNC_QUEUE_MAX; i++)
            if (e->queue[i].id && !e->queue[i].in_flight && !strcmp(e->queue[i].entity, entity))
                memset(&e->queue[i], 0, sizeof(e->queue[i]));
        return;
    }
    if (msg->payload_len >= SYNC_PAYLOAD_MAX) {
        e->status.dropped++;
        return;
    }

    // The newest order for an entity replaces one still waiting in the queue.
    SyncQueued *slot = NULL;
    for (size_t i = 0; i < SYNC_QUEUE_MAX && !slot; i++)
        if (e->queue[i].id && !e->queue[i].in_flight && !strcmp(e->queue[i].entity, entity)) slot = &e->queue[i];
    for (size_t i = 0; i < SYNC_QUEUE_MAX && !slot; i++)
        if (!e->queue[i].id) slot = &e->queue[i];
    if (!slot) {
        // Full: it stays retained on the broker and comes back at the next connection.
        e->status.dropped++;
        return;
    }
    memset(slot, 0, sizeof(*slot));
    memcpy(slot->entity, entity, en + 1);
    memcpy(slot->payload, msg->payload, msg->payload_len);
    slot->payload[msg->payload_len] = '\0';
    slot->retained = msg->retain;
    slot->id = e->next_id++;
    if (e->next_id == 0) e->next_id = 1;
    slot->group = slot->id;
    slot->received_ms = e->now_ms();
}

// ---- orders ----

static const char *event_type(const SyncOutcome *out)
{
    if (out->applied) return "command_applied";
    if (out->reason == SyncReason_Waiting) return "command_waiting";
    return "command_rejected";
}

static void publish_event(SyncEngine *e, const char *type, const char *entity, const char *payload, SyncReason reason,
                          bool rc_set, uint32_t rc)
{
    char json[512];
    const size_t n = sync_event_build(e->source, now_posix(e), type, entity, payload, reason, rc_set, rc, json,
                                      sizeof(json));
    if (n) sync_engine_publish(e, "event", json, n, false);
}

void sync_engine_event(SyncEngine *e, const char *type, const char *entity)
{
    publish_event(e, type, entity, NULL, SyncReason_None, false, 0);
}

// Reports one queued order's outcome and frees its slot.
static void finish_one(SyncEngine *e, SyncQueued *q, const SyncOutcome *out)
{
    const bool waiting = out->reason == SyncReason_Waiting;
    const uint32_t h = hash(q->entity, q->payload);
    bool already = false;
    if (waiting) {
        for (size_t i = 0; i < SYNC_QUEUE_MAX; i++) already = already || e->waiting[i] == h;
        if (!already)
            for (size_t i = 0; i < SYNC_QUEUE_MAX; i++)
                if (!e->waiting[i]) {
                    e->waiting[i] = h;
                    break;
                }
    } else {
        for (size_t i = 0; i < SYNC_QUEUE_MAX; i++)
            if (e->waiting[i] == h) e->waiting[i] = 0;
    }
    if (!already) {
        const bool rc_set = out->rc != 0;
        publish_event(e, event_type(out), q->entity, q->payload, out->reason, rc_set, out->rc);
    }
    if (!waiting) {
        // Used once: clear it on the broker.
        char sub[SYNC_ENTITY_MAX + 8];
        snprintf(sub, sizeof(sub), "%s/set", q->entity);
        sync_engine_publish(e, sub, "", 0, true);
        e->status.orders++;
        if (!out->applied) e->status.rejected++;
        snprintf(e->status.last_result, sizeof(e->status.last_result), "%s: %s", q->entity,
                 out->applied ? "applied" : sync_reason_name(out->reason));
    }
    memset(q, 0, sizeof(*q));
    e->want_state = true;
    e->next_state_ms = e->now_ms() + STATE_SOON_MS;
}

static void finish_group(SyncEngine *e, uint32_t group, const SyncOutcome *out)
{
    for (size_t i = 0; i < SYNC_QUEUE_MAX; i++)
        if (e->queue[i].id && e->queue[i].group == group) finish_one(e, &e->queue[i], out);
}

void sync_engine_order_done(SyncEngine *e, uint32_t id, const SyncOutcome *out)
{
    finish_group(e, id, out);
}

static void reject(SyncEngine *e, SyncQueued *q, SyncReason reason)
{
    SyncOutcome out;
    sync_outcome_init(&out);
    out.reason = reason;
    finish_one(e, q, &out);
}

static void accept(SyncEngine *e, SyncQueued *q)
{
    SyncOutcome out;
    sync_outcome_init(&out);
    out.applied = true;
    finish_one(e, q, &out);
}

static bool is_limit(SyncIntentKind k)
{
    return k == SyncIntent_LimitDay || k == SyncIntent_LimitUniform || k == SyncIntent_LimitsWeek;
}

// Oldest first among the free orders.
static SyncQueued *next_order(SyncEngine *e)
{
    SyncQueued *best = NULL;
    for (size_t i = 0; i < SYNC_QUEUE_MAX; i++) {
        SyncQueued *q = &e->queue[i];
        if (q->id && !q->in_flight && (!best || q->id < best->id)) best = q;
    }
    return best;
}

static void handle_engine_order(SyncEngine *e, SyncQueued *q, const SyncIntent *in)
{
    if (in->kind == SyncIntent_Discovery) {
        if (e->conf.ha_discovery != in->on) {
            if (!in->on && e->discovery_published) {
                char topic[160];
                if (sync_discovery_topic(&e->conf, topic, sizeof(topic))) mqtt_client_publish(&e->mqtt, topic, "", 0, true);
                e->discovery_published = false;
            }
            e->conf.ha_discovery = in->on;
            if (e->host.conf_changed) e->host.conf_changed(e->host.ctx, &e->conf);
            if (in->on) e->want_discovery = true;
        }
        accept(e, q);
        return;
    }
    if (in->kind == SyncIntent_SyncNow) {
        sync_engine_sync_now(e);
        accept(e, q);
        return;
    }
    // ExportReport (a host without SyncHost.report gets it as an order)
    const size_t n = e->host.report(e->host.ctx, e->scratch, sizeof(e->scratch));
    if (!n) {
        reject(e, q, SyncReason_Busy);
        return;
    }
    if (e->conf.publish_report) sync_engine_publish(e, "report", e->scratch, n, true);
    accept(e, q);
}

static void process_orders(SyncEngine *e)
{
    const uint64_t now = e->now_ms();
    // Limits keep coming while a slider moves: wait for a pause.
    uint64_t newest_limit = 0;
    for (size_t i = 0; i < SYNC_QUEUE_MAX; i++) {
        SyncIntent tmp;
        const SyncQueued *q = &e->queue[i];
        if (q->id && !q->in_flight && sync_apply_parse(q->entity, q->payload, strlen(q->payload), &tmp) == SyncReason_None &&
            is_limit(tmp.kind) && q->received_ms > newest_limit)
            newest_limit = q->received_ms;
    }
    const bool limits_settled = newest_limit == 0 || now - newest_limit >= SYNC_LIMIT_SETTLE_MS;

    for (int guard = 0; guard < SYNC_QUEUE_MAX; guard++) {
        SyncQueued *q = next_order(e);
        if (!q || !e->mqtt.connected) return;
        SyncIntent in;
        const SyncReason parsed = sync_apply_parse(q->entity, q->payload, strlen(q->payload), &in);
        if (parsed != SyncReason_None) {
            reject(e, q, parsed);
            continue;
        }
        // A report the host cannot write here (it reads the console on
        // another thread) goes to it like an order.
        const bool host_report = in.kind == SyncIntent_ExportReport && !e->host.report;
        if (!sync_intent_on_console(in.kind) && !host_report) {
            if (e->conf.policy == SyncPolicy_Off && !sync_intent_harmless(in.kind) && in.kind != SyncIntent_Discovery) {
                reject(e, q, SyncReason_PolicyOff);
                continue;
            }
            handle_engine_order(e, q, &in);
            continue;
        }
        if (!host_report) {
            if (e->conf.policy == SyncPolicy_Off) {
                reject(e, q, SyncReason_PolicyOff);
                continue;
            }
            if (sync_intent_needs_timer_writes(in.kind) && !e->conf.remote_timer_writes) {
                reject(e, q, SyncReason_TimerWritesDisabled);
                continue;
            }
            if (read_only(e)) {
                reject(e, q, SyncReason_ReadOnly);
                continue;
            }
        }
        if (is_limit(in.kind)) {
            if (!limits_settled) return;   // the next turn
            // Every limit order waiting, in the order they came, as one week.
            SyncIntent week;
            memset(&week, 0, sizeof(week));
            week.kind = SyncIntent_LimitsWeek;
            for (size_t i = 0; i < SYNC_QUEUE_MAX; i++) {
                SyncQueued *c = &e->queue[i];
                SyncIntent ci;
                if (!c->id || c->in_flight) continue;
                if (sync_apply_parse(c->entity, c->payload, strlen(c->payload), &ci) != SyncReason_None ||
                    !is_limit(ci.kind))
                    continue;
                c->group = q->id;
            }
            // Apply them oldest first, so a later one wins on the same day.
            uint32_t last = 0;
            for (;;) {
                SyncQueued *m = NULL;
                for (size_t i = 0; i < SYNC_QUEUE_MAX; i++) {
                    SyncQueued *c = &e->queue[i];
                    if (c->id && c->group == q->id && c->id > last && (!m || c->id < m->id)) m = c;
                }
                if (!m) break;
                last = m->id;
                SyncIntent ci;
                sync_apply_parse(m->entity, m->payload, strlen(m->payload), &ci);
                for (int d = 0; d < 7; d++)
                    if (ci.mask & (1u << d)) week.days[d] = ci.days[d];
                week.mask |= ci.mask;
            }
            in = week;
        }
        SyncOutcome out;
        sync_outcome_init(&out);
        const bool final = e->host.order(e->host.ctx, q->group, &in, q->entity, q->payload, q->retained, &out);
        if (final) {
            finish_group(e, q->group, &out);
        } else {
            const uint32_t g = q->group;
            for (size_t i = 0; i < SYNC_QUEUE_MAX; i++)
                if (e->queue[i].id && e->queue[i].group == g) e->queue[i].in_flight = true;
        }
    }
}

// ---- publishing ----

static void publish_discovery(SyncEngine *e)
{
    char topic[160];
    if (!sync_discovery_topic(&e->conf, topic, sizeof(topic))) return;
    const size_t n_profiles = e->host.profiles ? e->host.profiles(e->host.ctx, e->profiles, 16) : 0;
    SyncDiscovery d = { &e->conf, e->sw_version, read_only(e), (const char(*)[SYNC_PROFILE_MAX])e->profiles, n_profiles };
    const size_t n = sync_discovery_build(&d, e->scratch, sizeof(e->scratch));
    if (!n) {
        logf_(e, "remote link: the discovery does not fit%s%s", NULL, NULL);
        return;
    }
    if (mqtt_client_publish(&e->mqtt, topic, e->scratch, n, true) == 0) {
        e->status.publishes++;
        e->discovery_published = true;
    }
}

static void publish_due(SyncEngine *e)
{
    const uint64_t now = e->now_ms();
    if (e->want_discovery) {
        e->want_discovery = false;
        if (e->conf.ha_discovery) publish_discovery(e);
    }
    if (!e->mqtt.connected) return;
    if (e->want_online) {
        char topic[160];
        snprintf(topic, sizeof(topic), "%s/availability", e->base);
        if (mqtt_client_publish(&e->mqtt, topic, "online", 6, true) >= 0) e->want_online = false;
    }
    if (e->want_state || now >= e->next_state_ms) {
        if (now >= e->next_state_ms || e->want_state) {
            const size_t n = e->host.state ? e->host.state(e->host.ctx, e->scratch, sizeof(e->scratch)) : 0;
            if (n) sync_engine_publish(e, "state", e->scratch, n, true);
            e->want_state = false;
            e->next_state_ms = now + (uint64_t)e->conf.poll_s * 1000ULL;
        }
    }
    if (!e->mqtt.connected) return;
    if (e->conf.publish_activity && e->host.activity && (e->want_activity || now >= e->next_activity_ms)) {
        const size_t n = e->host.activity(e->host.ctx, e->scratch, sizeof(e->scratch));
        if (n) sync_engine_publish(e, "activity", e->scratch, n, true);
        e->want_activity = false;
        e->next_activity_ms = now + ACTIVITY_PERIOD_MS;
    }
}

static int subscribe(SyncEngine *e)
{
    char orders[160], ha[64];
    snprintf(orders, sizeof(orders), "%s/+/set", e->base);
    snprintf(ha, sizeof(ha), "%s/status", e->conf.discovery_prefix);
    const char *topics[2] = { orders, ha };
    const uint8_t qos[2] = { 1, 0 };
    return mqtt_client_subscribe(&e->mqtt, topics, qos, 2, CONNECT_TIMEOUT_MS);
}

void sync_engine_replay_orders(SyncEngine *e)
{
    if (!e->mqtt.connected) return;
    if (subscribe(e) < 0) lost(e);
}

static int connect_now(SyncEngine *e)
{
    e->status.state = SyncLink_Connecting;
    char will[160];
    snprintf(will, sizeof(will), "%s/availability", e->base);
    MqttConnect c;
    memset(&c, 0, sizeof(c));
    c.client_id = e->client_id;
    c.username = e->conf.username[0] ? e->conf.username : NULL;
    c.password = e->conf.username[0] && e->conf.password[0] ? e->conf.password : NULL;
    c.will_topic = will;
    c.will_payload = "offline";
    c.will_len = 7;
    c.will_retain = true;
    c.keepalive_s = SYNC_KEEPALIVE_S;
    c.clean_session = true;
    // "auto": 5.0, else 3.1.1 when the broker refuses 5.0 (or the other way
    // round once 3.1.1 worked), and the one that worked from then on.
    uint8_t other = 0;
    switch (e->conf.mqtt_version) {
        case SyncMqtt_V5: c.version = MQTT_V5; break;
        case SyncMqtt_V311: c.version = MQTT_V311; break;
        default:
            c.version = e->proto_ok ? e->proto_ok : MQTT_V5;
            other = c.version == MQTT_V5 ? MQTT_V311 : MQTT_V5;
            break;
    }
    int rc = mqtt_client_connect(&e->mqtt, e->conf.host, e->conf.port, &c, CONNECT_TIMEOUT_MS);
    if (rc < 0 && other && e->mqtt.version_refused) {
        logf_(e, "remote link: MQTT %s refused (%s), trying the other version", c.version == MQTT_V5 ? "5.0" : "3.1.1",
              e->mqtt.error);
        c.version = other;
        rc = mqtt_client_connect(&e->mqtt, e->conf.host, e->conf.port, &c, CONNECT_TIMEOUT_MS);
    }
    if (rc < 0) return -1;
    if (other) e->proto_ok = e->mqtt.version;
    e->status.protocol = e->mqtt.version;
    if (mqtt_client_publish(&e->mqtt, will, "online", 6, true) < 0) return -1;
    if (subscribe(e) < 0) return -1;
    return 0;
}

int sync_engine_step(SyncEngine *e, int max_wait_ms)
{
    const uint64_t now = e->now_ms();
    if (!e->conf.enabled || sync_conf_problem(&e->conf)) {
        if (e->mqtt.open) sync_engine_stop(e);
        e->status.state = SyncLink_Off;
        snprintf(e->status.error, sizeof(e->status.error), "%s",
                 e->conf.enabled ? sync_conf_problem(&e->conf) : "");
        return max_wait_ms;
    }

    if (!e->mqtt.connected) {
        if (now < e->status.next_attempt_ms) {
            e->status.state = SyncLink_Waiting;
            const uint64_t left = e->status.next_attempt_ms - now;
            return (int)(left < (uint64_t)max_wait_ms ? left : (uint64_t)max_wait_ms);
        }
        e->status.connects++;
        if (connect_now(e) < 0) {
            e->backoff_ms = e->backoff_ms ? e->backoff_ms * 2 : BACKOFF_MIN_MS;
            if (e->backoff_ms > BACKOFF_MAX_MS) e->backoff_ms = BACKOFF_MAX_MS;
            lost(e);
            return 0;
        }
        e->backoff_ms = 0;
        e->status.state = SyncLink_Online;
        e->status.error[0] = '\0';
        e->status.online_since_ms = e->now_ms();
        memset(e->waiting, 0, sizeof(e->waiting));
        sync_engine_sync_now(e);
        e->next_state_ms = 0;
        e->next_activity_ms = 0;
        logf_(e, "remote link: online (%s, MQTT %s)", e->conf.host, e->mqtt.version == MQTT_V5 ? "5.0" : "3.1.1");
    }

    process_orders(e);
    if (e->mqtt.connected) publish_due(e);
    if (!e->mqtt.connected) {
        if (!e->backoff_ms) e->backoff_ms = BACKOFF_MIN_MS;
        lost(e);
        return 0;
    }

    // Wait for the broker, but not past the next thing due.
    uint64_t wait = (uint64_t)(max_wait_ms > 0 ? max_wait_ms : 0);
    const uint64_t t = e->now_ms();
    if (e->next_state_ms > t && e->next_state_ms - t < wait) wait = e->next_state_ms - t;
    if (e->want_state || e->want_discovery || e->want_activity) wait = 0;
    for (size_t i = 0; i < SYNC_QUEUE_MAX; i++)
        if (e->queue[i].id && !e->queue[i].in_flight) wait = wait > 100 ? 100 : wait;
    if (mqtt_client_poll(&e->mqtt, (int)wait) < 0) {
        e->backoff_ms = BACKOFF_MIN_MS;
        lost(e);
    }
    return 0;
}
