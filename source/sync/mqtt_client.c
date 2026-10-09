// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "mqtt_client.h"

#include <stdio.h>
#include <string.h>

#define WRITE_TIMEOUT_MS 5000

void mqtt_client_init(MqttClient *c, SyncIo io, uint8_t *rx, size_t rx_cap, uint64_t (*now_ms)(void),
                      MqttOnMessage on_message, void *user)
{
    memset(c, 0, sizeof(*c));
    c->io = io;
    c->rx = rx;
    c->rx_cap = rx_cap;
    c->now_ms = now_ms;
    c->on_message = on_message;
    c->user = user;
    c->next_id = 1;
}

static int fail(MqttClient *c, const char *why)
{
    if (why) snprintf(c->error, sizeof(c->error), "%s", why);
    if (c->open && c->io.close) c->io.close(c->io.ctx);
    c->open = false;
    c->connected = false;
    c->rx_len = 0;
    c->skip = 0;
    c->ping_sent_ms = 0;
    return -1;
}

static int send_bytes(MqttClient *c, const void *buf, size_t len)
{
    if (!c->open) return -1;
    if (c->io.write(c->io.ctx, buf, len, WRITE_TIMEOUT_MS) < 0) return fail(c, "the connection was lost while sending");
    c->last_out_ms = c->now_ms();
    return 0;
}

static uint16_t take_id(MqttClient *c)
{
    const uint16_t id = c->next_id++;
    if (c->next_id == 0) c->next_id = 1;
    return id;
}

// What a packet just received asks for. `want` / `want_id`: the packet a
// caller waits for (CONNACK, SUBACK); *got is set when it arrived.
static int handle(MqttClient *c, const MqttPacket *p, uint8_t want, uint16_t want_id, bool *got, MqttPacket *got_packet)
{
    switch (p->type) {
    case MQTT_PUBLISH: {
        MqttPublish msg;
        if (mqtt_decode_publish(p, &msg) != MQTT_OK) return fail(c, "the broker sent a malformed message");
        if (c->on_message) c->on_message(c->user, &msg);
        if (msg.qos == 1) {
            const size_t n = mqtt_encode_puback(c->tx, sizeof(c->tx), msg.packet_id);
            if (send_bytes(c, c->tx, n) < 0) return -1;
        }
        // QoS 2 is never asked for; a broker that sends it anyway gets no
        // PUBREC and will resend: the message is still delivered once here.
        return 0;
    }
    case MQTT_PINGRESP:
        c->ping_sent_ms = 0;
        return 0;
    case MQTT_CONNACK:
    case MQTT_SUBACK:
    case MQTT_PUBACK:
    case MQTT_UNSUBACK:
        if (p->type == want) {
            uint16_t id = 0;
            if (p->type == MQTT_SUBACK && p->body_len >= 2) id = (uint16_t)((p->body[0] << 8) | p->body[1]);
            if (want != MQTT_SUBACK || id == want_id) {
                *got = true;
                *got_packet = *p;
            }
        } else if (p->type == MQTT_CONNACK) {
            return fail(c, "the broker sent a second CONNACK");
        }
        return 0;
    default:
        return fail(c, "the broker sent a packet a client never receives");
    }
}

// Reads what is there (waiting up to `timeout_ms` for the first bytes) and
// handles every whole packet. Stops early once `want` arrived; the packet is
// then copied out before the buffer moves on.
static int pump(MqttClient *c, int timeout_ms, uint8_t want, uint16_t want_id, bool *got, uint8_t *copy,
                size_t copy_cap, size_t *copy_len)
{
    if (!c->open) return -1;
    if (c->rx_len == c->rx_cap && c->skip == 0) return fail(c, "receive buffer full");
    const long n = c->io.read(c->io.ctx, c->rx + c->rx_len, c->rx_cap - c->rx_len, timeout_ms);
    if (n < 0) return fail(c, "the broker closed the connection");
    c->rx_len += (size_t)n;

    // Drop the rest of a message too large to hold.
    if (c->skip) {
        const size_t d = c->skip < c->rx_len ? c->skip : c->rx_len;
        memmove(c->rx, c->rx + d, c->rx_len - d);
        c->rx_len -= d;
        c->skip -= d;
    }

    while (c->rx_len > 0 && c->skip == 0) {
        MqttPacket p;
        const MqttResult r = mqtt_parse(c->rx, c->rx_len, c->rx_cap, &p);
        if (r == MQTT_INCOMPLETE) break;
        if (r == MQTT_MALFORMED) return fail(c, "the broker sent a malformed packet");
        if (r == MQTT_TOO_LARGE) {
            // Its whole size, from its header (the length bytes are there).
            size_t remaining = 0, mult = 1, i = 1;
            for (; i < c->rx_len && i <= 4; i++) {
                remaining += (size_t)(c->rx[i] & 0x7Fu) * mult;
                if (!(c->rx[i] & 0x80u)) {
                    i++;
                    break;
                }
                mult *= 128u;
            }
            c->skip = i + remaining;
            c->dropped++;
            const size_t d = c->skip < c->rx_len ? c->skip : c->rx_len;
            memmove(c->rx, c->rx + d, c->rx_len - d);
            c->rx_len -= d;
            c->skip -= d;
            continue;
        }
        bool here = false;
        MqttPacket wanted;
        if (handle(c, &p, want, want_id, &here, &wanted) < 0) return -1;
        if (here && copy) {
            const size_t m = wanted.body_len < copy_cap ? wanted.body_len : copy_cap;
            memcpy(copy, wanted.body, m);
            *copy_len = m;
        }
        memmove(c->rx, c->rx + p.size, c->rx_len - p.size);
        c->rx_len -= p.size;
        if (here) {
            *got = true;
            break;
        }
    }
    return 0;
}

