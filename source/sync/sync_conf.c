// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "sync_conf.h"

#include <stdio.h>
#include <string.h>

static void copy(char *dst, size_t cap, const char *src)
{
    snprintf(dst, cap, "%s", src);
}

void sync_conf_defaults(SyncConf *c)
{
    memset(c, 0, sizeof(*c));
    c->schema = SYNC_CONF_SCHEMA;
    c->port = 1883;
    c->policy = SyncPolicy_Ask;
    c->publish_activity = true;
    c->ha_discovery = true;
    c->poll_s = 30;
    copy(c->topic_prefix, sizeof(c->topic_prefix), "playguard");
    copy(c->discovery_prefix, sizeof(c->discovery_prefix), "homeassistant");
    c->log_level = 1;
}

bool sync_parse_uint(const char *s, uint32_t min, uint32_t max, uint32_t *out)
{
    if (!s || !*s) return false;
    uint64_t v = 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return false;
        v = v * 10 + (uint64_t)(*p - '0');
        if (v > 0xFFFFFFFFull) return false;
    }
    if (v < min || v > max) return false;
    *out = (uint32_t)v;
    return true;
}

static bool equal_nocase(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return false;
    }
    return *a == *b;
}

bool sync_parse_bool(const char *s, bool *out)
{
    if (equal_nocase(s, "1") || equal_nocase(s, "true") || equal_nocase(s, "on") || equal_nocase(s, "yes")) {
        *out = true;
        return true;
    }
    if (equal_nocase(s, "0") || equal_nocase(s, "false") || equal_nocase(s, "off") || equal_nocase(s, "no")) {
        *out = false;
        return true;
    }
    return false;
}

bool sync_conf_id_valid(const char *id)
{
    if (!id || strlen(id) != SYNC_ID_LEN) return false;
    for (const char *p = id; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) return false;
    return true;
}

// A topic level we put our own names under: letters, digits, '_' and '-'.
static bool prefix_valid(const char *s)
{
    if (!*s) return false;
    for (const char *p = s; *p; p++) {
        const char ch = *p;
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
            return false;
    }
    return true;
}

const char *sync_policy_name(SyncPolicy p)
{
    switch (p) {
        case SyncPolicy_Auto: return "auto";
        case SyncPolicy_Off: return "off";
        default: return "ask";
    }
}

void sync_kv_each(const char *text, size_t len, SyncKvFn fn, void *ctx)
{
    size_t i = 0;
    while (i < len) {
        size_t end = i;
        while (end < len && text[end] != '\n') end++;
        size_t a = i, b = end;
        if (b > a && text[b - 1] == '\r') b--;
        // UTF-8 byte-order mark on the first line.
        if (i == 0 && b - a >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
            (unsigned char)text[2] == 0xBF)
            a += 3;
        while (a < b && (text[a] == ' ' || text[a] == '\t')) a++;
        if (a < b && text[a] != '#') {
            size_t eq = a;
            while (eq < b && text[eq] != '=') eq++;
            if (eq < b) {
                size_t ke = eq;
                while (ke > a && (text[ke - 1] == ' ' || text[ke - 1] == '\t')) ke--;
                char key[48], value[512];
                const size_t kn = ke - a, vn = b - eq - 1;
                if (kn > 0 && kn < sizeof(key) && vn < sizeof(value)) {
                    memcpy(key, text + a, kn);
                    key[kn] = '\0';
                    memcpy(value, text + eq + 1, vn);
                    value[vn] = '\0';
                    fn(ctx, key, value);
                }
            }
        }
        i = end + 1;
    }
}

typedef struct {
    SyncConf *c;
    int read;
} ParseCtx;

static void keep_extra(SyncConf *c, const char *key, const char *value)
{
    const size_t used = strlen(c->extra);
    const size_t need = strlen(key) + 1 + strlen(value) + 1;
    if (used + need + 1 > sizeof(c->extra)) return;   // too many: dropped
    snprintf(c->extra + used, sizeof(c->extra) - used, "%s=%s\n", key, value);
}

static void str_field(char *dst, size_t cap, const char *value)
{
    if (strlen(value) < cap) copy(dst, cap, value);
}

