// Host tests for source/sync/sync_engine.c and mqtt_client.c against a fake
// MQTT 3.1.1 broker kept in memory (retained messages, subscriptions, last
// will, keep-alive), with a scripted clock: the connection and its last
// will, the discovery, the state and activity, orders applied, refused,
// merged, taken by the host or kept waiting, Home Assistant restarting,
// the broker refusing or dropping the connection, oversized messages.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mqtt_client.h"
#include "mqtt_packet.h"
#include "sync_engine.h"

// ---------------------------------------------------------------- clock

static uint64_t now = 1000000;
static uint64_t clock_ms(void) { return now; }

// ---------------------------------------------------------------- broker

#define MAX_RETAINED 64
#define MAX_LOG      512

typedef struct {
    char    topic[160];
    char    payload[16384];
    size_t  len;
    bool    retain;
} Message;

static struct {
    bool     up;              // accepts connections
    uint8_t  refuse_code;     // CONNACK return code
    bool     connected;       // a client is connected (CONNECT accepted)
    bool     open;
    bool     answer_pings;
    char     client_id[64], username[64], password[64];
    Message  will;
    bool     has_will;
    char     subs[4][160];
    uint8_t  sub_qos[4];
    size_t   n_subs;
    Message  retained[MAX_RETAINED];
    size_t   n_retained;
    Message *log;             // every PUBLISH the client sent
    size_t   n_log;
    unsigned pings, disconnects, connects;
    uint8_t  in[65536];       // client -> broker, not parsed yet
    size_t   in_len;
    uint8_t  out[262144];     // broker -> client
    size_t   out_len;
    uint16_t next_id;
} broker;

static void broker_reset(void)
{
    Message *log = broker.log;
    memset(&broker, 0, sizeof(broker));
    broker.log = log;
    broker.up = true;
    broker.answer_pings = true;
    broker.next_id = 1;
}

static void emit(const uint8_t *b, size_t n)
{
    assert(broker.out_len + n <= sizeof(broker.out));
    memcpy(broker.out + broker.out_len, b, n);
    broker.out_len += n;
}

static void emit_publish(const char *topic, const char *payload, size_t len, bool retain, uint8_t qos)
{
    uint8_t head[256];
    const size_t h = mqtt_encode_publish_head(head, sizeof(head), topic, len, qos, retain, false, qos ? broker.next_id++ : 0);
    assert(h);
    emit(head, h);
    emit((const uint8_t *)payload, len);
}

static bool subscribed(const char *topic, uint8_t *qos)
{
    for (size_t i = 0; i < broker.n_subs; i++)
        if (mqtt_topic_matches(broker.subs[i], topic, strlen(topic))) {
            *qos = broker.sub_qos[i];
            return true;
        }
    return false;
}

static Message *find_retained(const char *topic)
{
    for (size_t i = 0; i < broker.n_retained; i++)
        if (!strcmp(broker.retained[i].topic, topic)) return &broker.retained[i];
    return NULL;
}

// Stores (or deletes, empty payload) a retained message, and delivers it to
// the client when it is subscribed (RETAIN 0: an established subscription).
static void broker_publish(const char *topic, const char *payload, size_t len, bool retain)
{
    if (retain) {
        Message *m = find_retained(topic);
        if (len == 0) {
            if (m) *m = broker.retained[--broker.n_retained];
        } else {
            if (!m) {
                assert(broker.n_retained < MAX_RETAINED);
                m = &broker.retained[broker.n_retained++];
            }
            snprintf(m->topic, sizeof(m->topic), "%s", topic);
            memcpy(m->payload, payload, len);
            m->payload[len] = '\0';
            m->len = len;
            m->retain = true;
        }
    }
    uint8_t qos;
    if (broker.connected && subscribed(topic, &qos)) emit_publish(topic, payload, len, false, qos);
}

static void broker_drop(bool send_will)
{
    if (broker.connected && send_will && broker.has_will)
        broker_publish(broker.will.topic, broker.will.payload, broker.will.len, broker.will.retain);
    broker.connected = false;
    broker.open = false;
    broker.n_subs = 0;
    broker.in_len = broker.out_len = 0;
}

