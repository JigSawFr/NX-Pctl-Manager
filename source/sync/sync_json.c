// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "sync_json.h"

#include <stdio.h>
#include <string.h>

void sync_json_init(SyncJson *j, char *buf, size_t cap)
{
    memset(j, 0, sizeof(*j));
    j->buf = buf;
    j->cap = cap;
    j->first[0] = true;
    if (cap > 0) buf[0] = '\0';
    else j->overflow = true;
}

static void put(SyncJson *j, const char *s, size_t n)
{
    if (j->overflow) return;
    // Keep one byte for the terminating NUL.
    if (n >= j->cap - j->len) {
        j->overflow = true;
        return;
    }
    memcpy(j->buf + j->len, s, n);
    j->len += n;
    j->buf[j->len] = '\0';
}

static void putc_(SyncJson *j, char c)
{
    put(j, &c, 1);
}

// Before a value or a key: the comma that separates it from the previous one.
static void separate(SyncJson *j)
{
    if (j->after_key) {
        j->after_key = false;
        return;
    }
    if (j->depth < 0 || j->depth >= SYNC_JSON_DEPTH) {
        j->overflow = true;
        return;
    }
    if (!j->first[j->depth]) putc_(j, ',');
    j->first[j->depth] = false;
}

static void open_(SyncJson *j, char c)
{
    separate(j);
    putc_(j, c);
    if (j->depth + 1 >= SYNC_JSON_DEPTH) {
        j->overflow = true;
        return;
    }
    j->depth++;
    j->first[j->depth] = true;
}

static void close_(SyncJson *j, char c)
{
    if (j->depth <= 0) {
        j->overflow = true;
        return;
    }
    j->depth--;
    putc_(j, c);
}

size_t sync_json_end(SyncJson *j)
{
    if (j->overflow || j->depth != 0 || j->after_key) return 0;
    return j->len;
}

void sync_json_obj(SyncJson *j) { open_(j, '{'); }
void sync_json_obj_end(SyncJson *j) { close_(j, '}'); }
void sync_json_arr(SyncJson *j) { open_(j, '['); }
void sync_json_arr_end(SyncJson *j) { close_(j, ']'); }

static void escaped(SyncJson *j, const char *s, size_t n)
{
    putc_(j, '"');
    for (size_t i = 0; i < n; i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c == '"') put(j, "\\\"", 2);
        else if (c == '\\') put(j, "\\\\", 2);
        else if (c == '\n') put(j, "\\n", 2);
        else if (c == '\r') put(j, "\\r", 2);
        else if (c == '\t') put(j, "\\t", 2);
        else if (c < 0x20) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "\\u%04x", c);
            put(j, tmp, 6);
        } else putc_(j, (char)c);
    }
    putc_(j, '"');
}

void sync_json_key(SyncJson *j, const char *key)
{
    separate(j);
    escaped(j, key, strlen(key));
    putc_(j, ':');
    j->after_key = true;
}

void sync_json_strn(SyncJson *j, const char *s, size_t n)
{
    separate(j);
    escaped(j, s, n);
}

void sync_json_str(SyncJson *j, const char *s)
{
    sync_json_strn(j, s, strlen(s));
}

void sync_json_int(SyncJson *j, int64_t v)
{
    char tmp[24];
    const int n = snprintf(tmp, sizeof(tmp), "%lld", (long long)v);
    separate(j);
    put(j, tmp, (size_t)n);
}

void sync_json_uint(SyncJson *j, uint64_t v)
{
    char tmp[24];
    const int n = snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)v);
    separate(j);
    put(j, tmp, (size_t)n);
}

void sync_json_bool(SyncJson *j, bool v)
{
    separate(j);
    if (v) put(j, "true", 4);
    else put(j, "false", 5);
}

void sync_json_null(SyncJson *j)
{
    separate(j);
    put(j, "null", 4);
}

void sync_json_raw(SyncJson *j, const char *json)
{
    separate(j);
    put(j, json, strlen(json));
}

void sync_json_members(SyncJson *j, const char *members)
{
    if (!*members) return;
    separate(j);
    put(j, members, strlen(members));
}

void sync_json_hex64(SyncJson *j, uint64_t v)
{
    char tmp[17];
    snprintf(tmp, sizeof(tmp), "%016llX", (unsigned long long)v);
    sync_json_strn(j, tmp, 16);
}

void sync_json_kstr(SyncJson *j, const char *key, const char *s)
{
    sync_json_key(j, key);
    sync_json_str(j, s);
}

void sync_json_kint(SyncJson *j, const char *key, int64_t v)
{
    sync_json_key(j, key);
    sync_json_int(j, v);
}

void sync_json_kbool(SyncJson *j, const char *key, bool v)
{
    sync_json_key(j, key);
    sync_json_bool(j, v);
}

void sync_json_knull(SyncJson *j, const char *key)
{
    sync_json_key(j, key);
    sync_json_null(j);
}

void sync_json_kint_or_null(SyncJson *j, const char *key, bool ok, int64_t v)
{
    sync_json_key(j, key);
    if (ok) sync_json_int(j, v);
    else sync_json_null(j);
}

void sync_json_kbool_or_null(SyncJson *j, const char *key, bool ok, bool v)
{
    sync_json_key(j, key);
    if (ok) sync_json_bool(j, v);
    else sync_json_null(j);
}

void sync_json_kstr_or_null(SyncJson *j, const char *key, const char *s)
{
    sync_json_key(j, key);
    if (s && *s) sync_json_str(j, s);
    else sync_json_null(j);
}
