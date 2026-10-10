// sync_entities — the entities the console offers Home Assistant: the one
// table the native MQTT discovery is built from (sync_discovery.c), that the
// host tests check against the order parser (sync_apply.c), and that the HA
// integration carries a copy of (tests/sync_core writes it out as
// build/host-tests/sync/entities.json).
// Names follow HA's core nintendo_parental_controls integration where the
// meaning is the same.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SyncWrite_None = 0,   // read only
    SyncWrite_Free,       // an order, always offered (unless read-only)
    SyncWrite_Timer,      // an order that changes the play timer or unlocks: needs remote_timer_writes
} SyncWrite;

typedef struct {
    const char *id;               // object id, also the order's topic level
    const char *platform;         // HA platform
    SyncWrite   write;
    const char *name;             // English name shown in HA
    const char *value_template;   // over the state document; NULL: no state (button, event)
    const char *extra;            // more discovery options, a JSON fragment without braces ("" none)
} SyncEntity;

const SyncEntity *sync_entities(size_t *count);
const SyncEntity *sync_entity_find(const char *id);

#ifdef __cplusplus
}
#endif