static void read_str(const uint8_t **p, char *out, size_t cap)
{
    const size_t n = ((size_t)(*p)[0] << 8) | (*p)[1];
    assert(n < cap);
    memcpy(out, *p + 2, n);
    out[n] = '\0';
    *p += 2 + n;
}

static void broker_handle(const MqttPacket *pk)
{
    switch (pk->type) {
    case MQTT_CONNECT: {
        const uint8_t *p = pk->body + 7;
        const uint8_t flags = *p++;
        p += 2;   // keep-alive
        read_str(&p, broker.client_id, sizeof(broker.client_id));
        broker.has_will = flags & 0x04;
        if (broker.has_will) {
            read_str(&p, broker.will.topic, sizeof(broker.will.topic));
            char tmp[64];
            read_str(&p, tmp, sizeof(tmp));
            snprintf(broker.will.payload, sizeof(broker.will.payload), "%s", tmp);
            broker.will.len = strlen(tmp);
            broker.will.retain = flags & 0x20;
        }
        broker.username[0] = broker.password[0] = '\0';
        if (flags & 0x80) read_str(&p, broker.username, sizeof(broker.username));
        if (flags & 0x40) read_str(&p, broker.password, sizeof(broker.password));
        const uint8_t ack[4] = { 0x20, 0x02, 0x00, broker.refuse_code };
        emit(ack, 4);
        broker.connects++;
        broker.connected = broker.refuse_code == 0;
        break;
    }
    case MQTT_PUBLISH: {
        MqttPublish m;
        assert(mqtt_decode_publish(pk, &m) == MQTT_OK);
        assert(m.qos == 0);   // the client publishes QoS 0 only
        char topic[160];
        memcpy(topic, m.topic, m.topic_len);
        topic[m.topic_len] = '\0';
        assert(broker.n_log < MAX_LOG);
        Message *l = &broker.log[broker.n_log++];
        snprintf(l->topic, sizeof(l->topic), "%s", topic);
        memcpy(l->payload, m.payload, m.payload_len);
        l->payload[m.payload_len] = '\0';
        l->len = m.payload_len;
        l->retain = m.retain;
        broker_publish(topic, (const char *)m.payload, m.payload_len, m.retain);
        break;
    }
    case MQTT_SUBSCRIBE: {
        const uint8_t *p = pk->body;
        const uint16_t id = (uint16_t)((p[0] << 8) | p[1]);
        p += 2;
        uint8_t codes[4];
        size_t n = 0;
        char filters[4][160];
        while (p < pk->body + pk->body_len) {
            read_str(&p, filters[n], sizeof(filters[n]));
            const uint8_t q = *p++;
            // Replace an identical filter, as the spec says.
            size_t k = 0;
            while (k < broker.n_subs && strcmp(broker.subs[k], filters[n])) k++;
            if (k == broker.n_subs) broker.n_subs++;
            snprintf(broker.subs[k], sizeof(broker.subs[k]), "%s", filters[n]);
            broker.sub_qos[k] = q;
            codes[n++] = q;
        }
        uint8_t ack[8] = { 0x90, (uint8_t)(2 + n), (uint8_t)(id >> 8), (uint8_t)id };
        memcpy(ack + 4, codes, n);
        emit(ack, 4 + n);
        // Retained messages matching a new subscription, RETAIN 1.
        for (size_t f = 0; f < n; f++)
            for (size_t i = 0; i < broker.n_retained; i++)
                if (mqtt_topic_matches(filters[f], broker.retained[i].topic, strlen(broker.retained[i].topic)))
                    emit_publish(broker.retained[i].topic, broker.retained[i].payload, broker.retained[i].len, true,
                                 codes[f]);
        break;
    }
    case MQTT_PUBACK:
        break;
    case MQTT_PINGREQ:
        broker.pings++;
        if (broker.answer_pings) {
            const uint8_t r[2] = { 0xD0, 0x00 };
            emit(r, 2);
        }
        break;
    case MQTT_DISCONNECT:
        broker.disconnects++;
        broker_drop(false);
        break;
    default:
        assert(!"unexpected packet from the client");
    }
}

