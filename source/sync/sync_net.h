// sync_net — the plain TCP stream under the MQTT client (POSIX sockets: libnx
// on the Switch, the host's on desktop), and a monotonic clock. The socket
// service itself must already be up (borealis starts it in the app; the
// agent sysmodule calls socketInitialize).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int fd;
} SyncTcp;

// A SyncIo over `tcp` (which must outlive it).
SyncIo sync_tcp_io(SyncTcp *tcp);

// Milliseconds from an arbitrary start, never going back.
uint64_t sync_now_ms(void);

#ifdef __cplusplus
}
#endif
