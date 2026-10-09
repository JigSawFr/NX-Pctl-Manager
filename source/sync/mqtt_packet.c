// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "mqtt_packet.h"

#include <string.h>

// Bytes the remaining length takes (1..4), 0 when it is too large.
static size_t varint_size(size_t n)
{
    if (n < 128u) return 1;
    if (n < 16384u) return 2;
    if (n < 2097152u) return 3;
    if (n <= MQTT_MAX_REMAINING) return 4;
    return 0;
}

static size_t put_varint(uint8_t *p, size_t n)
{
    size_t i = 0;
    do {
        uint8_t b = (uint8_t)(n % 128u);
        n /= 128u;
        if (n > 0) b |= 0x80u;
        p[i++] = b;
    } while (n > 0);
    return i;
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

// A length-prefixed string: 2 + n bytes.
static size_t put_str(uint8_t *p, const void *s, size_t n)
{
    put16(p, (uint16_t)n);
    if (n) memcpy(p + 2, s, n);
    return 2 + n;
}

// Room for a packet of `remaining` bytes after its fixed header; writes the
// header and returns its size, 0 when it does not fit.
static size_t head(uint8_t *buf, size_t cap, uint8_t first, size_t remaining)
{
    const size_t vs = varint_size(remaining);
    if (vs == 0 || cap < 1 + vs || cap - 1 - vs < remaining) return 0;
    buf[0] = first;
    return 1 + put_varint(buf + 1, remaining);
}

bool mqtt_topic_valid(const char *topic)
{
    if (!topic || !*topic) return false;
    const size_t n = strlen(topic);
    if (n > 65535u) return false;
    for (size_t i = 0; i < n; i++)
        if (topic[i] == '+' || topic[i] == '#') return false;
    return true;
}

size_t mqtt_encode_connect(uint8_t *buf, size_t cap, const MqttConnect *c)
{
    if (!c || !c->client_id) return 0;
    const size_t id_len = strlen(c->client_id);
    if (id_len > 65535u) return 0;
    const bool user = c->username != NULL;
    const bool pass = user && c->password != NULL;
    const bool will = c->will_topic != NULL;
    if (will && (!mqtt_topic_valid(c->will_topic) || c->will_qos > 1 || c->will_len > 65535u)) return 0;

    size_t remaining = 10 + 2 + id_len;   // variable header + client id
    if (will) remaining += 2 + strlen(c->will_topic) + 2 + c->will_len;
    if (user) {
        if (strlen(c->username) > 65535u) return 0;
        remaining += 2 + strlen(c->username);
    }
    if (pass) {
        if (strlen(c->password) > 65535u) return 0;
        remaining += 2 + strlen(c->password);
    }

    size_t at = head(buf, cap, (uint8_t)(MQTT_CONNECT << 4), remaining);
    if (!at) return 0;
    at += put_str(buf + at, "MQTT", 4);
    buf[at++] = 4;   // protocol level 3.1.1
    uint8_t flags = 0;
    if (c->clean_session) flags |= 0x02;
    if (will) {
        flags |= 0x04;
        flags |= (uint8_t)(c->will_qos << 3);
        if (c->will_retain) flags |= 0x20;
    }
    if (pass) flags |= 0x40;
    if (user) flags |= 0x80;
    buf[at++] = flags;
    put16(buf + at, c->keepalive_s);
    at += 2;
    at += put_str(buf + at, c->client_id, id_len);
    if (will) {
        at += put_str(buf + at, c->will_topic, strlen(c->will_topic));
        at += put_str(buf + at, c->will_payload, c->will_len);
    }
    if (user) at += put_str(buf + at, c->username, strlen(c->username));
    if (pass) at += put_str(buf + at, c->password, strlen(c->password));
    return at;
}

size_t mqtt_encode_publish_head(uint8_t *buf, size_t cap, const char *topic, size_t payload_len,
                                uint8_t qos, bool retain, bool dup, uint16_t packet_id)
{
    if (!mqtt_topic_valid(topic) || qos > 1) return 0;
    if (qos == 1 && packet_id == 0) return 0;
    const size_t topic_len = strlen(topic);
    const size_t vh = 2 + topic_len + (qos ? 2u : 0u);
    if (payload_len > MQTT_MAX_REMAINING - vh) return 0;
    const size_t remaining = vh + payload_len;
    const size_t vs = varint_size(remaining);
    if (vs == 0 || cap < 1 + vs + vh) return 0;
    uint8_t first = (uint8_t)(MQTT_PUBLISH << 4);
    if (dup) first |= 0x08;
    first |= (uint8_t)(qos << 1);
    if (retain) first |= 0x01;
    buf[0] = first;
    size_t at = 1 + put_varint(buf + 1, remaining);
    at += put_str(buf + at, topic, topic_len);
    if (qos) {
        put16(buf + at, packet_id);
        at += 2;
    }
    return at;
}

size_t mqtt_encode_puback(uint8_t *buf, size_t cap, uint16_t packet_id)
{
    if (cap < 4) return 0;
    buf[0] = (uint8_t)(MQTT_PUBACK << 4);
    buf[1] = 2;
    put16(buf + 2, packet_id);
    return 4;
}

size_t mqtt_encode_subscribe(uint8_t *buf, size_t cap, uint16_t packet_id, const char *const *topics,
                             const uint8_t *qos, size_t count)
{
    if (count == 0 || packet_id == 0 || !topics) return 0;
    size_t remaining = 2;
    for (size_t i = 0; i < count; i++) {
        if (!topics[i] || !*topics[i]) return 0;
        const size_t n = strlen(topics[i]);
        if (n > 65535u || (qos && qos[i] > 1)) return 0;
        remaining += 2 + n + 1;
    }
    // SUBSCRIBE carries the reserved flags 0010.
    size_t at = head(buf, cap, (uint8_t)((MQTT_SUBSCRIBE << 4) | 0x02), remaining);
    if (!at) return 0;
    put16(buf + at, packet_id);
    at += 2;
    for (size_t i = 0; i < count; i++) {
        at += put_str(buf + at, topics[i], strlen(topics[i]));
        buf[at++] = qos ? qos[i] : 0;
    }
    return at;
}

size_t mqtt_encode_empty(uint8_t *buf, size_t cap, MqttType type)
{
    if (cap < 2 || (type != MQTT_PINGREQ && type != MQTT_DISCONNECT)) return 0;
    buf[0] = (uint8_t)(type << 4);
    buf[1] = 0;
    return 2;
}

MqttResult mqtt_parse(const uint8_t *buf, size_t len, size_t max_size, MqttPacket *out)
{
    memset(out, 0, sizeof(*out));
    if (len < 2) return MQTT_INCOMPLETE;
    size_t remaining = 0, mult = 1, i = 1;
    for (;;) {
        if (i >= len) return MQTT_INCOMPLETE;
        if (i > 4) return MQTT_MALFORMED;   // a fifth length byte
        const uint8_t b = buf[i++];
        remaining += (size_t)(b & 0x7Fu) * mult;
        if (!(b & 0x80u)) break;
        mult *= 128u;
    }
    const uint8_t type = buf[0] >> 4;
    if (type == 0 || type == 15) return MQTT_MALFORMED;
    if (remaining > max_size || i + remaining > max_size) return MQTT_TOO_LARGE;
    if (len - i < remaining) return MQTT_INCOMPLETE;
    out->type = type;
    out->flags = buf[0] & 0x0Fu;
    out->body = buf + i;
    out->body_len = remaining;
    out->size = i + remaining;
    return MQTT_OK;
}

MqttResult mqtt_decode_connack(const MqttPacket *p, MqttConnack *out)
{
    if (p->type != MQTT_CONNACK || p->flags != 0 || p->body_len != 2) return MQTT_MALFORMED;
    if (p->body[0] & 0xFEu) return MQTT_MALFORMED;
    out->session_present = p->body[0] & 1u;
    out->return_code = p->body[1];
    return MQTT_OK;
}

MqttResult mqtt_decode_publish(const MqttPacket *p, MqttPublish *out)
{
    memset(out, 0, sizeof(*out));
    if (p->type != MQTT_PUBLISH) return MQTT_MALFORMED;
    const uint8_t qos = (p->flags >> 1) & 3u;
    if (qos > 2) return MQTT_MALFORMED;
    if (p->body_len < 2) return MQTT_MALFORMED;
    const size_t topic_len = get16(p->body);
    const size_t vh = 2 + topic_len + (qos ? 2u : 0u);
    if (topic_len == 0 || vh > p->body_len) return MQTT_MALFORMED;
    const char *topic = (const char *)p->body + 2;
    for (size_t i = 0; i < topic_len; i++)
        if (topic[i] == '\0' || topic[i] == '+' || topic[i] == '#') return MQTT_MALFORMED;
    out->topic = topic;
    out->topic_len = topic_len;
    out->qos = qos;
    out->retain = p->flags & 1u;
    out->dup = (p->flags & 8u) != 0;
    if (qos) {
        out->packet_id = get16(p->body + 2 + topic_len);
        if (out->packet_id == 0) return MQTT_MALFORMED;
    }
    out->payload = p->body + vh;
    out->payload_len = p->body_len - vh;
    return MQTT_OK;
}

MqttResult mqtt_decode_puback(const MqttPacket *p, uint16_t *packet_id)
{
    if (p->type != MQTT_PUBACK || p->flags != 0 || p->body_len != 2) return MQTT_MALFORMED;
    *packet_id = get16(p->body);
    return MQTT_OK;
}

MqttResult mqtt_decode_suback(const MqttPacket *p, uint16_t *packet_id, uint8_t *codes, size_t max, size_t *count)
{
    *count = 0;
    if (p->type != MQTT_SUBACK || p->flags != 0 || p->body_len < 3) return MQTT_MALFORMED;
    *packet_id = get16(p->body);
    const size_t n = p->body_len - 2;
    for (size_t i = 0; i < n; i++) {
        const uint8_t c = p->body[2 + i];
        if (c > 2 && c != 0x80u) return MQTT_MALFORMED;
        if (i < max) codes[i] = c;
    }
    *count = n < max ? n : max;
    return MQTT_OK;
}

const char *mqtt_connack_text(uint8_t rc)
{
    switch (rc) {
        case 0: return "accepted";
        case 1: return "the broker does not speak MQTT 3.1.1";
        case 2: return "client identifier refused";
        case 3: return "broker unavailable";
        case 4: return "bad user name or password";
        case 5: return "not authorised";
        default: return "refused";
    }
}

bool mqtt_topic_matches(const char *filter, const char *topic, size_t len)
{
    size_t t = 0;
    const char *f = filter;
    for (;;) {
        if (*f == '#') return f[1] == '\0';   // matches the rest, the parent level included
        // One level of the filter against one level of the topic.
        const char *f_end = f;
        while (*f_end && *f_end != '/') f_end++;
        size_t t_end = t;
        while (t_end < len && topic[t_end] != '/') t_end++;
        const size_t f_n = (size_t)(f_end - f);
        if (!(f_n == 1 && *f == '+')) {
            if (f_n != t_end - t || memcmp(f, topic + t, f_n) != 0) return false;
        }
        const bool f_more = *f_end == '/';
        const bool t_more = t_end < len;
        if (!f_more && !t_more) return true;
        if (!f_more) return false;
        f = f_end + 1;
        if (!t_more) return *f == '#' && f[1] == '\0';   // "a/#" matches "a"
        t = t_end + 1;
    }
}