static void broker_feed(void)
{
    for (;;) {
        MqttPacket pk;
        if (mqtt_parse(broker.in, broker.in_len, sizeof(broker.in), &pk) != MQTT_OK) return;
        const size_t size = pk.size;
        broker_handle(&pk);
        if (!broker.open) return;
        memmove(broker.in, broker.in + size, broker.in_len - size);
        broker.in_len -= size;
    }
}

// ---------------------------------------------------------------- the client's stream

static int io_open(void *ctx, const char *host, uint16_t port, int timeout_ms, char *err, size_t err_size)
{
    (void)ctx;
    (void)timeout_ms;
    assert(!strcmp(host, "broker.lan") || !strcmp(host, "other.lan"));
    assert(port == 1883);
    if (!broker.up) {
        snprintf(err, err_size, "connection refused");
        return -1;
    }
    broker.open = true;
    broker.in_len = broker.out_len = 0;
    return 0;
}

static int io_write(void *ctx, const void *buf, size_t len, int timeout_ms)
{
    (void)ctx;
    (void)timeout_ms;
    if (!broker.open) return -1;
    assert(broker.in_len + len <= sizeof(broker.in));
    memcpy(broker.in + broker.in_len, buf, len);
    broker.in_len += len;
    broker_feed();
    return 0;
}

static long io_read(void *ctx, void *buf, size_t len, int timeout_ms)
{
    (void)ctx;
    if (!broker.open) return -1;
    if (broker.out_len == 0) {
        now += (uint64_t)(timeout_ms > 0 ? timeout_ms : 0);   // waited that long
        return 0;
    }
    const size_t n = broker.out_len < len ? broker.out_len : len;
    memcpy(buf, broker.out, n);
    memmove(broker.out, broker.out + n, broker.out_len - n);
    broker.out_len -= n;
    return (long)n;
}

static void io_close(void *ctx)
{
    (void)ctx;
    if (broker.open) broker_drop(true);
}

// ---------------------------------------------------------------- the host

static struct {
    unsigned states, activities, orders, reports, conf_changes;
    SyncIntent last_intent;
    char last_entity[64];
    bool last_retained;
    bool async;               // order() takes the order (returns false)
    uint32_t taken_id;
    SyncReason answer;        // the outcome order() gives
    bool read_only;
    SyncConf saved;
    unsigned profiles_asked;
} host;

static size_t h_state(void *ctx, char *out, size_t cap)
{
    (void)ctx;
    host.states++;
    return (size_t)snprintf(out, cap, "{\"schema\":1,\"n\":%u}", host.states);
}

static size_t h_activity(void *ctx, char *out, size_t cap)
{
    (void)ctx;
    host.activities++;
    return (size_t)snprintf(out, cap, "{\"total_min\":%u}", host.activities);
}

static bool h_order(void *ctx, uint32_t id, const SyncIntent *in, const char *entity, const char *payload, bool retained,
                    SyncOutcome *out)
{
    (void)ctx;
    (void)payload;
    host.orders++;
    host.last_intent = *in;
    snprintf(host.last_entity, sizeof(host.last_entity), "%s", entity);
    host.last_retained = retained;
    if (host.async) {
        host.taken_id = id;
        return false;
    }
    out->applied = host.answer == SyncReason_None;
    out->reason = host.answer;
    return true;
}

static size_t h_report(void *ctx, char *out, size_t cap)
{
    (void)ctx;
    host.reports++;
    return (size_t)snprintf(out, cap, "=== PlayGuard diagnostic ===\n");
}

static size_t h_profiles(void *ctx, char (*names)[SYNC_PROFILE_MAX], size_t max)
{
    (void)ctx;
    host.profiles_asked++;
    assert(max >= 2);
    strcpy(names[0], "School week");
    strcpy(names[1], "Holidays");
    return 2;
}

