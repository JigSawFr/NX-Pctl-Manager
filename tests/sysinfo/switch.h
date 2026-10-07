// Minimal fake <switch.h> for the sysinfo host tests.
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
typedef int SplConfigItem;
typedef enum { AppletType_Application = 0, AppletType_LibraryApplet = 2 } AppletType;

#define R_SUCCEEDED(rc) ((rc) == 0)
#define R_FAILED(rc) ((rc) != 0)
#define MAKEHOSVERSION(_major, _minor, _micro) (((u32)(_major) << 16) | ((u32)(_minor) << 8) | (u32)(_micro))
#define HOSVER_MAJOR(_v) (((_v) >> 16) & 0xFF)
#define HOSVER_MINOR(_v) (((_v) >> 8) & 0xFF)
#define HOSVER_MICRO(_v) ((_v) & 0xFF)

u32 hosversionGet(void);
bool hosversionIsAtmosphere(void);
Result splInitialize(void);
void splExit(void);
Result splGetConfig(SplConfigItem item, u64 *out);
AppletType appletGetAppletType(void);
