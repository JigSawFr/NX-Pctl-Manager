// sync_json — a JSON writer into a fixed buffer, for the remote link's
// payloads (state, activity, discovery, events). No allocation: the agent
// sysmodule links it with a static heap. Once the buffer is full every call
// is a no-op and sync_json_end() returns 0, so a caller checks once at the
// end. Keys and values are escaped; commas are placed automatically.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYNC_JSON_DEPTH 12

typedef struct {
    char  *buf;
    size_t cap, len;
    bool   overflow;
    int    depth;
    bool   first[SYNC_JSON_DEPTH];   // no member written yet at that depth
    bool   after_key;                // a key was just written: no comma before the value
} SyncJson;

void sync_json_init(SyncJson *j, char *buf, size_t cap);
// Length written (NUL-terminated), or 0 when it did not fit or is unbalanced.
size_t sync_json_end(SyncJson *j);

void sync_json_obj(SyncJson *j);       // {
void sync_json_obj_end(SyncJson *j);   // }
void sync_json_arr(SyncJson *j);       // [
void sync_json_arr_end(SyncJson *j);   // ]
void sync_json_key(SyncJson *j, const char *key);

void sync_json_str(SyncJson *j, const char *s);
void sync_json_strn(SyncJson *j, const char *s, size_t n);
void sync_json_int(SyncJson *j, int64_t v);
void sync_json_uint(SyncJson *j, uint64_t v);
void sync_json_bool(SyncJson *j, bool v);
void sync_json_null(SyncJson *j);
// Pre-formatted JSON (a number, an object built elsewhere), written as is.
void sync_json_raw(SyncJson *j, const char *json);
// Members already written as JSON ("\"a\":1,\"b\":true"), added to the
// object being written.
void sync_json_members(SyncJson *j, const char *members);
// 16 hex digits, upper case, as a string ("0100000000010000").
void sync_json_hex64(SyncJson *j, uint64_t v);

// key + value in one call.
void sync_json_kstr(SyncJson *j, const char *key, const char *s);
void sync_json_kint(SyncJson *j, const char *key, int64_t v);
void sync_json_kbool(SyncJson *j, const char *key, bool v);
void sync_json_knull(SyncJson *j, const char *key);
// The value when `ok`, else null.
void sync_json_kint_or_null(SyncJson *j, const char *key, bool ok, int64_t v);
void sync_json_kbool_or_null(SyncJson *j, const char *key, bool ok, bool v);
void sync_json_kstr_or_null(SyncJson *j, const char *key, const char *s);   // NULL or "" -> null

#ifdef __cplusplus
}
#endif
