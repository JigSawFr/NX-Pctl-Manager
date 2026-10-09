// Host tests for the remote link's console-free parts (source/sync/): the
// JSON writer, the MQTT 3.1.1 codec, sync.conf, the order parser and its
// agreement with the entity table, the Home Assistant discovery and the
// state / activity / event documents. Writes the discovery and a state to
// files that `make check` validates as JSON (tools/check_sync_json.py).
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mqtt_packet.h"
#include "sync_apply.h"
#include "sync_conf.h"
#include "sync_discovery.h"
#include "sync_entities.h"
#include "sync_exec.h"
#include "sync_json.h"
#include "sync_state.h"

static const char *out_dir = ".";

static bool contains(const char *hay, const char *needle)
{
    return strstr(hay, needle) != NULL;
}

static void save(const char *name, const char *text)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", out_dir, name);
    // Owner-writable only (fopen would create it 0666 under the umask).
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);
    FILE *f = fdopen(fd, "wb");
    assert(f);
    fputs(text, f);
    fclose(f);
}

// ---------------------------------------------------------------- JSON

static void test_json(void)
{
    char buf[256];
    SyncJson j;
    sync_json_init(&j, buf, sizeof(buf));
    sync_json_obj(&j);
    sync_json_kstr(&j, "a", "x\"y\\z\n\t\x01");
    sync_json_kint(&j, "n", -42);
    sync_json_kbool(&j, "t", true);
    sync_json_knull(&j, "z");
    sync_json_key(&j, "arr");
    sync_json_arr(&j);
    sync_json_int(&j, 1);
    sync_json_obj(&j);
    sync_json_obj_end(&j);
    sync_json_hex64(&j, 0x0100000000010000ULL);
    sync_json_arr_end(&j);
    sync_json_kint_or_null(&j, "maybe", false, 5);
    sync_json_members(&j, "\"m\":1,\"k\":2");
    sync_json_obj_end(&j);
    const size_t n = sync_json_end(&j);
    assert(n == strlen(buf));
    assert(!strcmp(buf, "{\"a\":\"x\\\"y\\\\z\\n\\t\\u0001\",\"n\":-42,\"t\":true,\"z\":null,"
                        "\"arr\":[1,{},\"0100000000010000\"],\"maybe\":null,\"m\":1,\"k\":2}"));

    // Too small: 0, and the buffer stays a valid C string.
    char small[8];
    sync_json_init(&j, small, sizeof(small));
    sync_json_obj(&j);
    sync_json_kstr(&j, "long", "value");
    sync_json_obj_end(&j);
    assert(sync_json_end(&j) == 0);
    assert(strlen(small) < sizeof(small));

    // Unbalanced: 0.
    sync_json_init(&j, buf, sizeof(buf));
    sync_json_obj(&j);
    assert(sync_json_end(&j) == 0);
    sync_json_init(&j, buf, sizeof(buf));
    sync_json_obj_end(&j);
    assert(sync_json_end(&j) == 0);
}

// ---------------------------------------------------------------- MQTT

static void test_connect_encoding(void)
{
    uint8_t buf[256];
    MqttConnect c = { 0 };
    c.client_id = "pg-a1b2c3d4-app";
    c.username = "user";
    c.password = "pass";
    c.will_topic = "playguard/a1b2c3d4/availability";
    c.will_payload = "offline";
    c.will_len = 7;
    c.will_retain = true;
    c.keepalive_s = 30;
    c.clean_session = true;
    const size_t n = mqtt_encode_connect(buf, sizeof(buf), &c);
    assert(n > 0);
    assert(buf[0] == 0x10);
    MqttPacket p;
    assert(mqtt_parse(buf, n, sizeof(buf), &p) == MQTT_OK);
    assert(p.type == MQTT_CONNECT && p.size == n);
    const uint8_t *b = p.body;
    assert(b[0] == 0 && b[1] == 4 && !memcmp(b + 2, "MQTT", 4) && b[6] == 4);
    // user, password, will retain, will QoS 0, will, clean session
    assert(b[7] == (0x80 | 0x40 | 0x20 | 0x04 | 0x02));
    assert(b[8] == 0 && b[9] == 30);
    size_t at = 10;
    assert(b[at] == 0 && b[at + 1] == strlen(c.client_id) && !memcmp(b + at + 2, c.client_id, strlen(c.client_id)));
    at += 2 + strlen(c.client_id);
    assert(b[at + 1] == strlen(c.will_topic));
    at += 2 + strlen(c.will_topic);
    assert(b[at + 1] == 7 && !memcmp(b + at + 2, "offline", 7));
    at += 9;
    assert(b[at + 1] == 4 && !memcmp(b + at + 2, "user", 4));
    at += 6;
    assert(b[at + 1] == 4 && !memcmp(b + at + 2, "pass", 4));
    at += 6;
    assert(at == p.body_len);

    // No user: no password either; buffer too small: 0.
    c.username = NULL;
    const size_t n2 = mqtt_encode_connect(buf, sizeof(buf), &c);
    assert(n2 > 0 && (buf[9] & 0xC0) == 0);   // connect flags: no user, no password
    assert(mqtt_encode_connect(buf, 10, &c) == 0);
    // A will topic with a wildcard is refused.
    c.will_topic = "a/#";
    assert(mqtt_encode_connect(buf, sizeof(buf), &c) == 0);
}