// Waits for `want` (with id `want_id` for a SUBACK) for up to `timeout_ms`.
static int await(MqttClient *c, uint8_t want, uint16_t want_id, int timeout_ms, uint8_t *body, size_t cap, size_t *len)
{
    const uint64_t until = c->now_ms() + (uint64_t)(timeout_ms > 0 ? timeout_ms : 0);
    bool got = false;
    while (!got) {
        const uint64_t now = c->now_ms();
        if (now >= until) return fail(c, "the broker did not answer in time");
        const uint64_t left = until - now;
        if (pump(c, (int)(left > 250 ? 250 : left), want, want_id, &got, body, cap, len) < 0) return -1;
    }
    return 0;
}

int mqtt_client_connect(MqttClient *c, const char *host, uint16_t port, const MqttConnect *opts, int timeout_ms)
{
    if (c->open) mqtt_client_close(c, false);
    c->error[0] = '\0';
    c->rx_len = 0;
    c->skip = 0;
    c->ping_sent_ms = 0;
    if (c->io.open(c->io.ctx, host, port, timeout_ms, c->error, sizeof(c->error)) < 0) {
        if (!c->error[0]) snprintf(c->error, sizeof(c->error), "cannot reach the broker");
        return -1;
    }
    c->open = true;
    const size_t n = mqtt_encode_connect(c->tx, sizeof(c->tx), opts);
    if (n == 0) return fail(c, "the connection settings are too long");
    if (send_bytes(c, c->tx, n) < 0) return -1;
    uint8_t body[2];
    size_t len = 0;
    if (await(c, MQTT_CONNACK, 0, timeout_ms, body, sizeof(body), &len) < 0) return -1;
    if (len != 2) return fail(c, "the broker sent a malformed CONNACK");
    if (body[1] != 0) {
        char why[96];
        snprintf(why, sizeof(why), "the broker refused the connection: %s", mqtt_connack_text(body[1]));
        return fail(c, why);
    }
    c->keepalive_s = opts->keepalive_s;
    c->connected = true;
    return 0;
}

int mqtt_client_subscribe(MqttClient *c, const char *const *topics, const uint8_t *qos, size_t count, int timeout_ms)
{
    if (!c->connected) return -1;
    const uint16_t id = take_id(c);
    const size_t n = mqtt_encode_subscribe(c->tx, sizeof(c->tx), id, topics, qos, count);
    if (n == 0) return fail(c, "the subscription does not fit");
    if (send_bytes(c, c->tx, n) < 0) return -1;
    uint8_t body[2 + 16];
    size_t len = 0;
    if (await(c, MQTT_SUBACK, id, timeout_ms, body, sizeof(body), &len) < 0) return -1;
    if (len < 2 + count) return fail(c, "the broker sent a malformed SUBACK");
    for (size_t i = 0; i < count; i++)
        if (body[2 + i] == 0x80u) return fail(c, "the broker refused a subscription (check the user's access list)");
    return 0;
}

int mqtt_client_publish(MqttClient *c, const char *topic, const void *payload, size_t len, bool retain)
{
    if (!c->connected) return -1;
    const size_t n = mqtt_encode_publish_head(c->tx, sizeof(c->tx), topic, len, 0, retain, false, 0);
    if (n == 0) return -1;   // a topic that cannot be published to: the connection is fine
    if (send_bytes(c, c->tx, n) < 0) return -1;
    if (len && send_bytes(c, payload, len) < 0) return -1;
    return 0;
}

int mqtt_client_poll(MqttClient *c, int timeout_ms)
{
    if (!c->connected) return -1;
    const uint64_t now = c->now_ms();
    if (c->keepalive_s) {
        const uint64_t period = (uint64_t)c->keepalive_s * 1000u;
        if (c->ping_sent_ms && now - c->ping_sent_ms > period) return fail(c, "the broker stopped answering");
        // A PINGREQ well before the keep-alive runs out, so a slow answer
        // still arrives in time.
        if (!c->ping_sent_ms && now - c->last_out_ms >= period * 3 / 4) {
            const size_t n = mqtt_encode_empty(c->tx, sizeof(c->tx), MQTT_PINGREQ);
            if (send_bytes(c, c->tx, n) < 0) return -1;
            c->ping_sent_ms = c->now_ms();
        }
    }
    bool got = false;
    return pump(c, timeout_ms, 0, 0, &got, NULL, 0, NULL);
}

void mqtt_client_close(MqttClient *c, bool send_disconnect)
{
    if (c->open && c->connected && send_disconnect) {
        const size_t n = mqtt_encode_empty(c->tx, sizeof(c->tx), MQTT_DISCONNECT);
        c->io.write(c->io.ctx, c->tx, n, 1000);
    }
    if (c->open && c->io.close) c->io.close(c->io.ctx);
    c->open = false;
    c->connected = false;
    c->rx_len = 0;
    c->skip = 0;
    c->ping_sent_ms = 0;
}