static void h_conf_changed(void *ctx, const SyncConf *conf)
{
    (void)ctx;
    host.conf_changes++;
    host.saved = *conf;
}

static bool h_read_only(void *ctx)
{
    (void)ctx;
    return host.read_only;
}

static uint64_t h_now_posix(void *ctx)
{
    (void)ctx;
    return 1791000000;
}

static void h_log(void *ctx, const char *line)
{
    (void)ctx;
    (void)line;
}

// ---------------------------------------------------------------- helpers

static SyncEngine engine;
static SyncConf conf;

static void setup(bool timer_writes, SyncPolicy policy)
{
    broker_reset();
    memset(&host, 0, sizeof(host));
    sync_conf_defaults(&conf);
    conf.enabled = true;
    strcpy(conf.host, "broker.lan");
    strcpy(conf.username, "parent");
    strcpy(conf.password, "secret");
    strcpy(conf.console_id, "a1b2c3d4");
    strcpy(conf.console_name, "Salon");
    conf.remote_timer_writes = timer_writes;
    conf.policy = policy;
    SyncIo io = { NULL, io_open, io_write, io_read, io_close };
    SyncHost h = { NULL, h_state, h_activity, h_order, h_report, h_profiles, h_conf_changed, h_read_only, h_now_posix, h_log };
    sync_engine_init(&engine, &conf, io, &h, clock_ms, "app", "1.1.0");
}

static void turns(int n)
{
    for (int i = 0; i < n; i++) {
        const int sleep = sync_engine_step(&engine, 50);
        now += (uint64_t)sleep;
    }
}

static const Message *last_on(const char *topic)
{
    for (size_t i = broker.n_log; i-- > 0;)
        if (!strcmp(broker.log[i].topic, topic)) return &broker.log[i];
    return NULL;
}

static size_t count_on(const char *topic)
{
    size_t n = 0;
    for (size_t i = 0; i < broker.n_log; i++) n += !strcmp(broker.log[i].topic, topic);
    return n;
}

static void ha_order(const char *entity, const char *payload)
{
    char topic[160];
    snprintf(topic, sizeof(topic), "playguard/a1b2c3d4/%s/set", entity);
    broker_publish(topic, payload, strlen(payload), true);
}

static bool retained_order(const char *entity)
{
    char topic[160];
    snprintf(topic, sizeof(topic), "playguard/a1b2c3d4/%s/set", entity);
    return find_retained(topic) != NULL;
}

static const char *last_event(void)
{
    const Message *m = last_on("playguard/a1b2c3d4/event");
    return m ? m->payload : "";
}

// ---------------------------------------------------------------- tests

static void test_connect_and_publish(void)
{
    setup(false, SyncPolicy_Auto);
    turns(3);
    assert(sync_engine_online(&engine) && engine.status.state == SyncLink_Online);
    assert(!strcmp(broker.client_id, "pg-a1b2c3d4-app"));
    assert(!strcmp(broker.username, "parent") && !strcmp(broker.password, "secret"));
    assert(broker.has_will && !strcmp(broker.will.topic, "playguard/a1b2c3d4/availability") &&
           !strcmp(broker.will.payload, "offline") && broker.will.retain);
    const Message *avail = find_retained("playguard/a1b2c3d4/availability");
    assert(avail && !strcmp(avail->payload, "online"));
    assert(broker.n_subs == 2 && !strcmp(broker.subs[0], "playguard/a1b2c3d4/+/set") && broker.sub_qos[0] == 1 &&
           !strcmp(broker.subs[1], "homeassistant/status"));
    const Message *disc = find_retained("homeassistant/device/playguard_a1b2c3d4/config");
    assert(disc && strstr(disc->payload, "\"cmps\":{") && !strstr(disc->payload, "limit_mon"));
    assert(host.profiles_asked >= 1);
    const Message *st = find_retained("playguard/a1b2c3d4/state");
    assert(st && strstr(st->payload, "\"schema\":1"));
    assert(find_retained("playguard/a1b2c3d4/activity"));

    // The state again after poll_s, not before.
    const unsigned states = host.states;
    now += 10000;
    turns(1);
    assert(host.states == states);
    now += 25000;
    turns(1);
    assert(host.states == states + 1);

    // Today's activity: every minute, or at once when the host says it changed.
    const size_t acts = count_on("playguard/a1b2c3d4/activity");
    turns(1);
    assert(count_on("playguard/a1b2c3d4/activity") == acts);
    sync_engine_activity_changed(&engine);
    turns(1);
    assert(count_on("playguard/a1b2c3d4/activity") == acts + 1);
}