static void test_publish_roundtrip(void)
{
    uint8_t buf[512];
    const char *payload = "{\"x\":1}";
    const size_t h = mqtt_encode_publish_head(buf, sizeof(buf), "playguard/id/state", strlen(payload), 0, true, false, 0);
    assert(h > 0);
    memcpy(buf + h, payload, strlen(payload));
    MqttPacket p;
    assert(mqtt_parse(buf, h + strlen(payload), sizeof(buf), &p) == MQTT_OK);
    MqttPublish m;
    assert(mqtt_decode_publish(&p, &m) == MQTT_OK);
    assert(m.retain && m.qos == 0 && !m.dup);
    assert(m.topic_len == strlen("playguard/id/state") && !memcmp(m.topic, "playguard/id/state", m.topic_len));
    assert(m.payload_len == strlen(payload) && !memcmp(m.payload, payload, m.payload_len));

    // QoS 1 carries a packet id (and refuses 0); QoS 2 is not sent.
    assert(mqtt_encode_publish_head(buf, sizeof(buf), "t", 0, 1, false, false, 0) == 0);
    const size_t h1 = mqtt_encode_publish_head(buf, sizeof(buf), "t", 0, 1, false, true, 0x1234);
    assert(h1 > 0 && (buf[0] & 0x0F) == 0x0A);
    assert(mqtt_parse(buf, h1, sizeof(buf), &p) == MQTT_OK && mqtt_decode_publish(&p, &m) == MQTT_OK);
    assert(m.qos == 1 && m.dup && m.packet_id == 0x1234 && m.payload_len == 0);
    assert(mqtt_encode_publish_head(buf, sizeof(buf), "t", 0, 2, false, false, 1) == 0);
    // Wildcards and empty topics cannot be published to.
    assert(mqtt_encode_publish_head(buf, sizeof(buf), "a/+/b", 0, 0, false, false, 0) == 0);
    assert(mqtt_encode_publish_head(buf, sizeof(buf), "a/#", 0, 0, false, false, 0) == 0);
    assert(mqtt_encode_publish_head(buf, sizeof(buf), "", 0, 0, false, false, 0) == 0);
    // An empty retained payload (clears a retained message).
    const size_t he = mqtt_encode_publish_head(buf, sizeof(buf), "a/b/set", 0, 0, true, false, 0);
    assert(he == 2 + 2 + 7 && buf[1] == 9);
}

static void test_remaining_length(void)
{
    // The boundaries of the variable length: 127/128, 16383/16384, 2097151/2097152.
    static uint8_t big[2100000 + 16];
    const size_t sizes[] = { 0, 1, 127 - 4, 128 - 4, 16383 - 4, 16384 - 4, 2097151 - 4, 2097152 - 4 };
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        const size_t h = mqtt_encode_publish_head(big, sizeof(big), "ab", sizes[i], 0, false, false, 0);
        assert(h > 0);
        const size_t remaining = 2 + 2 + sizes[i];
        const size_t len_bytes = remaining < 128 ? 1 : remaining < 16384 ? 2 : remaining < 2097152 ? 3 : 4;
        assert(h == 1 + len_bytes + 4);
        memset(big + h, 'x', sizes[i]);
        MqttPacket p;
        assert(mqtt_parse(big, h + sizes[i], sizeof(big), &p) == MQTT_OK);
        assert(p.body_len == remaining && p.size == h + sizes[i]);
        // One byte short: incomplete.
        if (h + sizes[i] > 2) assert(mqtt_parse(big, h + sizes[i] - 1, sizeof(big), &p) == MQTT_INCOMPLETE);
        // Over the caller's limit: too large (decided from the header alone).
        if (h + sizes[i] > 64) assert(mqtt_parse(big, h, 64, &p) == MQTT_TOO_LARGE);
    }
    // A fifth length byte is malformed; reserved types 0 and 15 too.
    const uint8_t five[] = { 0x30, 0xFF, 0xFF, 0xFF, 0xFF, 0x01 };
    MqttPacket p;
    assert(mqtt_parse(five, sizeof(five), 1u << 30, &p) == MQTT_MALFORMED);
    const uint8_t zero[] = { 0x00, 0x00 };
    assert(mqtt_parse(zero, 2, 64, &p) == MQTT_MALFORMED);
    const uint8_t fifteen[] = { 0xF0, 0x00 };
    assert(mqtt_parse(fifteen, 2, 64, &p) == MQTT_MALFORMED);
    // Fewer than two bytes, or a length still running: incomplete.
    assert(mqtt_parse(zero, 1, 64, &p) == MQTT_INCOMPLETE);
    const uint8_t running[] = { 0x30, 0x80 };
    assert(mqtt_parse(running, 2, 64, &p) == MQTT_INCOMPLETE);
}

