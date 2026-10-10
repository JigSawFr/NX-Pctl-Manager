// Minimal fake <switch.h> for the playstats_logic host tests: the integer
// types, Result helpers and the pdm play-event layout (field names and enum
// values as libnx has them; the unused members are left out).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int64_t s64;
typedef u32 Result;

#define R_SUCCEEDED(rc) ((rc) == 0)
#define R_FAILED(rc) ((rc) != 0)
#define MAKERESULT(module, description) ((((module) & 0x1FF)) | ((description) & 0x1FFF) << 9)
enum { Module_Libnx = 345 };
enum { LibnxError_OutOfMemory = 2 };

enum { AppletId_application = 0x01, AppletId_SystemAppletMenu = 0x03 };

typedef enum {
    PdmPlayEventType_Applet = 0,
    PdmPlayEventType_Account = 1,
    PdmPlayEventType_PowerStateChange = 2,
    PdmPlayEventType_OperationModeChange = 3,
    PdmPlayEventType_Initialize = 4,
} PdmPlayEventType;

typedef enum {
    PdmAppletEventType_Launch = 0,
    PdmAppletEventType_Exit = 1,
    PdmAppletEventType_InFocus = 2,
    PdmAppletEventType_OutOfFocus = 3,
    PdmAppletEventType_OutOfFocus4 = 4,
    PdmAppletEventType_Exit5 = 5,
    PdmAppletEventType_Exit6 = 6,
} PdmAppletEventType;

typedef enum {
    PdmPlayLogPolicy_All = 0,
    PdmPlayLogPolicy_LogOnly = 1,
    PdmPlayLogPolicy_None = 2,
} PdmPlayLogPolicy;

typedef struct {
    union {
        struct {
            u32 program_id[2];
            u32 unk_x8;
            u8 applet_id;
            u8 storage_id;
            u8 log_policy;
            u8 event_type;
            u8 unused[0xc];
        } applet;
        struct {
            u32 uid[4];
            u32 application_id[2];
            u8 type;
            u8 unused[0x7];
        } account;
        u8 data[0x1c];
    } event_data;
    u8 play_event_type;
    u8 pad[3];
    u64 timestamp_user;
    u64 timestamp_network;
    u64 timestamp_steady;
} PdmPlayEvent;