static void test_orders_applied_and_refused(void)
{
    setup(false, SyncPolicy_Auto);
    turns(3);
    // Applied: an event, the retained order cleared, the state again.
    const unsigned states = host.states;
    ha_order("vr_restricted", "ON");
    turns(2);
    assert(host.orders == 1 && host.last_intent.kind == SyncIntent_Vr && host.last_intent.on);
    assert(strstr(last_event(), "\"event_type\":\"command_applied\"") && strstr(last_event(), "\"entity\":\"vr_restricted\""));
    assert(!retained_order("vr_restricted"));
    now += 400;
    turns(1);
    assert(host.states > states);
    assert(!strcmp(engine.status.last_result, "vr_restricted: applied"));

    // Out of range: refused without asking the host.
    ha_order("restriction_level", "adult");
    turns(2);
    assert(host.orders == 1 && strstr(last_event(), "command_rejected") && strstr(last_event(), "\"reason\":\"invalid\""));
    assert(!retained_order("restriction_level"));

    // A play-timer order while remote_timer_writes is off.
    ha_order("limit_mon", "90");
    now += 2000;
    turns(2);
    assert(host.orders == 1 && strstr(last_event(), "timer_writes_disabled") && !retained_order("limit_mon"));

    // Refused by the host (the console said no).
    host.answer = SyncReason_NotCustom;
    ha_order("sns_post_restricted", "OFF");
    turns(2);
    assert(host.orders == 2 && strstr(last_event(), "\"reason\":\"not_custom\"") && !retained_order("sns_post_restricted"));
    assert(engine.status.rejected == 3);

    // Read-only: refused before the host.
    host.read_only = true;
    host.answer = SyncReason_None;
    ha_order("vr_restricted", "OFF");
    turns(2);
    assert(host.orders == 2 && strstr(last_event(), "read_only"));
    host.read_only = false;

    // An order that was there before the console came back: RETAIN 1.
    sync_engine_stop(&engine);
    ha_order("vr_restricted", "ON");
    now += 10000;
    turns(3);
    assert(host.orders == 3 && host.last_retained && !retained_order("vr_restricted"));
}

static void test_policy_off(void)
{
    setup(true, SyncPolicy_Off);
    turns(3);
    ha_order("lock_now", "PRESS");
    turns(2);
    assert(host.orders == 0 && strstr(last_event(), "policy_off"));
    // Harmless orders still work.
    const size_t before = count_on("homeassistant/device/playguard_a1b2c3d4/config");
    ha_order("sync_now", "PRESS");
    turns(2);
    assert(strstr(last_event(), "command_applied"));
    assert(count_on("homeassistant/device/playguard_a1b2c3d4/config") == before + 1);
    ha_order("export_report", "PRESS");
    turns(2);
    assert(host.reports == 1 && strstr(last_event(), "command_applied"));
    assert(!find_retained("playguard/a1b2c3d4/report"));   // publish_report is off
}