static void test_other_packets(void)
{
    uint8_t buf[256];
    const char *topics[] = { "playguard/id/+/set", "homeassistant/status" };
    const uint8_t qos[] = { 1, 0 };
    const size_t n = mqtt_encode_subscribe(buf, sizeof(buf), 7, topics, qos, 2);
    assert(n > 0 && buf[0] == 0x82);
    MqttPacket p;
    assert(mqtt_parse(buf, n, sizeof(buf), &p) == MQTT_OK && p.type == MQTT_SUBSCRIBE);
    assert(p.body[0] == 0 && p.body[1] == 7);
    assert(p.body[2] == 0 && p.body[3] == strlen(topics[0]));
    assert(p.body[4 + strlen(topics[0])] == 1);
    assert(mqtt_encode_subscribe(buf, sizeof(buf), 0, topics, qos, 2) == 0);   // id 0
    assert(mqtt_encode_subscribe(buf, sizeof(buf), 1, topics, qos, 0) == 0);   // nothing

    const uint8_t suback[] = { 0x90, 0x04, 0x00, 0x07, 0x01, 0x80 };
    assert(mqtt_parse(suback, sizeof(suback), 64, &p) == MQTT_OK);
    uint16_t id = 0;
    uint8_t codes[4];
    size_t count = 0;
    assert(mqtt_decode_suback(&p, &id, codes, 4, &count) == MQTT_OK);
    assert(id == 7 && count == 2 && codes[0] == 1 && codes[1] == 0x80);
    const uint8_t bad_suback[] = { 0x90, 0x03, 0x00, 0x07, 0x03 };
    assert(mqtt_parse(bad_suback, sizeof(bad_suback), 64, &p) == MQTT_OK);
    assert(mqtt_decode_suback(&p, &id, codes, 4, &count) == MQTT_MALFORMED);

    const uint8_t connack[] = { 0x20, 0x02, 0x01, 0x05 };
    MqttConnack ca;
    assert(mqtt_parse(connack, 4, 64, &p) == MQTT_OK && mqtt_decode_connack(&p, &ca) == MQTT_OK);
    assert(ca.session_present && ca.return_code == 5);
    assert(!strcmp(mqtt_connack_text(5), "not authorised"));
    assert(!strcmp(mqtt_connack_text(4), "bad user name or password"));
    const uint8_t bad_connack[] = { 0x20, 0x02, 0x02, 0x00 };
    assert(mqtt_parse(bad_connack, 4, 64, &p) == MQTT_OK && mqtt_decode_connack(&p, &ca) == MQTT_MALFORMED);

    assert(mqtt_encode_puback(buf, sizeof(buf), 0xBEEF) == 4);
    assert(mqtt_parse(buf, 4, 64, &p) == MQTT_OK);
    assert(mqtt_decode_puback(&p, &id) == MQTT_OK && id == 0xBEEF);
    assert(mqtt_encode_empty(buf, sizeof(buf), MQTT_PINGREQ) == 2 && buf[0] == 0xC0 && buf[1] == 0);
    assert(mqtt_encode_empty(buf, sizeof(buf), MQTT_DISCONNECT) == 2 && buf[0] == 0xE0);
    assert(mqtt_encode_empty(buf, sizeof(buf), MQTT_PUBLISH) == 0);

    // A PUBLISH whose topic runs past the packet, or holds a wildcard / NUL.
    const uint8_t cut[] = { 0x30, 0x03, 0x00, 0x05, 'a' };
    assert(mqtt_parse(cut, sizeof(cut), 64, &p) == MQTT_OK);
    MqttPublish m;
    assert(mqtt_decode_publish(&p, &m) == MQTT_MALFORMED);
    const uint8_t wild[] = { 0x30, 0x04, 0x00, 0x02, 'a', '#' };
    assert(mqtt_parse(wild, sizeof(wild), 64, &p) == MQTT_OK && mqtt_decode_publish(&p, &m) == MQTT_MALFORMED);
    const uint8_t nul[] = { 0x30, 0x04, 0x00, 0x02, 'a', 0 };
    assert(mqtt_parse(nul, sizeof(nul), 64, &p) == MQTT_OK && mqtt_decode_publish(&p, &m) == MQTT_MALFORMED);
    // QoS 1 with packet id 0.
    const uint8_t qos1_zero[] = { 0x32, 0x05, 0x00, 0x01, 'a', 0x00, 0x00 };
    assert(mqtt_parse(qos1_zero, sizeof(qos1_zero), 64, &p) == MQTT_OK && mqtt_decode_publish(&p, &m) == MQTT_MALFORMED);
}