static void on_kv(void *vctx, const char *key, const char *value)
{
    ParseCtx *p = (ParseCtx *)vctx;
    SyncConf *c = p->c;
    uint32_t u = 0;
    bool b = false;
    p->read++;
    if (!strcmp(key, "schema")) {
        if (sync_parse_uint(value, 1, 1000, &u)) c->schema = (int)u;
    } else if (!strcmp(key, "enabled")) {
        if (sync_parse_bool(value, &b)) c->enabled = b;
    } else if (!strcmp(key, "host")) {
        str_field(c->host, sizeof(c->host), value);
    } else if (!strcmp(key, "port")) {
        if (sync_parse_uint(value, 1, 65535, &u)) c->port = (uint16_t)u;
    } else if (!strcmp(key, "tls")) {
        if (sync_parse_bool(value, &b)) c->tls = b;
    } else if (!strcmp(key, "ca_file")) {
        str_field(c->ca_file, sizeof(c->ca_file), value);
    } else if (!strcmp(key, "username")) {
        str_field(c->username, sizeof(c->username), value);
    } else if (!strcmp(key, "password")) {
        str_field(c->password, sizeof(c->password), value);
    } else if (!strcmp(key, "allow_anonymous")) {
        if (sync_parse_bool(value, &b)) c->allow_anonymous = b;
    } else if (!strcmp(key, "console_id")) {
        if (sync_conf_id_valid(value)) copy(c->console_id, sizeof(c->console_id), value);
    } else if (!strcmp(key, "console_name")) {
        str_field(c->console_name, sizeof(c->console_name), value);
    } else if (!strcmp(key, "policy")) {
        if (!strcmp(value, "ask")) c->policy = SyncPolicy_Ask;
        else if (!strcmp(value, "auto")) c->policy = SyncPolicy_Auto;
        else if (!strcmp(value, "off")) c->policy = SyncPolicy_Off;
    } else if (!strcmp(key, "remote_timer_writes")) {
        if (sync_parse_bool(value, &b)) c->remote_timer_writes = b;
    } else if (!strcmp(key, "publish_report")) {
        if (sync_parse_bool(value, &b)) c->publish_report = b;
    } else if (!strcmp(key, "publish_activity")) {
        if (sync_parse_bool(value, &b)) c->publish_activity = b;
    } else if (!strcmp(key, "ha_discovery")) {
        if (sync_parse_bool(value, &b)) c->ha_discovery = b;
    } else if (!strcmp(key, "poll_s")) {
        if (sync_parse_uint(value, 10, 300, &u)) c->poll_s = (uint16_t)u;
    } else if (!strcmp(key, "topic_prefix")) {
        if (strlen(value) < sizeof(c->topic_prefix) && prefix_valid(value)) copy(c->topic_prefix, sizeof(c->topic_prefix), value);
    } else if (!strcmp(key, "discovery_prefix")) {
        if (strlen(value) < sizeof(c->discovery_prefix) && prefix_valid(value))
            copy(c->discovery_prefix, sizeof(c->discovery_prefix), value);
    } else if (!strcmp(key, "log_level")) {
        if (sync_parse_uint(value, 0, 2, &u)) c->log_level = (int)u;
    } else {
        keep_extra(c, key, value);
    }
}

bool sync_conf_parse(SyncConf *c, const char *text, size_t len)
{
    ParseCtx p = { c, 0 };
    c->extra[0] = '\0';
    sync_kv_each(text, len, on_kv, &p);
    return p.read > 0 || len == 0;
}

size_t sync_conf_write(const SyncConf *c, char *out, size_t cap)
{
    const int n = snprintf(out, cap,
        "# PlayGuard remote link (MQTT / Home Assistant). Written by PlayGuard's\n"
        "# Sync screen; see docs/sync-protocol.md. Keep this file private: it\n"
        "# holds the broker password.\n"
        "schema=%d\n"
        "enabled=%d\n"
        "host=%s\n"
        "port=%u\n"
        "tls=%d\n"
        "ca_file=%s\n"
        "username=%s\n"
        "password=%s\n"
        "allow_anonymous=%d\n"
        "console_id=%s\n"
        "console_name=%s\n"
        "policy=%s\n"
        "remote_timer_writes=%d\n"
        "publish_report=%d\n"
        "publish_activity=%d\n"
        "ha_discovery=%d\n"
        "poll_s=%u\n"
        "topic_prefix=%s\n"
        "discovery_prefix=%s\n"
        "log_level=%d\n"
        "%s",
        SYNC_CONF_SCHEMA, c->enabled, c->host, (unsigned)c->port, c->tls, c->ca_file, c->username, c->password,
        c->allow_anonymous, c->console_id, c->console_name, sync_policy_name(c->policy), c->remote_timer_writes,
        c->publish_report, c->publish_activity, c->ha_discovery, (unsigned)c->poll_s, c->topic_prefix,
        c->discovery_prefix, c->log_level, c->extra);
    if (n < 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
}

const char *sync_conf_problem(const SyncConf *c)
{
    if (!c->host[0]) return "no broker host";
    if (!sync_conf_id_valid(c->console_id)) return "no console id";
    if (!c->username[0] && !c->allow_anonymous) return "no user name (anonymous brokers must be allowed explicitly)";
    // Line breaks would end up in the file: the next read would split them.
    const char *fields[] = { c->host, c->username, c->password, c->console_name, c->ca_file };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++)
        if (strchr(fields[i], '\n') || strchr(fields[i], '\r')) return "a setting holds a line break";
    return NULL;
}