static void test_limits_merged(void)
{
    setup(true, SyncPolicy_Auto);
    turns(3);
    // Two sliders moved together: one console call after the pause.
    ha_order("limit_mon", "90");
    ha_order("limit_tue", "60");
    turns(1);
    assert(host.orders == 0);
    ha_order("limit_mon", "100");   // the newest replaces the queued one, and the pause starts again
    now += 1000;
    turns(1);
    assert(host.orders == 0);
    now += 600;
    turns(1);
    assert(host.orders == 0);
    now += 1000;
    turns(2);
    assert(host.orders == 1);
    const SyncIntent *in = &host.last_intent;
    assert(in->kind == SyncIntent_LimitsWeek && in->mask == 0x06 && in->days[1] == 100 && in->days[2] == 60);
    assert(!retained_order("limit_mon") && !retained_order("limit_tue"));
    // Two events, one per order.
    size_t applied = 0;
    for (size_t i = 0; i < broker.n_log; i++)
        if (!strcmp(broker.log[i].topic, "playguard/a1b2c3d4/event") && strstr(broker.log[i].payload, "command_applied"))
            applied++;
    assert(applied == 2);
    // The discovery announces the timer entities and the profiles.
    const Message *disc = find_retained("homeassistant/device/playguard_a1b2c3d4/config");
    assert(strstr(disc->payload, "limit_mon") && strstr(disc->payload, "\"ops\":[\"School week\",\"Holidays\"]"));
}

static void test_async_and_waiting(void)
{
    setup(true, SyncPolicy_Ask);
    turns(3);
    host.async = true;
    ha_order("console_lock", "ON");
    turns(2);
    assert(host.orders == 1 && host.taken_id != 0 && sync_engine_pending(&engine) == 1);
    assert(retained_order("console_lock"));   // nothing reported yet
    SyncOutcome out;
    sync_outcome_init(&out);
    out.applied = true;
    sync_engine_order_done(&engine, host.taken_id, &out);
    assert(sync_engine_pending(&engine) == 0 && !retained_order("console_lock"));
    assert(strstr(last_event(), "command_applied"));

    // Kept waiting (nobody at the console to confirm): stays on the broker,
    // reported once even when the broker sends it again.
    host.async = false;
    host.answer = SyncReason_Waiting;
    ha_order("stop_today", "PRESS");
    turns(2);
    assert(retained_order("stop_today") && strstr(last_event(), "command_waiting"));
    const size_t events = count_on("playguard/a1b2c3d4/event");
    sync_engine_replay_orders(&engine);
    turns(2);
    assert(host.orders == 3 && count_on("playguard/a1b2c3d4/event") == events);
    // Confirmed later.
    host.answer = SyncReason_None;
    sync_engine_replay_orders(&engine);
    turns(2);
    assert(!retained_order("stop_today") && strstr(last_event(), "command_applied"));
}

static void test_home_assistant_restart_and_discovery_switch(void)
{
    setup(false, SyncPolicy_Auto);
    turns(3);
    const size_t before = count_on("homeassistant/device/playguard_a1b2c3d4/config");
    broker_publish("homeassistant/status", "online", 6, false);
    turns(2);
    assert(count_on("homeassistant/device/playguard_a1b2c3d4/config") == before + 1);
    // "offline" changes nothing.
    broker_publish("homeassistant/status", "offline", 7, false);
    turns(2);
    assert(count_on("homeassistant/device/playguard_a1b2c3d4/config") == before + 1);

    // The integration takes over: the native discovery is cleared and saved off.
    ha_order("discovery", "off");
    turns(2);
    assert(!find_retained("homeassistant/device/playguard_a1b2c3d4/config"));
    assert(host.conf_changes == 1 && !host.saved.ha_discovery);
    broker_publish("homeassistant/status", "online", 6, false);
    turns(2);
    assert(!find_retained("homeassistant/device/playguard_a1b2c3d4/config"));
    ha_order("discovery", "on");
    turns(2);
    assert(find_retained("homeassistant/device/playguard_a1b2c3d4/config") && host.saved.ha_discovery);
}