static void test_topic_matching(void)
{
    assert(mqtt_topic_matches("a/+/set", "a/limit_mon/set", 15));
    assert(!mqtt_topic_matches("a/+/set", "a/x/y/set", 9));
    assert(mqtt_topic_matches("a/#", "a", 1));
    assert(mqtt_topic_matches("a/#", "a/b/c", 5));
    assert(mqtt_topic_matches("#", "x/y", 3));
    assert(!mqtt_topic_matches("a/b", "a/bc", 4));
    assert(mqtt_topic_matches("a/+", "a/", 2));
    assert(!mqtt_topic_matches("a/+", "a", 1));
    assert(mqtt_topic_valid("a/b") && !mqtt_topic_valid("a/+") && !mqtt_topic_valid(""));
}

// ---------------------------------------------------------------- sync.conf

static void test_conf(void)
{
    SyncConf c;
    sync_conf_defaults(&c);
    assert(c.port == 1883 && c.policy == SyncPolicy_Ask && c.ha_discovery && !c.remote_timer_writes);
    assert(!strcmp(c.topic_prefix, "playguard") && !strcmp(c.discovery_prefix, "homeassistant"));
    assert(sync_conf_problem(&c) != NULL);   // no host

    const char *text = "\xEF\xBB\xBF# comment\r\n"
                       "enabled=1\r\n"
                       "  host = broker.lan\n"           // spaces around the key, not the value
                       "port=8883\n"
                       "tls=on\n"
                       "username=parent\n"
                       "password= p=a ss#word\n"         // kept verbatim after '='
                       "console_id=a1b2c3d4\n"
                       "console_name=Salon\n"
                       "policy=auto\n"
                       "remote_timer_writes=yes\n"
                       "poll_s=5\n"                      // out of range: default kept
                       "topic_prefix=bad/prefix\n"       // refused
                       "future_key=keep me\n"
                       "noequals\n"
                       "=novalue\n";
    sync_conf_defaults(&c);
    assert(sync_conf_parse(&c, text, strlen(text)));
    assert(c.enabled && c.port == 8883 && c.tls);
    assert(!strcmp(c.host, " broker.lan"));
    assert(!strcmp(c.password, " p=a ss#word"));
    assert(!strcmp(c.console_id, "a1b2c3d4") && !strcmp(c.console_name, "Salon"));
    assert(c.policy == SyncPolicy_Auto && c.remote_timer_writes);
    assert(c.poll_s == 30 && !strcmp(c.topic_prefix, "playguard"));
    assert(contains(c.extra, "future_key=keep me\n"));
    assert(sync_conf_problem(&c) == NULL);

    // Round trip, unknown keys kept.
    char out[2048];
    const size_t n = sync_conf_write(&c, out, sizeof(out));
    assert(n > 0 && contains(out, "future_key=keep me\n") && contains(out, "policy=auto\n"));
    SyncConf back;
    sync_conf_defaults(&back);
    assert(sync_conf_parse(&back, out, n));
    assert(!memcmp(&back, &c, sizeof(c)));
    assert(sync_conf_write(&c, out, 64) == 0);

    // What is still missing.
    SyncConf m = c;
    m.console_id[0] = '\0';
    assert(sync_conf_problem(&m) && contains(sync_conf_problem(&m), "console id"));
    m = c;
    m.username[0] = '\0';
    assert(sync_conf_problem(&m));
    m.allow_anonymous = true;
    assert(sync_conf_problem(&m) == NULL);
    m = c;
    strcpy(m.console_name, "a\nb");
    assert(sync_conf_problem(&m));

    assert(sync_conf_id_valid("0123abcd") && !sync_conf_id_valid("0123ABCD") && !sync_conf_id_valid("123"));
    uint32_t u;
    assert(sync_parse_uint("300", 10, 300, &u) && u == 300);
    assert(!sync_parse_uint("301", 10, 300, &u) && !sync_parse_uint("-1", 0, 9, &u) && !sync_parse_uint("", 0, 9, &u));
    assert(!sync_parse_uint("99999999999", 0, 0xFFFFFFFFu, &u));
    bool b;
    assert(sync_parse_bool("TRUE", &b) && b && sync_parse_bool("off", &b) && !b && !sync_parse_bool("maybe", &b));
}

static void test_records(void)
{
    SyncRecords r;
    sync_records_clear(&r);
    assert(r.extra_weekday == -1);
    r.extra_weekday = 3;
    strcpy(r.extra_date, "2026-10-08");
    r.extra_base = 120;
    r.extra_value = 150;
    r.console_lock = true;
    r.console_lock_prev_ok = true;
    const uint16_t prev[7] = { 60, 90, 0xFFFF, 120, 0, 30, 45 };
    memcpy(r.console_lock_prev, prev, sizeof(prev));
    r.relock_pending = true;
    char text[512];
    const size_t n = sync_records_write(&r, text, sizeof(text));
    assert(n > 0);
    SyncRecords back;
    sync_records_parse(&back, text, n);
    assert(!memcmp(&back, &r, sizeof(r)));
    // A date without its weekday, or a broken list of limits.
    sync_records_parse(&back, "extra_weekday=2\nconsole_lock_prev=1,2,3\n", 40);
    assert(back.extra_weekday == -1 && !back.console_lock_prev_ok);
}

