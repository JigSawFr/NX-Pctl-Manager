// Minimal fake <switch.h> for the pctl_ops host tests. It models service
// ownership, firmware version and command payloads — not Horizon IPC encoding.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t s64;
typedef u32 Result;
typedef struct { unsigned unused; } Service;

#define R_SUCCEEDED(rc) ((rc) == 0)
#define R_FAILED(rc) ((rc) != 0)
#define MAKEHOSVERSION(_major, _minor, _micro) (((u32)(_major) << 16) | ((u32)(_minor) << 8) | (u32)(_micro))
#define HOSVER_MAJOR(_v) (((_v) >> 16) & 0xFF)
#define HOSVER_MINOR(_v) (((_v) >> 8) & 0xFF)
#define HOSVER_MICRO(_v) ((_v) & 0xFF)

bool hosversionAtLeast(u8 major, u8 minor, u8 micro);
Result pctlInitialize(void);
void pctlExit(void);
Service *pctlGetServiceSession_Service(void);
Result pctlauthRegisterPasscode(void);
Result mock_dispatch(Service *srv, u32 command, void *out, size_t out_size,
                     const void *in, size_t in_size);

/* Buffer descriptors (the "..." arguments) are dropped: the fake checks
 * payload sizes and ownership only. */
#define serviceDispatch(srv, command, ...) \
    mock_dispatch((srv), (command), NULL, 0, NULL, 0)
#define serviceDispatchOut(srv, command, value, ...) \
    mock_dispatch((srv), (command), &(value), sizeof(value), NULL, 0)
#define serviceDispatchIn(srv, command, value, ...) \
    mock_dispatch((srv), (command), NULL, 0, &(value), sizeof(value))
#define serviceDispatchInOut(srv, command, in_value, out_value, ...) \
    mock_dispatch((srv), (command), &(out_value), sizeof(out_value), &(in_value), sizeof(in_value))