static void test_connection_failures(void)
{
    // Refused credentials: the reason, then a pause before the next attempt.
    setup(false, SyncPolicy_Auto);
    broker.refuse_code = 4;
    turns(1);
    assert(!sync_engine_online(&engine) && engine.status.state == SyncLink_Waiting);
    assert(strstr(engine.status.error, "bad user name or password"));
    const unsigned attempts = broker.connects;
    sync_engine_step(&engine, 50);
    assert(broker.connects == attempts);   // not before the pause
    broker.refuse_code = 0;
    now += 5000;
    turns(2);
    assert(sync_engine_online(&engine));

    // The broker drops the connection: its last will says offline, then the
    // engine reconnects and says online again.
    broker_drop(true);
    turns(1);
    const Message *avail = find_retained("playguard/a1b2c3d4/availability");
    assert(avail && !strcmp(avail->payload, "offline"));
    assert(!sync_engine_online(&engine));
    now += 6000;
    turns(3);
    assert(sync_engine_online(&engine));
    avail = find_retained("playguard/a1b2c3d4/availability");
    assert(!strcmp(avail->payload, "online"));

    // Unreachable: the pause doubles, up to 5 minutes.
    broker_drop(false);
    broker.up = false;
    turns(1);
    for (int i = 0; i < 12; i++) {
        now += 400000;
        turns(1);
    }
    assert(engine.backoff_ms == 300000);
    broker.up = true;
    now += 400000;
    turns(3);
    assert(sync_engine_online(&engine) && engine.backoff_ms == 0);

    // A clean stop: offline, DISCONNECT, no last will.
    const unsigned disconnects = broker.disconnects;
    sync_engine_stop(&engine);
    assert(broker.disconnects == disconnects + 1);
    avail = find_retained("playguard/a1b2c3d4/availability");
    assert(!strcmp(avail->payload, "offline"));
}

static void test_keepalive(void)
{
    setup(false, SyncPolicy_Auto);
    turns(3);
    const unsigned pings = broker.pings;
    // Nothing sent for 3/4 of the keep-alive: a PINGREQ.
    for (int i = 0; i < 30 && broker.pings == pings; i++) {
        now += 1000;
        sync_engine_step(&engine, 0);
    }
    assert(broker.pings == pings + 1 && sync_engine_online(&engine));
    // The broker stops answering: closed after the keep-alive.
    broker.answer_pings = false;
    for (int i = 0; i < 90 && sync_engine_online(&engine); i++) {
        now += 1000;
        sync_engine_step(&engine, 0);
    }
    assert(!sync_engine_online(&engine) && strstr(engine.status.error, "stopped answering"));
}

static void test_oversized_and_reconfigure(void)
{
    setup(false, SyncPolicy_Auto);
    turns(3);
    // A retained order larger than the receive buffer is dropped, the
    // connection stays.
    static char big[SYNC_RX_SIZE + 512];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    ha_order("vr_restricted", big);
    turns(3);
    assert(sync_engine_online(&engine) && engine.mqtt.dropped == 1 && host.orders == 0);
    ha_order("vr_restricted", "ON");
    turns(2);
    assert(host.orders == 1);

    // Another broker: reconnects there.
    SyncConf other = conf;
    strcpy(other.host, "other.lan");
    const unsigned connects = broker.connects;
    sync_engine_reconfigure(&engine, &other);
    turns(3);
    assert(sync_engine_online(&engine) && broker.connects == connects + 1);
    // Turned off: disconnects and stays off.
    other.enabled = false;
    sync_engine_reconfigure(&engine, &other);
    turns(2);
    assert(!sync_engine_online(&engine) && engine.status.state == SyncLink_Off);
    // Not configured: off, with the reason.
    other.enabled = true;
    other.host[0] = '\0';
    sync_engine_reconfigure(&engine, &other);
    turns(1);
    assert(engine.status.state == SyncLink_Off && strstr(engine.status.error, "host"));
}

int main(void)
{
    static Message log[MAX_LOG];
    broker.log = log;
    test_connect_and_publish();
    test_orders_applied_and_refused();
    test_policy_off();
    test_limits_merged();
    test_async_and_waiting();
    test_home_assistant_restart_and_discovery_switch();
    test_connection_failures();
    test_keepalive();
    test_oversized_and_reconfigure();
    puts("sync_engine: all tests passed");
    return 0;
}
