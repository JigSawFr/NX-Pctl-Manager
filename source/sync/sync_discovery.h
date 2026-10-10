// sync_discovery — Home Assistant's native MQTT discovery for one console:
// one retained, device-based payload on
// `<discovery_prefix>/device/playguard_<id>/config` (HA 2024.11 and later),
// built from the entity table (sync_entities.c). Orders that need
// remote_timer_writes are only announced while it is on, and none while
// PlayGuard is read-only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "sync_apply.h"
#include "sync_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const SyncConf *conf;
    const char     *sw_version;   // the device's software version
    bool            read_only;
    const char    (*profiles)[SYNC_PROFILE_MAX];   // the profile select's options (none: no select)
    size_t          n_profiles;
} SyncDiscovery;

size_t sync_discovery_topic(const SyncConf *conf, char *out, size_t cap);
size_t sync_discovery_build(const SyncDiscovery *d, char *out, size_t cap);

// `<topic_prefix>/<console_id>` (no trailing slash).
size_t sync_topic_base(const SyncConf *conf, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