// ---------------------------------------------------------------- orders

static SyncReason parse(const char *entity, const char *payload, SyncIntent *in)
{
    return sync_apply_parse(entity, payload, strlen(payload), in);
}

static void test_orders(void)
{
    SyncIntent in;
    assert(parse("limit_mon", "90", &in) == SyncReason_None && in.kind == SyncIntent_LimitDay && in.day == 1 &&
           in.minutes == 90 && in.mask == 2 && in.days[1] == 90);
    assert(parse("limit_sun", " 90.0 ", &in) == SyncReason_None && in.minutes == 90);   // HA's number entity
    assert(parse("limit_sun", "1440", &in) == SyncReason_None && in.minutes == 0xFFFF);  // no limit
    assert(parse("limit_sun", "0", &in) == SyncReason_None && in.minutes == 0);
    assert(parse("limit_sun", "1441", &in) == SyncReason_OutOfRange);
    assert(parse("limit_sun", "90.5", &in) == SyncReason_Invalid);
    assert(parse("limit_sun", "-5", &in) == SyncReason_Invalid);
    assert(parse("limit_sun", "", &in) == SyncReason_Invalid);
    assert(parse("limit_uniform", "120", &in) == SyncReason_None && in.mask == 0x7F && in.days[6] == 120);
    assert(parse("max_screentime_today", "45", &in) == SyncReason_None && in.kind == SyncIntent_LimitToday);
    assert(parse("limits_week", "60, 90,120,120,120,180,1440", &in) == SyncReason_None);
    assert(in.mask == 0x7F && in.days[0] == 60 && in.days[1] == 90 && in.days[6] == 0xFFFF);
    assert(parse("limits_week", "60,90,120", &in) == SyncReason_Invalid);
    assert(parse("limits_week", "60,90,120,120,120,180,1440,5", &in) == SyncReason_Invalid);
    assert(parse("limits_week", "60,90,120,120,120,180,9999", &in) == SyncReason_OutOfRange);
    assert(parse("add_bonus_time", "30", &in) == SyncReason_None && in.kind == SyncIntent_BonusTime && in.minutes == 30);
    assert(parse("add_bonus_time", "4", &in) == SyncReason_OutOfRange);
    assert(parse("add_bonus_time", "181", &in) == SyncReason_OutOfRange);
    assert(parse("add_bonus_time_15", "PRESS", &in) == SyncReason_None && in.minutes == 15);
    assert(parse("add_bonus_time_30", "PRESS", &in) == SyncReason_None && in.minutes == 30);
    assert(parse("add_bonus_time_60", "PRESS", &in) == SyncReason_None && in.minutes == 60);
    assert(parse("add_bonus_time_60", "60", &in) == SyncReason_Invalid);
    assert(parse("unlocked", "ON", &in) == SyncReason_None && in.kind == SyncIntent_Unlock);
    assert(parse("unlocked", "OFF", &in) == SyncReason_None && in.kind == SyncIntent_LockNow);
    assert(parse("bedtime_alarm", "21:30:00", &in) == SyncReason_None && in.hour == 21 && in.minute == 30);
    assert(parse("bedtime_alarm", "21:30", &in) == SyncReason_None);
    assert(parse("bedtime_alarm", "15:59", &in) == SyncReason_OutOfRange);
    assert(parse("bedtime_alarm", "24:00", &in) == SyncReason_OutOfRange);
    assert(parse("bedtime_alarm", "9:30", &in) == SyncReason_Invalid);
    assert(parse("bedtime_end_time", "06:15", &in) == SyncReason_None);
    assert(parse("bedtime_end_time", "09:01", &in) == SyncReason_OutOfRange);
    assert(parse("bedtime_end_time", "04:59", &in) == SyncReason_OutOfRange);
    assert(parse("restriction_level", "young_child", &in) == SyncReason_None && in.level == 2);
    assert(parse("restriction_level", "adult", &in) == SyncReason_Invalid);
    assert(parse("profile", "School week", &in) == SyncReason_None && !strcmp(in.profile, "School week"));
    assert(parse("profile", "0123456789012345678901234567890123", &in) == SyncReason_Invalid);
    assert(parse("discovery", "off", &in) == SyncReason_None && in.kind == SyncIntent_Discovery && !in.on);
    assert(parse("vr_restricted", "maybe", &in) == SyncReason_Invalid);
    assert(parse("no_such_thing", "1", &in) == SyncReason_UnknownEntity);
    assert(sync_apply_parse("limit_mon", "9\0" "0", 4, &in) == SyncReason_Invalid);   // a NUL inside

    // Nothing that deletes parental controls, unlinks the companion app or
    // touches the PIN is ever accepted, whatever it is called.
    const char *never[] = { "delete", "delete_parental_controls", "delete_settings", "unlink", "delete_pairing",
                            "pairing", "pin", "pin_code", "set_pin", "show_pin", "get_pin", "reset" };
    for (size_t i = 0; i < sizeof(never) / sizeof(never[0]); i++) {
        assert(parse(never[i], "PRESS", &in) == SyncReason_UnknownEntity);
        assert(parse(never[i], "ON", &in) == SyncReason_UnknownEntity);
    }
    assert(!strcmp(sync_reason_name(SyncReason_TimerWritesDisabled), "timer_writes_disabled"));
}

