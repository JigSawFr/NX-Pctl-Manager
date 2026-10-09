// sync_conf — the remote link's settings (sd:/switch/playguard/sync.conf)
// and the small flat files PlayGuard and the agent exchange: "key=value",
// one per line, '#' starts a comment. Read by both processes with this one
// parser (the agent has no JSON library). PlayGuard's Sync screen writes
// sync.conf; it can also be edited by hand. A key this version does not know
// is kept and written back.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYNC_CONF_SCHEMA   1
#define SYNC_HOST_MAX      254
#define SYNC_USER_MAX      65
#define SYNC_PASS_MAX      129
#define SYNC_PATH_MAX      128
#define SYNC_ID_LEN        8      // console id: 8 lower-case hex digits
#define SYNC_NAME_MAX      33
#define SYNC_PREFIX_MAX    33
#define SYNC_EXTRA_MAX     1024   // unknown lines kept

typedef enum {
    SyncPolicy_Ask  = 0,   // PlayGuard open: confirm on the console; the agent alone: refuse
    SyncPolicy_Auto = 1,   // apply without asking (still behind every write guard)
    SyncPolicy_Off  = 2,   // publish only, refuse every order
} SyncPolicy;

typedef struct {
    int        schema;
    bool       enabled;
    char       host[SYNC_HOST_MAX];
    uint16_t   port;                    // 1883; 8883 turns tls on by default
    bool       tls;
    char       ca_file[SYNC_PATH_MAX];  // PEM, for a broker with a private certificate authority
    char       username[SYNC_USER_MAX];
    char       password[SYNC_PASS_MAX];
    bool       allow_anonymous;         // connect without a user name
    char       console_id[SYNC_ID_LEN + 1];
    char       console_name[SYNC_NAME_MAX];
    SyncPolicy policy;
    bool       remote_timer_writes;     // orders may change the play timer (unlocking with the stored PIN)
    bool       publish_report;
    bool       publish_activity;
    bool       ha_discovery;            // Home Assistant MQTT discovery
    uint16_t   poll_s;                  // state refresh, 10..300
    char       topic_prefix[SYNC_PREFIX_MAX];      // "playguard"
    char       discovery_prefix[SYNC_PREFIX_MAX];  // "homeassistant"
    int        log_level;               // 0 errors, 1 info, 2 debug
    char       extra[SYNC_EXTRA_MAX];   // lines with unknown keys, as read
} SyncConf;

void sync_conf_defaults(SyncConf *c);
// Reads `text` over the defaults (call sync_conf_defaults first). Values out
// of range keep their default. Returns false when nothing could be read at
// all (an empty file is fine and returns true).
bool sync_conf_parse(SyncConf *c, const char *text, size_t len);
// The file's content; the length written, 0 when `cap` is too small.
size_t sync_conf_write(const SyncConf *c, char *out, size_t cap);
// What is still missing before connecting: NULL when ready, else an English
// reason ("no broker host" …). The UI shows its own words.
const char *sync_conf_problem(const SyncConf *c);
// 8 lower-case hex digits.
bool sync_conf_id_valid(const char *id);
// "ask" / "auto" / "off".
const char *sync_policy_name(SyncPolicy p);

// The generic reader the other flat files use: calls `fn` for each
// "key=value" line (key trimmed, value as written up to the line end, a
// trailing '\r' dropped).
typedef void (*SyncKvFn)(void *ctx, const char *key, const char *value);
void sync_kv_each(const char *text, size_t len, SyncKvFn fn, void *ctx);

// Parses an unsigned integer with bounds; false when it is not one.
bool sync_parse_uint(const char *s, uint32_t min, uint32_t max, uint32_t *out);
// "1" / "true" / "on" / "yes" and "0" / "false" / "off" / "no".
bool sync_parse_bool(const char *s, bool *out);

#ifdef __cplusplus
}
#endif