// The entity table and the order parser agree: every order HA can send
// parses, read-only entities take no order, the timer ones need
// remote_timer_writes, and every intent the parser has is an entity.
static void test_entities_match_orders(void)
{
    size_t n = 0;
    const SyncEntity *list = sync_entities(&n);
    assert(n > 30);
    for (size_t i = 0; i < n; i++) {
        const SyncEntity *e = &list[i];
        for (size_t k = 0; k < i; k++) assert(strcmp(list[k].id, e->id));   // unique ids
        assert(sync_entity_find(e->id) == e);
        SyncIntent in;
        const char *sample = !strcmp(e->platform, "number") ? "60"
                           : !strcmp(e->platform, "switch") ? "ON"
                           : !strcmp(e->platform, "button") ? "PRESS"
                           : !strcmp(e->platform, "time") ? (strstr(e->id, "end") ? "06:30:00" : "21:00:00")
                           : !strcmp(e->id, "restriction_level") ? "child"
                           : !strcmp(e->id, "profile") ? "School week"
                           : "1";
        const SyncReason r = parse(e->id, sample, &in);
        if (e->write == SyncWrite_None) {
            assert(r == SyncReason_UnknownEntity);
            continue;
        }
        assert(r == SyncReason_None);
        const bool timer = sync_intent_needs_timer_writes(in.kind);
        assert(timer == (e->write == SyncWrite_Timer));
        assert(e->value_template || !strcmp(e->platform, "button"));
    }
}

// ---------------------------------------------------------------- discovery

static size_t count(const char *hay, const char *needle)
{
    size_t c = 0;
    for (const char *p = strstr(hay, needle); p; p = strstr(p + 1, needle)) c++;
    return c;
}

static void test_discovery(void)
{
    SyncConf c;
    sync_conf_defaults(&c);
    strcpy(c.console_id, "a1b2c3d4");
    strcpy(c.console_name, "Salon \"Léa\"");
    char topic[128];
    assert(sync_discovery_topic(&c, topic, sizeof(topic)) > 0);
    assert(!strcmp(topic, "homeassistant/device/playguard_a1b2c3d4/config"));

    static char buf[16384];
    const char profiles[2][SYNC_PROFILE_MAX] = { "School week", "Holidays" };
    SyncDiscovery d = { &c, "1.1.0", false, profiles, 2 };

    // remote_timer_writes off: no limit, bedtime, lock or extra time entity.
    size_t n = sync_discovery_build(&d, buf, sizeof(buf));
    assert(n > 0 && n < sizeof(buf));
    assert(contains(buf, "\"ids\":[\"playguard_a1b2c3d4\"]"));
    assert(contains(buf, "\"name\":\"Salon \\\"Léa\\\"\""));
    assert(contains(buf, "\"avty_t\":\"playguard/a1b2c3d4/availability\""));
    assert(contains(buf, "\"used_screen_time\":{\"p\":\"sensor\""));
    assert(contains(buf, "\"cmd_t\":\"playguard/a1b2c3d4/vr_restricted/set\""));
    assert(contains(buf, "\"cmd_t\":\"playguard/a1b2c3d4/lock_now/set\""));
    assert(!contains(buf, "limit_mon"));
    assert(!contains(buf, "\"unlocked\""));
    assert(!contains(buf, "add_bonus_time"));
    assert(!contains(buf, "\"profile\""));
    assert(contains(buf, "\"stat_t\":\"playguard/a1b2c3d4/event\",\"evt_typ\":["));
    const size_t without = n;

    // On: the timer entities appear, retained and optimistic, with the profiles.
    c.remote_timer_writes = true;
    n = sync_discovery_build(&d, buf, sizeof(buf));
    assert(n > without);
    assert(contains(buf, "\"limit_mon\":{\"p\":\"number\""));
    assert(contains(buf, "\"cmd_t\":\"playguard/a1b2c3d4/limit_mon/set\",\"ret\":true,\"opt\":true"));
    assert(contains(buf, "\"ops\":[\"School week\",\"Holidays\"]"));
    assert(contains(buf, "\"bedtime_alarm\":{\"p\":\"time\""));
    // Every component has a unique id and the device's availability.
    size_t total = 0;
    const SyncEntity *list = sync_entities(&total);
    assert(count(buf, "\"uniq_id\":\"playguard_a1b2c3d4_") == total);
    (void)list;
    save("discovery.json", buf);
    printf("discovery: %zu bytes, %zu components\n", n, total);

    // Without profiles, no profile select (a select needs options).
    d.n_profiles = 0;
    n = sync_discovery_build(&d, buf, sizeof(buf));
    assert(!contains(buf, "\"profile\":{"));
    // Read-only: sensors only.
    d.read_only = true;
    n = sync_discovery_build(&d, buf, sizeof(buf));
    assert(n > 0 && !contains(buf, "cmd_t"));
    // Too small a buffer: 0.
    assert(sync_discovery_build(&d, buf, 512) == 0);
}

// ---------------------------------------------------------------- state

static void timer_state(PtState *pt)
{
    memset(pt, 0, sizeof(*pt));
    pt->session_valid = pt->fw_supported = pt->valid = true;
    const uint16_t days[7] = { 180, 120, 120, 120, 120, 120, 0xFFFF };
    memcpy(pt->day_min, days, sizeof(days));
    pt->enabled_valid = pt->enabled = true;
    pt->restricted_valid = true;
    pt->temporary_unlocked_valid = true;
    pt->remaining_valid = true;
    pt->remaining_ns = 45ULL * 60000000000ULL;
    pt->alarm_disabled_valid = true;
    pt->bedtime_valid = pt->bedtime_enabled = true;
    pt->bedtime_hour = 21;
    pt->bedtime_minute = 30;
    pt->bedtime_reset_valid = true;
    pt->bedtime_reset_hour = 6;
}

static void test_state(void)
{
    SyncConf c;
    sync_conf_defaults(&c);
    strcpy(c.console_id, "a1b2c3d4");
    strcpy(c.console_name, "Salon");
    PctlStatus st;
    memset(&st, 0, sizeof(st));
    st.restriction_enabled_ok = st.restriction_enabled = true;
    st.temp_unlocked_ok = true;
    st.pin_length_ok = true;
    st.pin_length = 4;
    st.safety_level_ok = true;
    st.safety_level = 3;
    st.settings_ok = true;
    st.settings.rating_age = 12;
    st.settings.sns_post_restriction = true;
    st.pairing_active_ok = true;
    PtState pt;
    timer_state(&pt);
    SysInfo sys;
    memset(&sys, 0, sizeof(sys));
    sys.hos_version = (23u << 16) | 1u;
    sys.ams_valid = true;
    sys.ams_major = 1;
    sys.ams_minor = 12;
    SyncRecords rec;
    sync_records_clear(&rec);
    rec.extra_weekday = 1;
    strcpy(rec.extra_date, "2026-10-05");
    rec.extra_base = 90;
    rec.extra_value = 120;

    SyncSnapshot s;
    memset(&s, 0, sizeof(s));
    s.source = "app";
    s.ts = 1791000000;
    s.local_date = "2026-10-05";
    s.weekday = 1;   // Monday
    s.clock_accurate_ok = s.clock_accurate = true;
    s.conf = &c;
    s.sys = &sys;
    s.app_version = "1.1.0";
    s.status = &st;
    s.timer = &pt;
    s.records = &rec;
    s.activity_ok = true;
    s.activity_s = 74 * 60;
    s.now_playing = 0x0100000000010000ULL;
    s.now_playing_name = "Super Mario Odyssey";
    s.now_playing_since = 1790990000;
    static char buf[4096];
    size_t n = sync_state_build(&s, buf, sizeof(buf));
    assert(n > 0);
    save("state.json", buf);
    assert(contains(buf, "\"source\":\"app\""));
    assert(contains(buf, "\"firmware\":\"23.0.1\",\"atmosphere\":\"1.12.0\""));
    assert(contains(buf, "\"level\":\"child\""));
    assert(contains(buf, "\"limits\":{\"sun\":180,\"mon\":120,\"tue\":120,\"wed\":120,\"thu\":120,\"fri\":120,\"sat\":1440}"));
    assert(contains(buf, "\"limits_min\":[180,120,120,120,120,120,1440]"));
    assert(contains(buf, "\"uniform_min\":null"));
    assert(contains(buf, "\"limit_today_min\":120"));
    assert(contains(buf, "\"remaining_min\":45"));
    assert(contains(buf, "\"used_min\":75"));
    assert(contains(buf, "\"alarm_on\":true"));
    assert(contains(buf, "\"bedtime\":{\"enabled\":true,\"start\":\"21:30:00\",\"end\":\"06:00:00\"}"));
    assert(contains(buf, "\"extended_today_min\":30"));
    assert(contains(buf, "\"now_playing\":{\"app_id\":\"0100000000010000\",\"name\":\"Super Mario Odyssey\""));
    assert(contains(buf, "\"activity_today\":{\"used_min\":74"));
    assert(contains(buf, "\"link\":{\"policy\":\"ask\""));

    // Nothing counted yet today (1454 reads 0): the whole limit is left, the
    // time played is unknown.
    pt.remaining_ns = 0;
    n = sync_state_build(&s, buf, sizeof(buf));
    assert(contains(buf, "\"remaining_min\":120") && contains(buf, "\"used_min\":null"));
    // Time is up.
    pt.restricted = true;
    n = sync_state_build(&s, buf, sizeof(buf));
    assert(contains(buf, "\"limit_reached\":true") && contains(buf, "\"remaining_min\":0") && contains(buf, "\"used_min\":120"));

    // Nothing could be read: every object is there, its leaves null.
    SyncSnapshot empty;
    memset(&empty, 0, sizeof(empty));
    empty.weekday = -1;
    empty.conf = &c;
    n = sync_state_build(&empty, buf, sizeof(buf));
    assert(n > 0);
    save("state_empty.json", buf);
    assert(contains(buf, "\"limits\":{\"sun\":null"));
    assert(contains(buf, "\"limits_min\":null"));
    assert(contains(buf, "\"bedtime\":{\"enabled\":null,\"start\":null,\"end\":null}"));
    assert(contains(buf, "\"now_playing\":{\"app_id\":null,\"name\":null,\"since\":null}"));
    assert(contains(buf, "\"enabled\":null"));
    assert(contains(buf, "\"weekday\":null"));
}

static void test_activity_names_events(void)
{
    static char buf[4096];
    const SyncAppTime apps[] = { { 0x0100000000010000ULL, 3600 }, { 0x01000A10041EA000ULL, 840 }, { 0x1, 0 } };
    const SyncAccountTime accts[] = { { { 0x0011223344556677ULL, 0x8899AABBCCDDEEFFULL }, 4440 } };
    SyncActivity a = { "agent", 1791000000, "2026-10-05", false, apps, 3, NULL, 0, 0x0100000000010000ULL, 1790990000 };
    size_t n = sync_activity_build(&a, buf, sizeof(buf));
    assert(n > 0);
    assert(contains(buf, "\"total_s\":4440,\"total_min\":74"));
    assert(contains(buf, "{\"app_id\":\"0100000000010000\",\"s\":3600,\"min\":60}"));
    assert(contains(buf, "{\"app_id\":\"01000A10041EA000\",\"s\":840,\"min\":14}"));
    assert(!contains(buf, "\"0000000000000001\""));   // nothing played: left out
    assert(contains(buf, "\"per_account\":null"));
    assert(contains(buf, "\"now_playing\":{\"app_id\":\"0100000000010000\""));
    save("activity.json", buf);
    a.final = true;
    a.accounts = accts;
    a.n_accounts = 1;
    n = sync_activity_build(&a, buf, sizeof(buf));
    assert(contains(buf, "\"final\":true") && !contains(buf, "now_playing"));
    assert(contains(buf, "\"uid\":\"00112233445566778899AABBCCDDEEFF\",\"s\":4440,\"min\":74"));

    const SyncAppName names[] = { { 0x0100000000010000ULL, "Super Mario Odyssey" }, { 0x2, "" } };
    const SyncAccountName nick[] = { { { 1, 2 }, "Léa" } };
    n = sync_names_build(1791000000, names, 2, nick, 1, buf, sizeof(buf));
    assert(n > 0 && contains(buf, "\"0100000000010000\":\"Super Mario Odyssey\"") &&
           contains(buf, "\"00000000000000010000000000000002\":\"Léa\"") && !contains(buf, "0000000000000002\":\"\""));

    n = sync_event_build("agent", 1791000000, "command_rejected", "limit_mon", "5000", SyncReason_OutOfRange, false, 0,
                         buf, sizeof(buf));
    assert(n > 0 && contains(buf, "\"event_type\":\"command_rejected\",\"entity\":\"limit_mon\",\"payload\":\"5000\","
                                  "\"reason\":\"out_of_range\",\"rc\":null"));
    n = sync_event_build("app", 1, "command_applied", "lock_now", "PRESS", SyncReason_None, true, 0x1234, buf, sizeof(buf));
    assert(contains(buf, "\"reason\":null,\"rc\":\"0x00001234\""));
}

int main(int argc, char **argv)
{
    if (argc > 1) out_dir = argv[1];
    test_json();
    test_connect_encoding();
    test_publish_roundtrip();
    test_remaining_length();
    test_other_packets();
    test_topic_matching();
    test_conf();
    test_records();
    test_orders();
    test_entities_match_orders();
    test_discovery();
    test_state();
    test_activity_names_events();
    puts("sync_core: all tests passed");
    return 0;
}
